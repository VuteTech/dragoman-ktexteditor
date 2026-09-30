/*
    SPDX-FileCopyrightText: 2026 Blagovest Petrov <blagovest@petrovs.info>
    SPDX-FileCopyrightText: 2026 Vute Tech Ltd. <https://vute.tech>
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "dragomanclient.h"
#include "dragoman_debug.h"

#include <KLocalizedString>

#include <QAtomicInteger>
#include <QCoreApplication>
#include <QDBusAbstractInterface>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QTimer>
#include <QVariantMap>

#include <algorithm>
#include <utility>

using namespace Qt::StringLiterals;

namespace
{

constexpr auto serviceName = "dev.l10n_bg.dragomand.Translator1";
constexpr auto objectPath = "/dev/l10n_bg/dragomand/Translator1";
constexpr auto translatorInterface = "dev.l10n_bg.dragomand.Translator1";
constexpr auto requestInterface = "dev.l10n_bg.dragomand.Request1";
constexpr auto errorNotInstalled = "dev.l10n_bg.dragomand.Error.NotInstalled";

// Response codes of the request pattern.
constexpr uint responseSuccess = 0;
constexpr uint responseCancelled = 1;
constexpr uint responseError = 2;

// The daemon replies fast once a model is warm, but a cold load of a base
// model takes seconds, and a first use downloads tens of megabytes.
constexpr int translateTimeoutMs = 120 * 1000;
constexpr int prepareTimeoutMs = 600 * 1000;
constexpr int listTimeoutMs = 25 * 1000;

QDBusConnection bus()
{
    return QDBusConnection::sessionBus();
}

QString nextToken()
{
    static QAtomicInteger<quint64> counter;
    return u"ktexteditor_%1_%2"_s.arg(QCoreApplication::applicationPid()).arg(counter.fetchAndAddRelaxed(1));
}

QStringList toStringList(const QVariant &value)
{
    // Nested arrays inside a{sv} arrive as QDBusArgument, not QStringList.
    if (value.canConvert<QDBusArgument>()) {
        return qdbus_cast<QStringList>(value.value<QDBusArgument>());
    }
    return value.toStringList();
}

DragomanReply failure(const QString &message)
{
    DragomanReply reply;
    reply.error = message;
    return reply;
}

QString errorFrom(const QVariantMap &results, const QString &fallback)
{
    const QString message = results.value(u"error"_s).toString();
    return message.isEmpty() ? fallback : message;
}

QString errorFrom(const QDBusError &error)
{
    return error.message().isEmpty() ? error.name() : error.message();
}

/**
 * The Request1 interface of one request object. QtDBus relays the object's
 * D-Bus signals to the Qt signals of the same name and signature, which
 * connect like any other Qt signal. Connecting to one adds the bus match
 * rule, before the method call that creates the request is sent.
 */
class RequestProxy : public QDBusAbstractInterface
{
    Q_OBJECT
public:
    RequestProxy(const QString &path, QObject *parent)
        : QDBusAbstractInterface(QString::fromLatin1(serviceName), path, requestInterface, bus(), parent)
    {
    }

Q_SIGNALS:
    void Response(uint code, const QVariantMap &results);
    void Progress(double fraction, const QString &stage);
};

} // namespace

/**
 * One in-flight request: owns the signal subscription and a give-up timer,
 * and guarantees that finished() fires exactly once.
 */
class DragomanRequest : public QObject
{
    Q_OBJECT
public:
    explicit DragomanRequest(QObject *parent)
        : QObject(parent)
        , token(nextToken())
        , m_proxy(DragomanClient::requestPath(bus().baseService(), token), this)
    {
        connect(&m_proxy, &RequestProxy::Response, this, &DragomanRequest::finish);
        connect(&m_proxy, &RequestProxy::Progress, this, [this](double fraction, const QString &stage) {
            // The daemon is another process: clamp before anything renders it.
            Q_EMIT progress(std::clamp(fraction, 0.0, 1.0), stage);
        });
    }

    /// Whether the session bus is usable for the subscription.
    [[nodiscard]] bool isSubscribed() const
    {
        return m_proxy.connection().isConnected();
    }

    void arm(int timeoutMs)
    {
        QTimer::singleShot(timeoutMs, this, [this] {
            finish(responseError, {{u"error"_s, i18n("No answer from the translation daemon.")}});
        });
    }

    const QString token;

Q_SIGNALS:
    void finished(uint code, const QVariantMap &results);
    void progress(double fraction, const QString &stage);

private:
    void finish(uint code, const QVariantMap &results)
    {
        if (!std::exchange(m_done, true)) {
            Q_EMIT finished(code, results);
        }
    }

    RequestProxy m_proxy;
    bool m_done = false;
};

DragomanClient::DragomanClient(QObject *parent)
    : QObject(parent)
{
}

QString DragomanClient::requestPath(QStringView uniqueName, QStringView token)
{
    QString escaped = uniqueName.toString();
    std::ranges::replace_if(
        escaped,
        [](QChar c) {
            const char16_t u = c.unicode();
            return !((u >= u'0' && u <= u'9') || (u >= u'a' && u <= u'z') || (u >= u'A' && u <= u'Z'));
        },
        u'_');
    return u"/dev/l10n_bg/dragomand/request/"_s + escaped + u'/' + token;
}

void DragomanClient::translate(const QString &source, const QString &target, const QStringList &segments, Callback callback)
{
    startTranslate(source, target, segments, std::move(callback), true);
}

