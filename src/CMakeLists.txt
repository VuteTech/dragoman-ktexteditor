# SPDX-FileCopyrightText: 2026 Blagovest Petrov <blagovest@petrovs.info>
# SPDX-FileCopyrightText: 2026 Vute Tech Ltd. <https://vute.tech>
# SPDX-License-Identifier: GPL-3.0-or-later

# The D-Bus client and the direction logic, shared by the plugin and the
# autotests.
add_library(dragomancore STATIC)
target_sources(dragomancore PRIVATE
    dragomanclient.cpp
    dragomanclient.h
    languagedirection.cpp
    languagedirection.h)
ecm_qt_declare_logging_category(dragomancore
    HEADER dragoman_debug.h
    IDENTIFIER DRAGOMAN_LOG
    CATEGORY_NAME dev.l10n_bg.dragomand.ktexteditor
    DESCRIPTION "Dragoman translation plugin for KTextEditor"
    EXPORT DRAGOMAN)
set_target_properties(dragomancore PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_definitions(dragomancore PUBLIC TRANSLATION_DOMAIN="dragoman-ktexteditor")
target_include_directories(dragomancore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(dragomancore PUBLIC
    Qt6::Core
    Qt6::DBus
    KF6::I18n)

# The plugin metadata carries the version from the build.
configure_file(dragomanplugin.json.in ${CMAKE_CURRENT_BINARY_DIR}/dragomanplugin.json @ONLY)

kcoreaddons_add_plugin(dragomanplugin INSTALL_NAMESPACE "kf6/ktexteditor")
target_sources(dragomanplugin PRIVATE
    dragomanplugin.cpp
    dragomanplugin.h
    plugin.qrc)
target_link_libraries(dragomanplugin PRIVATE
    dragomancore
    Qt6::Gui
    Qt6::Widgets
    KF6::ConfigCore
    KF6::CoreAddons
    KF6::TextEditor
    KF6::XmlGui)

ecm_qt_install_logging_categories(EXPORT DRAGOMAN FILE dragoman-ktexteditor.categories DESTINATION ${KDE_INSTALL_LOGGINGCATEGORIESDIR})
