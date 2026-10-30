# Fireworkz native Linux build (work in progress)
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
#
# Milestone 1: the platform-independent core (t5/cmodules) as a static library.
# Milestone 2: UI message resources (generated into <build>/Resources/Linux/Messages).
# See t5/Build/ReadMeL.txt for how to build and the porting plan.

cmake_minimum_required(VERSION 3.18) # cmake -E cat

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

# ---- Message resources -------------------------------------------------------
#
# MakeMess (-linux) preprocesses each module's UK tag:message source into the
# NUL-separated block that cmodules/resource.c loads from Linux/Messages/ObNN
# on the resources path, where NN is the OBJECT_ID (see ob_skel/xp_skel.h).
# Modules without an entry here have no messages of their own.

set(FIREWORKZ_RESOURCES_DIR ${CMAKE_CURRENT_BINARY_DIR}/Resources)

add_executable(make_messages ${T5}/Build/Tools/MakeMess/MakeMess.c)

set(FIREWORKZ_MESSAGE_SOURCES
    "00|ob_skel/resource/UK/msg|ob_skspt/resource/uk-msg"
    "01|ob_ss/resource/UK/msg"
    "04|ob_draw/resource/uk-msg"
    "06|ob_chart/resource/UK/msg"
    "07|ob_hefo/resource/uk-msg"
    "08|ob_spell/resource/UK/msg"
    "15|ob_dlg/resource/uk-msg"
    "17|ob_drwio/resource/uk-msg"
    "18|ob_toolb/resource/uk-msg"
    "19|ob_ruler/resource/uk-msg"
    "21|ob_mails/resource/uk-msg"
    "24|fl_csv/resource/uk-msg"
    "26|fl_xls/resource/uk-msg"
    "27|fl_123/resource/uk-msg"
    "28|fl_rtf/resource/uk-msg"
    "29|ob_spelb/resource/uk-msg"
    "30|ob_mlec/resource/uk-msg"
    "31|ob_file/resource/uk-msg"
    "38|ob_recn/resource/uk-msg")

set(FIREWORKZ_MESSAGE_FILES)
foreach(entry IN LISTS FIREWORKZ_MESSAGE_SOURCES)
    string(REPLACE "|" ";" parts "${entry}")
    list(POP_FRONT parts object_id)
    set(sources)
    foreach(part IN LISTS parts)
        list(APPEND sources ${T5}/${part})
    endforeach()
    set(joined ${CMAKE_CURRENT_BINARY_DIR}/msgs-src/Ob${object_id})
    set(out ${FIREWORKZ_RESOURCES_DIR}/Linux/Messages/Ob${object_id})
    add_custom_command(
        OUTPUT ${out}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${FIREWORKZ_RESOURCES_DIR}/Linux/Messages ${CMAKE_CURRENT_BINARY_DIR}/msgs-src
        COMMAND ${CMAKE_COMMAND} -E cat ${sources} > ${joined}
        COMMAND make_messages -linux -country UK -from ${joined} -to ${out}
        DEPENDS make_messages ${sources}
        VERBATIM)
    list(APPEND FIREWORKZ_MESSAGE_FILES ${out})
endforeach()

add_custom_target(fireworkz_messages ALL DEPENDS ${FIREWORKZ_MESSAGE_FILES})

enable_testing()

# sk_alpha.c supplies the real character class tables; the rest of ob_skel is stubbed for now
add_executable(fireworkz_core_smoke
    t5/Build/linux/core_smoke.c
    t5/Build/linux/core_smoke_host.c
    t5/ob_skel/sk_alpha.c)
target_link_libraries(fireworkz_core_smoke PRIVATE fireworkz_core)
target_compile_definitions(fireworkz_core_smoke PRIVATE FIREWORKZ_TEST_RESOURCES="${FIREWORKZ_RESOURCES_DIR}")
add_dependencies(fireworkz_core_smoke fireworkz_messages)
add_test(NAME core_smoke COMMAND fireworkz_core_smoke)
