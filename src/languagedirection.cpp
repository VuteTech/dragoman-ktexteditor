/*
    SPDX-FileCopyrightText: 2026 Blagovest Petrov <blagovest@petrovs.info>
    SPDX-FileCopyrightText: 2026 Vute Tech Ltd. <https://vute.tech>
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "languagedirection.h"

#include <QLocale>

#include <algorithm>
#include <array>

using namespace Qt::StringLiterals;

namespace Dragoman
{

bool isCyrillicLanguage(const QString &code)
{
    // Mirrors CYRILLIC_SOURCES in the LibreOffice extension.
    static constexpr std::array cyrillic{u"bg", u"ru", u"uk", u"be", u"sr"};
    return std::ranges::any_of(cyrillic, [&code](const char16_t *language) {
        return code == QStringView(language);
    });
}

LanguagePair chooseDirection(const LanguagePair &saved, const QString &text)
{
    const bool sourceCyrillic = isCyrillicLanguage(saved.first);
    const bool targetCyrillic = isCyrillicLanguage(saved.second);
    if (sourceCyrillic == targetCyrillic) {
        return saved; // the script cannot tell the two languages apart
    }
    const auto cyrillic = std::ranges::count_if(text, [](QChar c) {
        return c.script() == QChar::Script_Cyrillic;
    });
    const auto latin = std::ranges::count_if(text, [](QChar c) {
        return c.script() == QChar::Script_Latin;
    });
    if (cyrillic + latin == 0) {
        return saved;
    }
    const double ratio = double(cyrillic) / double(cyrillic + latin);
    if ((sourceCyrillic && ratio < 0.3) || (targetCyrillic && ratio > 0.7)) {
        return {saved.second, saved.first};
    }
    return saved;
}

QString languageLabel(const QString &code)
{
    const QLocale locale(code);
    if (locale.language() == QLocale::C) {
        return code;
    }
    return u"%1 (%2)"_s.arg(QLocale::languageToString(locale.language()), code);
}

} // namespace Dragoman
