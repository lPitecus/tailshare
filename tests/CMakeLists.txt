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
