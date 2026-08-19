# SPDX-FileCopyrightText: 2026 Arthur Silva
# SPDX-License-Identifier: GPL-2.0-or-later

# The transfer, without a single UI symbol: the plugin and the development
# probe both drive it, and the tests exercise it headless.
add_library(tailshare_send STATIC
    archiver.cpp
    sendjob.cpp
    sendmessages.cpp
)

target_include_directories(tailshare_send
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(tailshare_send
    PUBLIC tailshare_core Qt6::Core
    PRIVATE Qt6::Concurrent KF6::Archive KF6::I18n
)

target_compile_definitions(tailshare_send
    PRIVATE TRANSLATION_DOMAIN="tailshare"
)

set_target_properties(tailshare_send PROPERTIES POSITION_INDEPENDENT_CODE ON)
