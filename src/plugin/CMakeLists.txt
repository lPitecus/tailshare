# SPDX-FileCopyrightText: 2026 Arthur Silva
# SPDX-License-Identifier: GPL-2.0-or-later

# The version has one owner, project() in the top-level CMakeLists.txt. The
# metadata is generated from it so the installed plugin can say which version it
# is -- Dolphin shows it under Settings -> Context Menu, and a bug report can
# name it without digging in the package manager.
configure_file(tailshareitemaction.json.in ${CMAKE_CURRENT_BINARY_DIR}/tailshareitemaction.json @ONLY)

kcoreaddons_add_plugin(tailshareitemaction
    SOURCES
        tailshareitemaction.cpp
        closeguard.cpp
    INSTALL_NAMESPACE "kf6/kfileitemaction"
)

target_link_libraries(tailshareitemaction
    PRIVATE
        tailshare_notify
        KF6::KIOWidgets
        KF6::I18n
        KF6::WidgetsAddons
)

target_compile_definitions(tailshareitemaction
    PRIVATE TRANSLATION_DOMAIN="tailshare"
)

# K_PLUGIN_CLASS_WITH_JSON names the file, and moc looks for it along the
# include path: the generated copy has to exist before automoc runs.
set_property(TARGET tailshareitemaction APPEND PROPERTY
    AUTOGEN_TARGET_DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/tailshareitemaction.json)

# Mirrors the installed layout inside the build tree, so Dolphin can load the
# plugin straight from here with QT_PLUGIN_PATH and no install step.
set_target_properties(tailshareitemaction PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins/kf6/kfileitemaction
)
