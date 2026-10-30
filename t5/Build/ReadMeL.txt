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

Milestone 2 - DONE for messages: UI message text
    cmodules/resource.c now reads Messages/ObNN (one file per OBJECT_ID)
    from Linux/ on the resources path, reusing the RISC OS tag:message
    index and lookup code. CMake generates the files from each module's
    UK *-msg source with MakeMess -linux (MakeMess now builds natively;
    -linux keeps w-prefixed lines and drops r-prefixed ones, the reverse
    of the RISC OS build, to follow WINDOWS). Output is under
    <build>/Resources/Linux/Messages. core_smoke checks the lookup.
    ob_skspt's messages are appended to Ob00 (assumed: its object ID
    is not in the OBJECT_ID enum). Only UK is generated so far.
    Message text is 8-bit Latin-N (TSTR is SBSTR here); the Qt layer
    must convert to UTF-16 for display.
    NOT YET DONE: toolbar bitmaps (resource_bitmap_* are still absent
    on Linux and arrive with the Qt layer in milestone 3).

Milestone 3 - IN PROGRESS: skeleton and the Qt interface
    All 51 ob_skel files build as libfireworkz_skel, with the dialog metrics,
    resource IDs and view/edge geometry following WINDOWS. Save As is declared
    as windows_save_as() for the Qt layer to supply.

    The host layer is split in two:

      ob_skel/linux/ho_*.c   the host_*() functions, in C (the counterpart of
                             ob_skel/windows/ho_*.c)
      ob_skel/linux/qt_host.h  the narrow C interface, qth_*(), to everything
                             that needs Qt; implemented in C++ in ob_skel/linux/qt/
                             (not yet written) and for tests by qth_null.c

    qt_host.h is plain C with no Fireworkz dependencies. Sizes and font metrics
    cross it in pixits; text is Latin-1; all calls are from the GUI thread.

    Done: fonts. fonty.c enumerates host fonts through qth_font_enumerate() into
    the same sorted table as Windows (a small LOGFONT stand-in in xp_skel.h), and
    measures through qth_font_metrics()/qth_font_text_width(). ho_font.c has
    host_font_find/dispose/delete and host_fonty_uchars_width.
    font_smoke tests this against the headless backend.

    Done: painting and windows (interface and C side). qt_host.h now has the
    painter (fill/frame rectangle, line, invert, clip, text, image), the window
    calls and the event structs. ho_xform.c and ho_paint.c turn pixit geometry
    into device pixels with the context's host_xform and call the painter:
    rectangles, crossed rectangles, inverted outlines, ruler and dashed lines,
    clipping and simple text. qth_null.c records paint calls and window state
    and can inject events; paint_smoke checks the whole path.

    Libraries: fireworkz_host_linux is ob_skel/linux/ho_*.c alone (what the
    tests link); fireworkz_skel is the rest of ob_skel.

    Still to do in qt_host.h and ho_*.c: menus and dialogs, clipboard, Save As /
    Open file dialogs, printing, the view/window functions (host_view_init and
    friends, ho_win.c), keyboard input, bitmaps and the toolbar images, and
    the Qt (C++) implementation of all of it.
    Known gaps in what exists: chunk widths ignore italic under/overhang;
    host_paint_border_line, framed boxes, bitmaps, drawfile and plain text
    painting are not ported yet.

Interim stubs (search for "TODO" near "LINUX"):
    gr_rdiag.c   text bounding boxes are estimated, not measured
    im_convert.c image conversion fails (no GDI+ or RISC OS ChangeFSI)

Outside cmodules, 82 of the 123 other files already compile for Linux
unchanged (37 of the 51 in ob_skel).


Next milestones
---------------

2. Resources: message text is done (see Status); toolbar bitmaps remain.

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
