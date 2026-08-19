# SPDX-FileCopyrightText: 2026 Arthur Silva
# SPDX-License-Identifier: GPL-2.0-or-later

# Development tool: built, never installed (no install() rule on purpose).
add_executable(tailshare-probe main.cpp)

target_link_libraries(tailshare-probe
    PRIVATE tailshare_send tailshare_notify Qt6::Core Qt6::Gui KF6::I18n
)

target_compile_definitions(tailshare-probe
    PRIVATE TRANSLATION_DOMAIN="tailshare"
)