void DragomanClient::startTranslate(const QString &source, const QString &target, const QStringList &segments, Callback callback, bool installOnDemand)
{
    auto *request = new DragomanRequest(this);
    if (!request->isSubscribed()) {
        delete request;
        qCWarning(DRAGOMAN_LOG) << "no session bus connection";
        callback(failure(i18n("Cannot connect to the session bus.")));
        return;
    }
    connect(request, &DragomanRequest::finished, this, [request, callback](uint code, const QVariantMap &results) {
        request->deleteLater();
        DragomanReply reply;
        switch (code) {
        case responseSuccess:
            reply.ok = true;
            reply.translations = toStringList(results.value(u"translations"_s));
            reply.pivot = results.value(u"pivot"_s).toString();
            break;
        case responseCancelled:
            reply.error = i18n("The translation was cancelled.");
            break;
        default:
            reply.error = errorFrom(results, i18n("The translation failed."));
            qCWarning(DRAGOMAN_LOG) << "Translate failed:" << reply.error;
            break;
        }
        callback(reply);
    });
    request->arm(translateTimeoutMs);

    QDBusMessage call = QDBusMessage::createMethodCall(QString::fromLatin1(serviceName),
                                                       QString::fromLatin1(objectPath),
                                                       QString::fromLatin1(translatorInterface),
                                                       u"Translate"_s);
    call.setArguments({source,
                       target,
                       segments,
                       QVariant::fromValue(QVariantMap{
                           {u"handle_token"_s, request->token},
                           {u"priority"_s, u"interactive"_s},
                       })});
    auto *watcher = new QDBusPendingCallWatcher(bus().asyncCall(call), this);
    connect(watcher,
            &QDBusPendingCallWatcher::finished,
            this,
            [this, request, source, target, segments, callback, installOnDemand](QDBusPendingCallWatcher *watcher) {
                watcher->deleteLater();
                const QDBusPendingReply<QDBusObjectPath> reply = *watcher;
                if (!reply.isError()) {
                    return; // the Response signal carries the outcome
                }
                delete request; // the method failed, so no Response will come
                const QDBusError error = reply.error();
                if (installOnDemand && error.name() == QLatin1StringView(errorNotInstalled)) {
                    preparePair(source, target, [this, source, target, segments, callback](const DragomanReply &prepared) {
                        if (!prepared.ok) {
                            callback(prepared);
                        } else {
                            startTranslate(source, target, segments, callback, false);
                        }
                    });
                    return;
                }
                qCWarning(DRAGOMAN_LOG) << "Translate call failed:" << error.name() << error.message();
                callback(failure(errorFrom(error)));
            });
}

void DragomanClient::preparePair(const QString &source, const QString &target, Callback callback)
{
    auto *request = new DragomanRequest(this);
    if (!request->isSubscribed()) {
        delete request;
        qCWarning(DRAGOMAN_LOG) << "no session bus connection";
        callback(failure(i18n("Cannot connect to the session bus.")));
        return;
    }
    connect(request, &DragomanRequest::progress, this, &DragomanClient::progress);
    connect(request, &DragomanRequest::finished, this, [request, callback](uint code, const QVariantMap &results) {
        request->deleteLater();
        if (code == responseSuccess) {
            DragomanReply prepared;
            prepared.ok = true;
            callback(prepared);
        } else {
            const QString error = errorFrom(results, i18n("Installing the language pair failed."));
            qCWarning(DRAGOMAN_LOG) << "PreparePair failed:" << error;
            callback(failure(error));
        }
    });
    request->arm(prepareTimeoutMs);

    QDBusMessage call = QDBusMessage::createMethodCall(QString::fromLatin1(serviceName),
                                                       QString::fromLatin1(objectPath),
                                                       QString::fromLatin1(translatorInterface),
                                                       u"PreparePair"_s);
    call.setArguments({source,
                       target,
                       QVariant::fromValue(QVariantMap{
                           {u"handle_token"_s, request->token},
                       })});
    auto *watcher = new QDBusPendingCallWatcher(bus().asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [request, callback](QDBusPendingCallWatcher *watcher) {
        watcher->deleteLater();
        const QDBusPendingReply<QDBusObjectPath> reply = *watcher;
        if (reply.isError()) {
            delete request;
            qCWarning(DRAGOMAN_LOG) << "PreparePair call failed:" << reply.error().name() << reply.error().message();
            callback(failure(errorFrom(reply.error())));
        }
    });
}

void DragomanClient::listPairs(PairsCallback callback)
{
    const QDBusMessage call = QDBusMessage::createMethodCall(QString::fromLatin1(serviceName),
                                                             QString::fromLatin1(objectPath),
                                                             QString::fromLatin1(translatorInterface),
                                                             u"ListLanguagePairs"_s);
    auto *watcher = new QDBusPendingCallWatcher(bus().asyncCall(call, listTimeoutMs), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [callback](QDBusPendingCallWatcher *watcher) {
        watcher->deleteLater();
        const QDBusMessage reply = watcher->reply();
        if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
            qCWarning(DRAGOMAN_LOG) << "ListLanguagePairs failed:" << reply.errorName() << reply.errorMessage();
            callback({}, reply.errorMessage().isEmpty() ? i18n("No answer from the translation daemon.") : reply.errorMessage());
            return;
        }
        Pairs pairs;
        const auto array = reply.arguments().at(0).value<QDBusArgument>();
        array.beginArray();
        while (!array.atEnd()) {
            QVariantMap record;
            array >> record;
            QString source = record.value(u"source"_s).toString();
            QString target = record.value(u"target"_s).toString();
            if (!source.isEmpty() && !target.isEmpty()) {
                pairs.append({std::move(source), std::move(target)});
            }
        }
        array.endArray();
        callback(pairs, QString());
    });
}

#include "dragomanclient.moc"
