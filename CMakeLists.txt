# Fireworkz native Linux build (work in progress)
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
#
# Milestone 1: the platform-independent core (t5/cmodules) as a static library.
# See t5/Build/ReadMeL.txt for how to build and the porting plan.

cmake_minimum_required(VERSION 3.16)

project(fireworkz LANGUAGES C)

option(FIREWORKZ_RELEASED "Build a release (no tracing, no runtime checks)" ON)

set(T5 ${CMAKE_CURRENT_SOURCE_DIR}/t5)

file(GLOB FIREWORKZ_CORE_SOURCES CONFIGURE_DEPENDS ${T5}/cmodules/*.c)

add_library(fireworkz_core STATIC ${FIREWORKZ_CORE_SOURCES})

target_include_directories(fireworkz_core PUBLIC ${T5})

target_compile_definitions(fireworkz_core PUBLIC TARGET_LINUX)
if(FIREWORKZ_RELEASED)
    target_compile_definitions(fireworkz_core PUBLIC RELEASED)
endif()

# The code base was written for compilers where char is unsigned (Norcroft, MSVC /J)
# and freely type-puns between structure layouts.
set_target_properties(fireworkz_core PROPERTIES C_STANDARD 99 C_EXTENSIONS ON)
target_compile_options(fireworkz_core PUBLIC
    -funsigned-char
    -fno-strict-aliasing
    -Wall
    -Wno-missing-braces
    -Wno-parentheses-equality
    -Wno-unknown-pragmas)

target_link_libraries(fireworkz_core PUBLIC m)

enable_testing()

# sk_alpha.c supplies the real character class tables; the rest of ob_skel is stubbed for now
add_executable(fireworkz_core_smoke
    t5/Build/linux/core_smoke.c
    t5/Build/linux/core_smoke_host.c
    t5/ob_skel/sk_alpha.c)
target_link_libraries(fireworkz_core_smoke PRIVATE fireworkz_core)
add_test(NAME core_smoke COMMAND fireworkz_core_smoke)
