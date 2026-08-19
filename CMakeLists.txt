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

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Core)
find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS
    CoreAddons   # KPluginFactory
    KIO          # KAbstractFileItemActionPlugin
    I18n         # i18n()
    Notifications
    Archive      # KZip
)

# Tests are opt-out; ECM's KDECMakeSettings already handles BUILD_TESTING.
if(BUILD_TESTING)
    find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Test)
endif()

ecm_set_disabled_deprecation_versions(
    QT 6.11
    KF 6.28
)

add_subdirectory(src/core)

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
