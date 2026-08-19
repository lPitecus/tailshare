# SPDX-FileCopyrightText: 2026 Arthur Silva
# SPDX-License-Identifier: GPL-2.0-or-later

# Kept apart from tailshare_send on purpose: KF6::Notifications drags in
# Qt6::Gui, and neither the tests nor the probe's default mode need a display.
add_library(tailshare_notify STATIC
    sendnotifier.cpp
)

target_include_directories(tailshare_notify
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(tailshare_notify
    PUBLIC tailshare_send Qt6::Core
    PRIVATE KF6::Notifications KF6::I18n
)

target_compile_definitions(tailshare_notify
    PRIVATE TRANSLATION_DOMAIN="tailshare"
)

set_target_properties(tailshare_notify PROPERTIES POSITION_INDEPENDENT_CODE ON)
