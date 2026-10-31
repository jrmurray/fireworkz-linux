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

    The Qt 6 backend (ob_skel/linux/qt/qth_qt.cpp, library fireworkz_qth_qt) implements
    qt_host.h for real: fonts from QFontDatabase, painting with QPainter (invert is
    RasterOp_NotDestination), windows as QWidgets turning Qt events into QTH_EVENTs,
    an inverted caret and the mouse grab. qt_smoke runs it offscreen
    (QT_QPA_PLATFORM=offscreen) and checks fonts, resize/paint events and the
    pixels that come out of fill, invert, frame and clip. Needs Qt 6 Widgets
    (Fedora qt6-qtbase-devel); -DFIREWORKZ_BUILD_QT=OFF skips it.
    Not yet in the Qt backend: scroll bars (extent and position are remembered
    only), menus, dialogs, clipboard, printing.

    Libraries: fireworkz_host_linux is ob_skel/linux/ho_*.c alone (what the
    tests link); fireworkz_skel is the rest of ob_skel.

    Still to do in qt_host.h and ho_*.c: menus and dialogs, clipboard, Save As /
    Open file dialogs, printing, the view/window functions (host_view_init and
    friends, ho_win.c), keyboard input, bitmaps and the toolbar images, and
    the Qt (C++) implementation of all of it.
    Known gaps in what exists: chunk widths ignore italic under/overhang;
    host_paint_border_line, framed boxes, bitmaps, drawfile and plain text
    painting are not ported yet.

