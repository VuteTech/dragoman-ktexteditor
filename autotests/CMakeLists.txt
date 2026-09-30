# SPDX-FileCopyrightText: 2026 Blagovest Petrov <blagovest@petrovs.info>
# SPDX-FileCopyrightText: 2026 Vute Tech Ltd. <https://vute.tech>
# SPDX-License-Identifier: GPL-3.0-or-later

include(ECMAddTests)

ecm_add_tests(
    languagedirectiontest.cpp
    dragomanclienttest.cpp
    LINK_LIBRARIES dragomancore Qt6::Test)
