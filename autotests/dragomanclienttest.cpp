/*
    SPDX-FileCopyrightText: 2026 Blagovest Petrov <blagovest@petrovs.info>
    SPDX-FileCopyrightText: 2026 Vute Tech Ltd. <https://vute.tech>
    SPDX-License-Identifier: GPL-3.0-or-later
*/

/*
 * DragomanClient against a fake daemon on a private bus. The test starts
 * its own dbus-daemon, with no service directories so that nothing can be
 * activated, points the session bus at it, and registers the fake
 * Translator1 service on a second connection, so every message really
 * travels through the bus. The fake emits Response before it even returns
 * the method reply, which only works when the client subscribed first.
 */

#include "dragomanclient.h"

#include <QDBusConnection>
#include <QDBusContext>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QFile>
#include <QProcess>
#include <QSet>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include <optional>

using namespace Qt::StringLiterals;

namespace
{
constexpr auto serviceName = "dev.l10n_bg.dragomand.Translator1";
constexpr auto objectPath = "/dev/l10n_bg/dragomand/Translator1";
constexpr auto requestInterface = "dev.l10n_bg.dragomand.Request1";
}

class FakeTranslator : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "dev.l10n_bg.dragomand.Translator1")
public:
    explicit FakeTranslator(const QDBusConnection &connection)
        : m_connection(connection)
    {
    }

    QSet<QString> installed{u"bg-en"_s};
    QString failWith; // when set, Translate answers with this error
    int prepareCalls = 0;

public Q_SLOTS:
    Q_SCRIPTABLE QDBusObjectPath Translate(const QString &source, const QString &target, const QStringList &segments, const QVariantMap &options)
    {
        if (!installed.contains(source + u'-' + target)) {
            sendErrorReply(u"dev.l10n_bg.dragomand.Error.NotInstalled"_s, u"%1-%2 is not installed"_s.arg(source, target));
            return {};
        }
        const QString path = pathFor(options);
        if (!failWith.isEmpty()) {
            respond(path, 2, {{u"error"_s, failWith}});
        } else {
            QStringList translations;
            for (const QString &segment : segments) {
                translations.append(segment.toUpper());
            }
            respond(path, 0, {{u"translations"_s, translations}});
        }
        return QDBusObjectPath(path);
    }

    Q_SCRIPTABLE QDBusObjectPath PreparePair(const QString &source, const QString &target, const QVariantMap &options)
    {
        ++prepareCalls;
        const QString path = pathFor(options);
        QDBusMessage progress = QDBusMessage::createSignal(path, QString::fromLatin1(requestInterface), u"Progress"_s);
        progress << 1.5 << u"download"_s; // out of range on purpose: the client clamps
        m_connection.send(progress);
        installed.insert(source + u'-' + target);
        respond(path, 0, {});
        return QDBusObjectPath(path);
    }

    Q_SCRIPTABLE QList<QVariantMap> ListLanguagePairs()
    {
        return {
            {{u"source"_s, u"bg"_s}, {u"target"_s, u"en"_s}},
            {{u"source"_s, u"en"_s}, {u"target"_s, u"bg"_s}},
        };
    }

private:
    QString pathFor(const QVariantMap &options) const
    {
        return DragomanClient::requestPath(message().service(), options.value(u"handle_token"_s).toString());
    }

    // Sent before the method returns: the client must already listen.
    void respond(const QString &path, uint code, const QVariantMap &results)
    {
        QDBusMessage response = QDBusMessage::createSignal(path, QString::fromLatin1(requestInterface), u"Response"_s);
        response << code << results;
        m_connection.send(response);
    }

    QDBusConnection m_connection;
};

class DragomanClientTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase()
    {
        const QString dbusDaemon = QStandardPaths::findExecutable(u"dbus-daemon"_s);
        if (dbusDaemon.isEmpty()) {
            QSKIP("dbus-daemon is not installed");
        }
        QVERIFY(m_dir.isValid());
        const QString config = m_dir.filePath(u"bus.conf"_s);
        QFile file(config);
        QVERIFY(file.open(QIODevice::WriteOnly));
        // Ordinary literals: moc's preprocessor misreads "//" inside raw strings.
        file.write(
            "<busconfig>\n"
            "  <type>session</type>\n"
            "  <listen>unix:tmpdir=/tmp</listen>\n"
            "  <policy context=\"default\">\n"
            "    <allow send_destination=\"*\" eavesdrop=\"true\"/>\n"
            "    <allow eavesdrop=\"true\"/>\n"
            "    <allow own=\"*\"/>\n"
            "  </policy>\n"
            "</busconfig>\n");
        file.close();

        m_bus.start(dbusDaemon, {u"--config-file"_s, config, u"--nofork"_s, u"--print-address=1"_s});
        QVERIFY(m_bus.waitForStarted());
        QVERIFY(m_bus.waitForReadyRead(10000));
        const QByteArray address = m_bus.readLine().trimmed();
        QVERIFY(!address.isEmpty());
        // Must happen before the first use of the session bus in this process.
        qputenv("DBUS_SESSION_BUS_ADDRESS", address);
        QVERIFY(QDBusConnection::sessionBus().isConnected());

        qDBusRegisterMetaType<QList<QVariantMap>>();
        m_fakeConnection.emplace(QDBusConnection::connectToBus(QString::fromUtf8(address), u"fake-dragomand"_s));
        QVERIFY(m_fakeConnection->isConnected());
        m_fake = new FakeTranslator(*m_fakeConnection);
        QVERIFY(m_fakeConnection->registerObject(QString::fromLatin1(objectPath), m_fake, QDBusConnection::ExportScriptableSlots));
        QVERIFY(m_fakeConnection->registerService(QString::fromLatin1(serviceName)));
    }

    void cleanupTestCase()
    {
        if (m_fakeConnection) {
            QDBusConnection::disconnectFromBus(u"fake-dragomand"_s);
        }
        delete m_fake;
        m_bus.kill();
        m_bus.waitForFinished();
    }

    void translatesInstalledPair()
    {
        DragomanClient client;
        std::optional<DragomanReply> reply;
        int calls = 0;
        client.translate(u"bg"_s, u"en"_s, {u"добро"_s, u"утро"_s}, [&](const DragomanReply &r) {
            reply = r;
            ++calls;
        });
        QTRY_VERIFY(reply.has_value());
        QVERIFY2(reply->ok, qPrintable(reply->error));
        QCOMPARE(reply->translations, QStringList({u"ДОБРО"_s, u"УТРО"_s}));
        QTest::qWait(200);
        QCOMPARE(calls, 1); // exactly once, even after the method reply arrives
    }

    void installsMissingPairOnDemand()
    {
        DragomanClient client;
        QSignalSpy progress(&client, &DragomanClient::progress);
        std::optional<DragomanReply> reply;
        const int prepareBefore = m_fake->prepareCalls;
        client.translate(u"de"_s, u"en"_s, {u"guten morgen"_s}, [&](const DragomanReply &r) {
            reply = r;
        });
        QTRY_VERIFY(reply.has_value());
        QVERIFY2(reply->ok, qPrintable(reply->error));
        QCOMPARE(reply->translations, QStringList({u"GUTEN MORGEN"_s}));
        QCOMPARE(m_fake->prepareCalls, prepareBefore + 1);
        QCOMPARE(progress.count(), 1);
        QCOMPARE(progress.at(0).at(0).toDouble(), 1.0); // clamped
        QCOMPARE(progress.at(0).at(1).toString(), u"download"_s);
    }

    void reportsErrorResponses()
    {
        m_fake->failWith = u"the engine failed"_s;
        DragomanClient client;
        std::optional<DragomanReply> reply;
        client.translate(u"bg"_s, u"en"_s, {u"текст"_s}, [&](const DragomanReply &r) {
            reply = r;
        });
        QTRY_VERIFY(reply.has_value());
        m_fake->failWith.clear();
        QVERIFY(!reply->ok);
        QCOMPARE(reply->error, u"the engine failed"_s);
    }

    void listsPairs()
    {
        DragomanClient client;
        std::optional<DragomanClient::Pairs> pairs;
        QString error;
        client.listPairs([&](const DragomanClient::Pairs &p, const QString &e) {
            pairs = p;
            error = e;
        });
        QTRY_VERIFY(pairs.has_value());
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(pairs->size(), 2);
        QVERIFY(pairs->contains(Dragoman::LanguagePair(u"bg"_s, u"en"_s)));
        QVERIFY(pairs->contains(Dragoman::LanguagePair(u"en"_s, u"bg"_s)));
    }

    void failsCleanlyWithoutDaemon()
    {
        QVERIFY(m_fakeConnection->unregisterService(QString::fromLatin1(serviceName)));
        DragomanClient client;
        std::optional<DragomanReply> reply;
        client.translate(u"bg"_s, u"en"_s, {u"текст"_s}, [&](const DragomanReply &r) {
            reply = r;
        });
        QTRY_VERIFY(reply.has_value());
        QVERIFY(m_fakeConnection->registerService(QString::fromLatin1(serviceName)));
        QVERIFY(!reply->ok);
        QVERIFY(!reply->error.isEmpty());
    }

private:
    QTemporaryDir m_dir;
    QProcess m_bus;
    std::optional<QDBusConnection> m_fakeConnection;
    FakeTranslator *m_fake = nullptr;
};

QTEST_GUILESS_MAIN(DragomanClientTest)

#include "dragomanclienttest.moc"