Milestone 4 - DONE (first light): the application runs
    The fireworkz program links, starts, loads the default template and shows a
    document window; a spreadsheet template shows column headers, grid lines,
    the selected cell and scroll bars. app_smoke runs it on a virtual display
    and checks it opens a window and draws something.

    To run it (the build tree finds its own resources):

        cmake -S . -B build -G Ninja && cmake --build build
        build/fireworkz                                  # on the desktop
        QT_QPA_PLATFORM=offscreen FIREWORKZ_SCREENSHOT=/tmp/fw build/fireworkz
                                                         # no display: writes /tmp/fw-0.png

    Choosing the first document: FIREWORKZ_TEMPLATES=<dir> uses the first template in that
    directory, FIREWORKZ_RESOURCES=<path> overrides where resources are looked for.
    The build stages what the program looks for into build/Resources: Linux/Messages/ObNN
    (UI text), Linux/Bitmaps/ObNN (id to image file), Config.txt, Templates/*.fwt.

    How it fits together
      ob_skel/linux/ho_main.c    main(): startup, first document, event loop
      ho_event.c                 wm_event_get(): input, then scheduled and null events when idle
      ho_win.c                   views, panes, rulers, scroll bars, clicks, keys, drag, caret
                                 (from windows/ho_win.c, Win32 calls shimmed over qt_host.h)
      ho_paint.c ho_text.c       painting through the redraw context; document text via fonty
      ho_marker.c ho_font.c ho_xform.c ho_misc.c ho_color.c
      ho_stubs.c                 clearly marked stand-ins (below)
      qt/qth_qt.cpp              the Qt 6 implementation of qt_host.h
    Resources: cmodules/resource.c loads messages and bitmaps; ob_dlg/li_dlg.c measures text
    in the dialog font.

    Menus, toolbar and status line: done (first cut)
      ho_menu.c      builds each window's QMenuBar (and popups) from the skeleton's menu trees, with
                     an item's MENU_ENTRY as its client; items are enabled when a menu is about to be
                     shown, as host_onInitMenu does on Windows; choosing an item runs its macro command
      ob_toolb/li_toolb.c   the toolbar and status line windows: placing them, mouse handling over the
                     buttons (shared with Windows' wi_toolb.c), creating and disposing them
      ob_toolb.c     paints the toolbar from the icon strip in the resources, and the status line
      The default Choices (which say which tools the toolbar has) are staged from the RISC OS set
      into build/Resources/Choices as ChoiceNN.txt.

    Dialogs: done (first cut)
      ob_dlg/li_dlg.c builds each dialog as a Qt dialog (qth_dlg_* in qt_host.h, implemented in
                     qt/qth_qt.cpp) from the engine's DIALOG_CONTROL tables, as wi_dlg.c does with a
                     Windows dialog template; the widgets' events go back to the same engine entry
                     points the Windows messages do. Sizes use the Windows dialog units, measured in
                     the desktop's font. A spin control is one widget (edit plus arrows).
      Menu items are reached without a keyboard for tests with FIREWORKZ_TEST_MENU="Page>Paper..."
      (with FIREWORKZ_SCREENSHOT): app_smoke does this and checks that the dialog appears.
      Tables that give a layout only for RISC OS and Windows had to be widened to Linux
      (ui_data.c, sk_choic.c, sk_find.c, ui_styl2.c, ...): a dialog that loops forever laying itself
      out, or crashes on the first run, has probably missed one. Other dialogs have not all been
      looked at yet.

    Open, Insert File, Save As: done (first cut)
      qth_file_dialog() is a QFileDialog (the KDE one on Plasma) with the program's own file type
      filters. ob_file.c's open_document and ui_save.c's windows_save_as have Linux versions that
      follow the Windows ones. FIREWORKZ_TEST_FILE=path stands in for the chooser in tests;
      app_smoke saves a document and opens it again. File names are passed as raw bytes, so names
      outside ASCII show wrongly in the program's Latin-1 text but still work.
      The toolbar now starts with New and Open (RISC OS has no such buttons), staged into Choice18.
      Not done: the Save As dialog's extra "save selection" check box and page range (the Windows
      common dialog hook), template picking from folders, "Save As Template" naming, recent files.
      Fixed on the way: of_save.c indexed one before an arglist when all its arguments were empty
      (harmless on 32-bit, a crash on 64-bit).

    Input, tested with a script (FIREWORKZ_TEST_SCRIPT, see test_script_start() in qth_qt.cpp):
      "wait:800;click:100,150;type:hello;key:Return;menu:Edit>Find...;button:From Top;shot:out;quit"
      sends Qt mouse and key events to the document window, or to the modal dialog if there is one.
      Seen to work offscreen: typing into cells, Return, Home, arrows, shift+arrows selecting,
      replacing a selection, Delete, double click selecting a word, toolbar buttons (Bold on and off,
      the status line help text), menu items, the Find dialog (typing, From Top), the Paper dialog
      (spin arrows, Apply and the value kept), Choices, the Box dialog (palette swatches, line style
      pictures, the Colour... chooser), Save As and Open. app_smoke does a typing run too.
      Not tried: dragging out a selection, the mouse wheel, scroll bars by mouse, resizing panes,
      right clicks and pop-up menus, keyboard menu accelerators and shortcuts (Ctrl+...), drag and
      drop, a real window manager (focus, activation, repeat keys), the KDE look of the dialogs.
      Bugs this found: host_keys_in_buffer() had no Linux branch and returned garbage, so some keys
      stayed in the cache unsent; the Colour... button only had Windows and RISC OS code.
      Compiler warnings still to look at: u.f64 used uninitialised in fl_123.c and fl_xls_loadb.c
      (Lotus and Excel number import, probably a byte-order branch not done for Linux), and
      sle_ss.c:958 uses hwnd uninitialised.

    Clipboard: done (first cut), for text in cells
      Copy and Cut keep the selection as before and also put it on the desktop's clipboard, in the
      program's own format (application/x-fireworkz) and as plain text; Paste takes the program's own
      format if the clipboard has it (so styles and cells survive, even between two copies of the
      program), else plain text, else what the program kept. Edit>Paste is greyed only when there is
      nothing anywhere. The pieces: qth_clipboard_* in qt_host.h, linux_clipboard_* in ob_cells.c.
      Tested offscreen: copy, cut, paste, repeated paste, and text set by "another program"
      (script step clip:text); app_smoke does one of each.
      Not done: pasting other formats (RTF, CSV, spreadsheets, pictures), copying pictures, notes and
      other objects' selections (only cells' Copy/Cut publish), a "clipboard changed" refresh of the
      Paste menu while the menu is already open, and a primary selection (middle click) paste.

    Printing: done (first cut)
      ob_skel/linux/ho_print.c lays the pages out as windows/ho_print.c does (scale, margins, two up,
      collated copies) onto a QPrinter's painter, through qth_print_* in qt_host.h. Print Setup is
      Qt's QPrintDialog, Print... is the program's own dialog, the Paper dialog's Read Printer button
      reads the chosen printer. FIREWORKZ_PRINT_PDF=file sends the output to a PDF whatever printers
      there are: app_smoke does this and, with pdftotext, checks the words arrived as text.
      Seen: a one page A4 document, with real (searchable) text. Not tried: several pages, two up,
      landscape, copies, a real printer or the CUPS queue, scaling, charts and pictures on paper,
      the progress shown during a long print, cancelling a print (there is no abort button yet),
      print preview, draft printing (T5_MSG_DRAFT_PRINT is still a stub).

    Draw files: done (first cut)
      ob_skel/linux/ho_draw.c paints a RISC OS Draw file with Qt paths (qth_path_*, qth_paint_path,
      qth_paint_text_scaled): paths with fills, lines, dashes, caps, joins and even-odd or winding
      rules; text, also transformed (turned) text, with the font list mapped to Sans, Serif and
      Monospace by the RISC OS font name; groups and tagged objects. Draws on the screen and onto a
      printer. Seen: the Fireworkz logo (t5/Resources/logos/fzc,aff, a real Draw file) inserted with
      Edit>Insert>Picture, complete with its turned text, and a chart's frame, axes, labels and legend
      on a printed page. app_smoke inserts the logo.
      Not done: sprites, JPEGs, Windows DIBs (draw_OBJDIB), text areas, triangle line caps (drawn
      flat), exclusive-or painting (eor_paths, used when dragging; drawn normally), and the font
      mapping is by name only. Chart bars were not seen: the one chart tried had no data, as the
      script's way of typing cells did not work out, so a chart with data still wants looking at.
      Saving to a Draw file (drwprint.c's drawfile_paint_*) is separate and has not been tried.

    What does not work yet (each is marked TODO where it lives):
      picture controls: drawn (QTH_DLG_BLANK canvases, painted by li_dlg.c from the resource bitmaps,
                                    colour swatches and other user controls through the redraw context)
                                    but only looked at, not clicked: hover/press/click are wired up untested
      help from dialogs             dialog_windows_help() does nothing
      key shortcuts shown in menus  informational only: no QShortcut is bound (the skeleton maps keys)
      tooltips, the lower status line, the tool tip hover look
      print preview
            the clipboard, drag and drop, help
      row borders and rulers on the default template; italic under/overhang in text widths
      key shortcuts (Ctrl+...) are menu accelerators on Windows, so wait for menus
      packaging                     .desktop file, icon, MIME types, install rules

Interim stubs (search for "TODO" near "LINUX"):
    gr_rdiag.c   text bounding boxes are estimated, not measured
    im_convert.c image conversion fails (no GDI+ or RISC OS ChangeFSI)

Outside cmodules, 82 of the 123 other files already compile for Linux
unchanged (37 of the 51 in ob_skel).


Next milestones
---------------

2. Resources: messages and bitmaps (loaded by id): done.

3. Skeleton and 4. Objects: done (see Milestone 3 and 4 above).

5. Make it a usable application: (menu bar, toolbar and dialogs are done) picture controls in dialogs,
   (Open and Save As are done)  the clipboard, printing, Draw file rendering.

6. Desktop integration: .desktop file, icons, MIME types for Fireworkz documents.


Known upstream issue
--------------------

ucs4conv.c maps byte 0x80 in Windows-1252 (and the Acorn Latin tables) to
U+20A0 EURO-CURRENCY SIGN rather than U+20AC EURO SIGN. This isn't changed
here because it affects every platform.
