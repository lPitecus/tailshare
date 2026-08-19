# SPDX-FileCopyrightText: 2026 Arthur Silva
# SPDX-License-Identifier: GPL-2.0-or-later

include(ECMMarkAsTest)

set(TAILSHARE_CORE_TESTS
    statusparsertest
    devicestest
    taildropreasontest
    sendplantest
    tailscaleclienttest
)

# These drive the transfer, so they need the send library (and KZip, to read
# back what the archiver wrote).
set(TAILSHARE_SEND_TESTS
    archivertest
    sendjobtest
)

foreach(test ${TAILSHARE_CORE_TESTS})
    add_executable(${test} ${test}.cpp)
    target_link_libraries(${test} PRIVATE tailshare_core Qt6::Test KF6::I18n)
    target_compile_definitions(${test} PRIVATE FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
    add_test(NAME ${test} COMMAND ${test})
    ecm_mark_as_test(${test})
endforeach()

foreach(test ${TAILSHARE_SEND_TESTS})
    add_executable(${test} ${test}.cpp)
    target_link_libraries(${test} PRIVATE tailshare_send Qt6::Test KF6::Archive KF6::I18n)
    target_compile_definitions(${test} PRIVATE FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
    add_test(NAME ${test} COMMAND ${test})
    ecm_mark_as_test(${test})
endforeach()

# Nothing to link, just gettext: keeps the catalog honest as strings change.
find_program(GETTEXT_MSGCMP_EXECUTABLE msgcmp)
find_program(GETTEXT_XGETTEXT_EXECUTABLE xgettext)
if(GETTEXT_MSGCMP_EXECUTABLE AND GETTEXT_XGETTEXT_EXECUTABLE)
    add_test(NAME translationstest
             COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/check-translations.sh ${CMAKE_SOURCE_DIR})
endif()

# The pt-BR catalog, against the catalog this build compiled into <build>/locale.
add_executable(i18ntest i18ntest.cpp)
target_link_libraries(i18ntest PRIVATE tailshare_send Qt6::Test KF6::I18n)
target_compile_definitions(i18ntest PRIVATE BUILD_LOCALE_DIR="${CMAKE_BINARY_DIR}/locale")
add_dependencies(i18ntest pofiles)
add_test(NAME i18ntest COMMAND i18ntest)
ecm_mark_as_test(i18ntest)

# Drives the built plugin binary through KIO's own loading path, so it needs
# the .so itself rather than a library to link against.
if(TARGET tailshareitemaction)
    add_executable(plugintest plugintest.cpp)
    target_link_libraries(plugintest PRIVATE Qt6::Test Qt6::Widgets KF6::KIOWidgets KF6::CoreAddons KF6::I18n)
    target_compile_definitions(plugintest PRIVATE
        FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
        PLUGIN_PATH="$<TARGET_FILE:tailshareitemaction>"
        PROJECT_VERSION="${PROJECT_VERSION}"
    )
    add_dependencies(plugintest tailshareitemaction)
    add_test(NAME plugintest COMMAND plugintest)
    # No display in a test run; the plugin only ever needs a parent widget.
    set_tests_properties(plugintest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_PLUGIN_PATH=${CMAKE_BINARY_DIR}/plugins")
    ecm_mark_as_test(plugintest)
endif()
