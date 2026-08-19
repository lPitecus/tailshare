# SPDX-FileCopyrightText: 2026 Arthur Silva
# SPDX-License-Identifier: GPL-2.0-or-later

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

# Mirrors the installed layout inside the build tree, so Dolphin can load the
# plugin straight from here with QT_PLUGIN_PATH and no install step.
set_target_properties(tailshareitemaction PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins/kf6/kfileitemaction
)
