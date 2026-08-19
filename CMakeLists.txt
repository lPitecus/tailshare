# SPDX-FileCopyrightText: 2026 Arthur Silva
# SPDX-License-Identifier: GPL-2.0-or-later

cmake_minimum_required(VERSION 3.20)

project(tailshare VERSION 0.1.0 LANGUAGES CXX)

set(QT_MIN_VERSION 6.6.0)
set(KF_MIN_VERSION 6.0.0)

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMDeprecationSettings)
include(FeatureSummary)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS
    Core
    Concurrent  # compressing off the main thread
    Gui         # pulled in by KF6::Notifications
    Widgets     # the plugin builds a QMenu
)
find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS
    CoreAddons     # KPluginFactory
    KIO            # KAbstractFileItemActionPlugin
    I18n           # i18n()
    Notifications
    Archive        # KZip
    WidgetsAddons  # KMessageBox, for the close warning
)

# Tests are opt-out; ECM's KDECMakeSettings already handles BUILD_TESTING.
if(BUILD_TESTING)
    find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Test)
endif()

ecm_set_disabled_deprecation_versions(
    QT 6.11
    KF 6.28
)

option(BUILD_TOOLS "Build the development probe (never installed)" ON)

add_subdirectory(src/core)
add_subdirectory(src/send)
add_subdirectory(src/notify)
add_subdirectory(src/plugin)

if(BUILD_TOOLS)
    add_subdirectory(tools/tailshare-probe)
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

ki18n_install(po)

install(FILES data/tailshare.notifyrc DESTINATION ${KDE_INSTALL_KNOTIFYRCDIR})

# The same file inside the build tree, so the probe can be run against it with
# XDG_DATA_DIRS=<build>/share:$XDG_DATA_DIRS before anything is installed.
configure_file(data/tailshare.notifyrc ${CMAKE_BINARY_DIR}/share/knotifications6/tailshare.notifyrc COPYONLY)

# ki18n_install() compiles the catalogues into <build>/locale, which is not
# where KLocalizedString looks. Mirror them under the same <build>/share the
# line above uses, so one XDG_DATA_DIRS covers notifications and translations.
if(TARGET pofiles)
    add_custom_target(tailshare_locale_in_build_tree ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_BINARY_DIR}/locale ${CMAKE_BINARY_DIR}/share/locale
        COMMENT "Mirroring message catalogs into share/locale"
        VERBATIM
    )
    add_dependencies(tailshare_locale_in_build_tree pofiles)
endif()

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
