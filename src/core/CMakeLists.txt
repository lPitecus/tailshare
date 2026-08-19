# SPDX-FileCopyrightText: 2026 Arthur Silva
# SPDX-License-Identifier: GPL-2.0-or-later

add_library(tailshare_core STATIC
    version.cpp
)

target_include_directories(tailshare_core
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(tailshare_core
    PUBLIC Qt6::Core
)

target_compile_definitions(tailshare_core
    PRIVATE TAILSHARE_VERSION_STRING="${PROJECT_VERSION}"
)

# The plugin that links this is a shared module.
set_target_properties(tailshare_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
