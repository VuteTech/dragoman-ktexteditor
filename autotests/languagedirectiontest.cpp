/*
    SPDX-FileCopyrightText: 2026 Blagovest Petrov <blagovest@petrovs.info>
    SPDX-FileCopyrightText: 2026 Vute Tech Ltd. <https://vute.tech>
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "languagedirection.h"
#include "dragomanclient.h"

#include <QTest>

using namespace Qt::StringLiterals;
using Dragoman::LanguagePair;

class LanguageDirectionTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void chooseDirection_data()
    {
        QTest::addColumn<QString>("source");
        QTest::addColumn<QString>("target");
        QTest::addColumn<QString>("text");
        QTest::addColumn<QString>("expectedSource");

        QTest::newRow("cyrillic text keeps bg-en") << u"bg"_s << u"en"_s << u"Добро утро"_s << u"bg"_s;
        QTest::newRow("latin text reverses bg-en") << u"bg"_s << u"en"_s << u"Good morning"_s << u"en"_s;
        QTest::newRow("latin text keeps en-bg") << u"en"_s << u"bg"_s << u"Good morning"_s << u"en"_s;
        QTest::newRow("cyrillic text reverses en-bg") << u"en"_s << u"bg"_s << u"Добро утро"_s << u"bg"_s;
        QTest::newRow("mixed text keeps the saved pair") << u"bg"_s << u"en"_s << u"Добро утро, Kate"_s << u"bg"_s;
        QTest::newRow("no letters keep the saved pair") << u"bg"_s << u"en"_s << u"12:30 -- 42"_s << u"bg"_s;
        QTest::newRow("same script cannot decide") << u"de"_s << u"en"_s << u"Good morning"_s << u"de"_s;
        QTest::newRow("two cyrillic languages cannot decide") << u"bg"_s << u"ru"_s << u"Good morning"_s << u"bg"_s;
    }

    void chooseDirection()
    {
        QFETCH(QString, source);
        QFETCH(QString, target);
        QFETCH(QString, text);
        QFETCH(QString, expectedSource);

        const LanguagePair result = Dragoman::chooseDirection({source, target}, text);
        QCOMPARE(result.first, expectedSource);
        QCOMPARE(result.second, expectedSource == source ? target : source);
    }

    void isCyrillicLanguage()
    {
        QVERIFY(Dragoman::isCyrillicLanguage(u"bg"_s));
        QVERIFY(Dragoman::isCyrillicLanguage(u"uk"_s));
        QVERIFY(!Dragoman::isCyrillicLanguage(u"en"_s));
        QVERIFY(!Dragoman::isCyrillicLanguage(QString()));
    }

    void languageLabel()
    {
        QCOMPARE(Dragoman::languageLabel(u"bg"_s), u"Bulgarian (bg)"_s);
        QCOMPARE(Dragoman::languageLabel(u"xx-not-a-language"_s), u"xx-not-a-language"_s);
    }

    // The escaping rule of the daemon's D-Bus contract: every character of
    // the sender's unique name that is not an ASCII letter or digit becomes
    // '_', including the leading ':'.
    void requestPath()
    {
        QCOMPARE(DragomanClient::requestPath(u":1.42", u"ktexteditor_7_0"), u"/dev/l10n_bg/dragomand/request/_1_42/ktexteditor_7_0"_s);
        QCOMPARE(DragomanClient::requestPath(u":1.2-x_y", u"t"), u"/dev/l10n_bg/dragomand/request/_1_2_x_y/t"_s);
    }
};

QTEST_GUILESS_MAIN(LanguageDirectionTest)

#include "languagedirectiontest.moc"
