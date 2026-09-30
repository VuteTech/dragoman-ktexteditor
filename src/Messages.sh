#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Blagovest Petrov <blagovest@petrovs.info>
# SPDX-FileCopyrightText: 2026 Vute Tech Ltd. <https://vute.tech>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Extracts the translatable strings into $podir/dragoman-ktexteditor.pot,
# following KDE's Messages.sh convention (run by KDE's scripty, or by hand
# with EXTRACTRC, XGETTEXT and podir set).
$EXTRACTRC ./*.rc >> rc.cpp
# podir comes from the environment, like EXTRACTRC and XGETTEXT.
# shellcheck disable=SC2154
$XGETTEXT ./*.cpp ./*.h -o "$podir/dragoman-ktexteditor.pot"
