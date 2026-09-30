# SPDX-FileCopyrightText: 2026 Blagovest Petrov <blagovest@petrovs.info>
# SPDX-FileCopyrightText: 2026 Vute Tech Ltd. <https://vute.tech>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# KTextEditor plugin (Kate, KWrite, KDevelop): offline translation through
# the dragomand daemon.
#
#   cmake -S . -B build -G Ninja
#   cmake --build build
#   ctest --test-dir build
#   sudo cmake --install build        # or --prefix ~/.local
#
# `cmake --build build --target clang-format` applies the KDE code style.

cmake_minimum_required(VERSION 3.24)

# The version lives in git tags only (vX.Y.Z). In order of precedence:
# -DDRAGOMAN_VERSION=X.Y.Z, the .tarball-version file that the release
# tarball carries, `git describe` (X.Y.Z on a tag, X.Y.Z.N for N commits
# after it), and 0.0.0 outside any of these.
if(NOT DRAGOMAN_VERSION)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.tarball-version")
        file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/.tarball-version" DRAGOMAN_VERSION LIMIT_COUNT 1)
    else()
        execute_process(
            COMMAND git describe --tags --long --match "v[0-9]*"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            OUTPUT_VARIABLE _described
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
            RESULT_VARIABLE _describe_result)
        if(_describe_result EQUAL 0 AND _described MATCHES "^v([0-9]+\\.[0-9]+\\.[0-9]+)-([0-9]+)-g")
            if(CMAKE_MATCH_2 EQUAL 0)
                set(DRAGOMAN_VERSION "${CMAKE_MATCH_1}")
            else()
                set(DRAGOMAN_VERSION "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}")
            endif()
        else()
            set(DRAGOMAN_VERSION "0.0.0")
        endif()
    endif()
endif()

project(dragoman-ktexteditor VERSION ${DRAGOMAN_VERSION} LANGUAGES CXX)

# The oldest supported distribution release (Debian 13) sets these.
set(QT_MIN_VERSION "6.8.0")
set(KF_MIN_VERSION "6.13.0")

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

# Level 6.13: C++20, and KDE's strict Qt settings (no implicit ASCII
# casts, QT_NO_KEYWORDS, QStringBuilder, no narrowing in connect).
set(KDE_COMPILERSETTINGS_LEVEL 6.13)

include(KDEInstallDirs)
# Not an invent.kde.org project: predefining this target makes
# KDECMakeSettings skip its translation fetching, which probes the git
# remote at configure time.
add_custom_target(fetch-translations)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(KDEClangFormat)
include(KDEGitCommitHooks)
include(ECMDeprecationSettings)
include(ECMQtDeclareLoggingCategory)
include(FeatureSummary)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Core DBus Gui Widgets)
find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS Config CoreAddons I18n TextEditor XmlGui)

# Fail the build on any API deprecated up to the minimum versions.
ecm_set_disabled_deprecation_versions(QT ${QT_MIN_VERSION} KF ${KF_MIN_VERSION})

add_subdirectory(src)

if(BUILD_TESTING)
    find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Test)
    add_subdirectory(autotests)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/po")
    ki18n_install(po)
endif()

install(FILES dev.l10n_bg.dragomand.ktexteditor.metainfo.xml DESTINATION ${KDE_INSTALL_METAINFODIR})

file(GLOB_RECURSE ALL_CLANG_FORMAT_SOURCE_FILES src/*.cpp src/*.h autotests/*.cpp autotests/*.h)
kde_clang_format(${ALL_CLANG_FORMAT_SOURCE_FILES})
kde_configure_git_pre_commit_hook(CHECKS CLANG_FORMAT)

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
