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
list(APPEND FIREWORKZ_CORE_SOURCES ${T5}/external/OtherFree/wn_pnpoly.c)

add_library(fireworkz_core STATIC ${FIREWORKZ_CORE_SOURCES})

target_include_directories(fireworkz_core PUBLIC ${T5})

target_compile_definitions(fireworkz_core PUBLIC TARGET_LINUX PORTABLE_MULDIV64)
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

# ---- Skeleton (milestone 3, in progress) --------------------------------------
#
# Everything in ob_skel, plus the Linux host layer (ob_skel/linux/ho_*.c), which
# reaches Qt only through ob_skel/linux/qt_host.h. The Windows and RISC OS host
# layers (ob_skel/windows, ob_skel/riscos) are not used.

file(GLOB FIREWORKZ_SKEL_SOURCES CONFIGURE_DEPENDS ${T5}/ob_skel/*.c)
add_library(fireworkz_skel STATIC ${FIREWORKZ_SKEL_SOURCES})
target_link_libraries(fireworkz_skel PUBLIC fireworkz_core)

# All the objects and the file converters (ob_*, fl_*, fs_*), except the skeleton's own
file(GLOB FIREWORKZ_OBJECT_SOURCES CONFIGURE_DEPENDS ${T5}/ob_*/*.c ${T5}/fl_*/*.c ${T5}/fs_*/*.c)
list(FILTER FIREWORKZ_OBJECT_SOURCES EXCLUDE REGEX "/ob_skel/")
add_library(fireworkz_objects STATIC ${FIREWORKZ_OBJECT_SOURCES})
target_link_libraries(fireworkz_objects PUBLIC fireworkz_core)

# The Linux host layer on its own, so that it can be tested without the rest of the skeleton
file(GLOB FIREWORKZ_HOST_SOURCES CONFIGURE_DEPENDS ${T5}/ob_skel/linux/ho_*.c)
add_library(fireworkz_host_linux STATIC ${FIREWORKZ_HOST_SOURCES})
target_link_libraries(fireworkz_host_linux PUBLIC fireworkz_core)

# Headless stand-in for the Qt layer, for tests
add_library(fireworkz_qth_null STATIC ${T5}/ob_skel/linux/qth_null.c)
target_compile_options(fireworkz_qth_null PRIVATE -Wall)
target_include_directories(fireworkz_qth_null PUBLIC ${T5})

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

add_executable(fireworkz_font_smoke t5/Build/linux/font_smoke.c t5/Build/linux/core_smoke_host.c t5/ob_skel/sk_alpha.c)
target_link_libraries(fireworkz_font_smoke PRIVATE fireworkz_host_linux fireworkz_qth_null fireworkz_core)
add_test(NAME font_smoke COMMAND fireworkz_font_smoke)

add_executable(fireworkz_paint_smoke t5/Build/linux/paint_smoke.c t5/Build/linux/core_smoke_host.c t5/ob_skel/sk_alpha.c)
target_link_libraries(fireworkz_paint_smoke PRIVATE fireworkz_host_linux fireworkz_qth_null fireworkz_core)
add_test(NAME paint_smoke COMMAND fireworkz_paint_smoke)

# ---- Qt 6 backend --------------------------------------------------------------
#
# The real implementation of ob_skel/linux/qt_host.h. Needs Qt 6 Widgets
# (Fedora: qt6-qtbase-devel; Debian/Ubuntu: qt6-base-dev).

option(FIREWORKZ_BUILD_QT "Build the Qt 6 host backend" ON)

if(FIREWORKZ_BUILD_QT)
    enable_language(CXX)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    find_package(Qt6 COMPONENTS Widgets)
endif()

if(FIREWORKZ_BUILD_QT AND Qt6_FOUND)
    add_library(fireworkz_qth_qt STATIC ${T5}/ob_skel/linux/qt/qth_qt.cpp)
    target_include_directories(fireworkz_qth_qt PUBLIC ${T5})
    target_link_libraries(fireworkz_qth_qt PUBLIC Qt6::Widgets)
    target_compile_options(fireworkz_qth_qt PRIVATE -Wall)

    add_executable(fireworkz_qt_smoke t5/Build/linux/qt_smoke.cpp)
    target_link_libraries(fireworkz_qt_smoke PRIVATE fireworkz_qth_qt)
    add_test(NAME qt_smoke COMMAND fireworkz_qt_smoke)
    set_tests_properties(qt_smoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
elseif(FIREWORKZ_BUILD_QT)
    message(WARNING "Qt 6 Widgets not found: the Qt backend is not built (-DFIREWORKZ_BUILD_QT=OFF to silence)")
endif()

# ---- The application (milestone 4, in progress) -----------------------------------
option(FIREWORKZ_BUILD_APP "Try to build the fireworkz executable (does not link yet)" OFF)

if(FIREWORKZ_BUILD_APP AND TARGET fireworkz_qth_qt)
    add_executable(fireworkz ${T5}/firewrkz/firewrkz.c)
    target_link_libraries(fireworkz PRIVATE
        -Wl,--start-group fireworkz_objects fireworkz_skel fireworkz_host_linux fireworkz_core -Wl,--end-group
        fireworkz_qth_qt)
endif()
