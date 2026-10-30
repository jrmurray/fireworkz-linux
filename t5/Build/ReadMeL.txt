ReadMe for Fireworkz Build (Linux) - WORK IN PROGRESS
----------------------------------------------------

** This Source Code Form is subject to the terms of the Mozilla Public
** License, v. 2.0. If a copy of the MPL was not distributed with this
** file, You can obtain one at https://mozilla.org/MPL/2.0/.

A native Linux port, aimed at KDE Plasma (Qt 6 / KDE Frameworks 6).
It is NOT yet an application: so far only the platform-independent core
builds and is tested.


Prerequisites
-------------

    sudo apt install build-essential cmake ninja-build      # Debian, Ubuntu, KDE neon
    sudo dnf install gcc cmake ninja-build                  # Fedora
    sudo pacman -S base-devel cmake ninja                   # Arch


Building and testing
--------------------

From the top of the repository:

    cmake -S . -B build -G Ninja
    cmake --build build
    ctest --test-dir build --output-on-failure

Add -DFIREWORKZ_RELEASED=OFF for a checking build (tracing, runtime assertions).


How the port is structured
--------------------------

LINUX is a third target alongside RISCOS and WINDOWS, set by TARGET_LINUX
(CMakeLists.txt passes it) or by __linux__. See cmodules/coltsoft/defineos.h
and cmodules/coltsoft/target_linux.h.

Where the two existing targets differ, LINUX follows WINDOWS for anything
that affects document compatibility or on-screen geometry:

    96 dpi pixel geometry           (ob_skel/xp_skel.h)
    Windows-1252 8-bit code page    (cmodules/ucs4conv.c)
    little-endian IEEE doubles      (cmodules/typepack.c)

and is POSIX everywhere else:

    paths use '/', search paths ':', MAX_PATHSTRING 1023    (cmodules/file.h)
    files via C stdio (FILE *); mkdir/remove/rename/stat    (cmodules/file.c)
    directory listing via opendir/readdir/fnmatch           (cmodules/fileutil.c)
    temporary files under $TMPDIR, else /tmp                (cmodules/fileutil.c)
    millisecond clock from CLOCK_MONOTONIC                  (cmodules/monotime.c)

Host handles (HOST_WND, HOST_FONT, RESOURCE_BITMAP_HANDLE, the painter in
REDRAW_CONTEXT_LINUX) are opaque pointers, to be owned by the Qt layer.

The Windows build gets Draw-file geometry from the Dial Solutions DLLs,
which can't be used here. cmodules/gr_rdiag.c computes path bounding boxes
itself (the control polygon, plus half the line width).


Status
------

Milestone 1 - DONE: core library
    All 94 files in cmodules build as libfireworkz_core (GCC or Clang).
    Build/linux/core_smoke.c checks dates, code page conversion,
    filenames, the double layout, file I/O and the clock.
    Build/linux/core_smoke_host.c is a TEST-ONLY stand-in for the
    handful of ob_skel symbols the core needs.

Interim stubs (search for "TODO" near "LINUX"):
    gr_rdiag.c   text bounding boxes are estimated, not measured
    im_convert.c image conversion fails (no GDI+ or RISC OS ChangeFSI)

Outside cmodules, 82 of the 123 other files already compile for Linux
unchanged (37 of the 51 in ob_skel).


Next milestones
---------------

2. Resources: UI message text and toolbar bitmaps. The RISC OS Messages
   files are plain text and are the likely source for Linux. This replaces
   resource_lookup_tstr_no_default() and friends in cmodules/resource.c.

3. Skeleton: the rest of ob_skel, which mostly needs the per-platform
   dialog metrics (DIALOG_STD*), plus a new ob_skel/linux host layer
   (the counterpart of ob_skel/windows/ho_*.c) written in C++ against
   Qt 6 behind a C interface: windows, events, painting, fonts, menus,
   dialogs, clipboard and printing (QPrinter).

4. Objects and converters: the remaining ob_* and fl_*/fs_* modules,
   then firewrkz/firewrkz.c as the application entry point.

5. Desktop integration: .desktop file, icons, MIME types for Fireworkz
   documents, KDE file dialogs.


Known upstream issue
--------------------

ucs4conv.c maps byte 0x80 in Windows-1252 (and the Acorn Latin tables) to
U+20A0 EURO-CURRENCY SIGN rather than U+20AC EURO SIGN. This isn't changed
here because it affects every platform.
