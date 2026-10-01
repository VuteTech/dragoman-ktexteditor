/*
    SPDX-FileCopyrightText: 2026 Blagovest Petrov <blagovest@petrovs.info>
    SPDX-FileCopyrightText: 2026 Vute Tech Ltd. <https://vute.tech>
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#pragma once

#include <QString>

#include <utility>

namespace Dragoman
{

/// A translation direction: source and target language codes.
using LanguagePair = std::pair<QString, QString>;

/// Whether text in @p code is written in Cyrillic.
[[nodiscard]] bool isCyrillicLanguage(const QString &code);

/**
 * The direction to translate @p text in. The saved pair is the anchor;
 * the selection's script may reverse it: mostly Latin text with a Cyrillic
 * source (or the other way round) means the user selected text in the
 * target language, so it translates back. Pairs whose two languages share
 * a script, and text without letters, keep the saved direction.
 */
[[nodiscard]] LanguagePair chooseDirection(const LanguagePair &saved, const QString &text);

/// "bg" becomes "Bulgarian (bg)", the name in the user's language
/// (libdragoman-qt); codes it does not know stay as they are.
[[nodiscard]] QString languageLabel(const QString &code);

} // namespace Dragoman
