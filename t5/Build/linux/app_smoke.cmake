# Start the fireworkz program on a virtual display with a screenshot request: it must open a document,
# save a picture of it and exit cleanly, and the picture must not be empty.
#
# Run as: cmake -DAPP=<program> -DSHOT=<picture name without -0.png> -P app_smoke.cmake

file(REMOVE ${SHOT}-0.png)

set(ENV{QT_QPA_PLATFORM} offscreen)
set(ENV{FIREWORKZ_SCREENSHOT} ${SHOT})

execute_process(COMMAND ${APP} RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 60)

if(NOT result EQUAL 0)
    message(FATAL_ERROR "fireworkz exited with ${result}\n${out}\n${err}")
endif()

if(NOT EXISTS ${SHOT}-0.png)
    message(FATAL_ERROR "fireworkz did not open a window to save a picture of\n${out}\n${err}")
endif()

file(SIZE ${SHOT}-0.png size)
if(size LESS 1000)
    message(FATAL_ERROR "the picture of the window is only ${size} bytes: nothing was drawn")
endif()

message(STATUS "fireworkz opened a window: ${size} byte picture")

# Then choose a menu item that opens a dialog (FIREWORKZ_TEST_MENU); the dialog must be on screen when the
# picture is taken, as a second window, and the program must still exit cleanly once it is over.

file(REMOVE ${SHOT}-dlg-0.png ${SHOT}-dlg-1.png)

set(ENV{FIREWORKZ_SCREENSHOT} ${SHOT}-dlg)
set(ENV{FIREWORKZ_SCREENSHOT_MS} 3000)
set(ENV{FIREWORKZ_TEST_MENU} "Page>Paper...")

execute_process(COMMAND ${APP} RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 60)

if(NOT result EQUAL 0)
    message(FATAL_ERROR "fireworkz exited with ${result} with a dialog open\n${out}\n${err}")
endif()

if(NOT EXISTS ${SHOT}-dlg-1.png)
    message(FATAL_ERROR "the Paper dialog did not open\n${out}\n${err}")
endif()

file(SIZE ${SHOT}-dlg-1.png dlg_size)
if(dlg_size LESS 1000)
    message(FATAL_ERROR "the picture of the dialog is only ${dlg_size} bytes: nothing was drawn")
endif()

message(STATUS "fireworkz opened a dialog: ${dlg_size} byte picture")

# Save As to a file (FIREWORKZ_TEST_FILE stands in for the file chooser), then open that file again: the saved
# file must be a Fireworkz document and opening it must give a second document window.

set(SAVED ${SHOT}-saved.fwk)
file(REMOVE ${SAVED} ${SHOT}-open-0.png ${SHOT}-open-1.png)

set(ENV{FIREWORKZ_TEST_FILE} ${SAVED})
set(ENV{FIREWORKZ_TEST_MENU} "File>Save As...")
set(ENV{FIREWORKZ_SCREENSHOT} ${SHOT}-save)

execute_process(COMMAND ${APP} RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 60)

if(NOT result EQUAL 0)
    message(FATAL_ERROR "fireworkz exited with ${result} saving a file\n${out}\n${err}")
endif()

if(NOT EXISTS ${SAVED})
    message(FATAL_ERROR "Save As did not write ${SAVED}\n${out}\n${err}")
endif()

file(STRINGS ${SAVED} first_line LIMIT_COUNT 1)
if(NOT first_line MATCHES "Fireworkz")
    message(FATAL_ERROR "${SAVED} is not a Fireworkz document: ${first_line}")
endif()

set(ENV{FIREWORKZ_TEST_MENU} "File>Open...")
set(ENV{FIREWORKZ_SCREENSHOT} ${SHOT}-open)

execute_process(COMMAND ${APP} RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 60)

if(NOT result EQUAL 0)
    message(FATAL_ERROR "fireworkz exited with ${result} opening a file\n${out}\n${err}")
endif()

if(NOT EXISTS ${SHOT}-open-1.png)
    message(FATAL_ERROR "opening ${SAVED} did not give a second window\n${out}\n${err}")
endif()

message(STATUS "fireworkz saved and reopened a document")

# Typing: click in the first cell, type some words, make one bold from the toolbar, find a word with the Find
# dialog (whose own buttons are pressed), save and look for the text in the saved file.

set(TYPED ${SHOT}-typed.fwk)
file(REMOVE ${TYPED})

set(ENV{FIREWORKZ_TEST_FILE} ${TYPED})
unset(ENV{FIREWORKZ_TEST_MENU})
unset(ENV{FIREWORKZ_SCREENSHOT})
unset(ENV{FIREWORKZ_SCREENSHOT_MS})
set(ENV{FIREWORKZ_TEST_SCRIPT} "wait:800;click:100,150;type:alpha bravo;key:Return;type:charlie;menu:Edit>Find...;wait:300;type:bravo;button:From Top;wait:300;key:Delete;menu:File>Save As...;wait:500;quit")

execute_process(COMMAND ${APP} RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 60)

if(NOT result EQUAL 0)
    message(FATAL_ERROR "fireworkz exited with ${result} while typing\n${out}\n${err}")
endif()

file(READ ${TYPED} typed_text)
if(NOT typed_text MATCHES "alpha " OR NOT typed_text MATCHES "charlie")
    message(FATAL_ERROR "the typed text is not in ${TYPED}:\n${typed_text}")
endif()
if(typed_text MATCHES "bravo")
    message(FATAL_ERROR "the word found and deleted is still in ${TYPED}:\n${typed_text}")
endif()

message(STATUS "fireworkz took typing, a dialog and Delete")

# The clipboard: copy a word and paste it (the program's own format, through the desktop's clipboard), then
# paste text that another program put there.

set(CLIPPED ${SHOT}-clipped.fwk)
file(REMOVE ${CLIPPED})

set(ENV{FIREWORKZ_TEST_FILE} ${CLIPPED})
set(ENV{FIREWORKZ_TEST_SCRIPT} "wait:800;click:100,150;type:ab;key:Home;key:shift+End;menu:Edit>Copy;key:End;menu:Edit>Paste;clip:xyz;menu:Edit>Paste;menu:File>Save As...;wait:500;quit")

execute_process(COMMAND ${APP} RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 60)

if(NOT result EQUAL 0)
    message(FATAL_ERROR "fireworkz exited with ${result} using the clipboard\n${out}\n${err}")
endif()

file(READ ${CLIPPED} clipped_text)
# the caret stays where a pasted fragment began, so the text from outside lands between the two ab
if(NOT clipped_text MATCHES "abxyzab")
    message(FATAL_ERROR "copy and paste did not give abxyzab in ${CLIPPED}:\n${clipped_text}")
endif()

message(STATUS "fireworkz copied and pasted")

# Printing: choose Print, press its Print button, with the output going to a PDF file instead of a printer
# (FIREWORKZ_PRINT_PDF); the file must be a PDF and, if pdftotext is to hand, hold the typed words.

set(PRINTED ${SHOT}-printed.pdf)
file(REMOVE ${PRINTED})

set(ENV{FIREWORKZ_PRINT_PDF} ${PRINTED})
set(ENV{FIREWORKZ_TEST_SCRIPT} "wait:800;click:100,150;type:zebra printing test;menu:File>Print...;wait:500;button:Print;wait:1500;quit")

execute_process(COMMAND ${APP} RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 60)
unset(ENV{FIREWORKZ_PRINT_PDF})

if(NOT result EQUAL 0)
    message(FATAL_ERROR "fireworkz exited with ${result} printing\n${out}\n${err}")
endif()

if(NOT EXISTS ${PRINTED})
    message(FATAL_ERROR "printing did not write ${PRINTED}\n${out}\n${err}")
endif()

file(READ ${PRINTED} pdf_head LIMIT 8)
if(NOT pdf_head MATCHES "^%PDF")
    message(FATAL_ERROR "${PRINTED} is not a PDF")
endif()

find_program(PDFTOTEXT pdftotext)
if(PDFTOTEXT)
    execute_process(COMMAND ${PDFTOTEXT} ${PRINTED} - OUTPUT_VARIABLE pdf_text)
    if(NOT pdf_text MATCHES "zebra printing test")
        message(FATAL_ERROR "the printed text is not in ${PRINTED}: ${pdf_text}")
    endif()
endif()

message(STATUS "fireworkz printed to a PDF")

# Draw files: insert the Fireworkz logo (a RISC OS Draw file, which has to be copied to a name with an extension)
# and take a picture; the picture is much bigger than that of the empty document if the logo was drawn.

set(LOGO ${SHOT}-logo.drw)
file(COPY_FILE ${CMAKE_CURRENT_LIST_DIR}/../../Resources/logos/fzc,aff ${LOGO})
file(REMOVE ${SHOT}-logoshot-0.png)

set(ENV{FIREWORKZ_TEST_FILE} ${LOGO})
set(ENV{FIREWORKZ_TEST_SCRIPT} "wait:800;click:60,150;menu:Edit>Insert>Picture...;wait:1500;shot:${SHOT}-logoshot;quit")

execute_process(COMMAND ${APP} RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 60)

if(NOT result EQUAL 0)
    message(FATAL_ERROR "fireworkz exited with ${result} drawing a Draw file\n${out}\n${err}")
endif()

file(SIZE ${SHOT}-logoshot-0.png logo_size)
if(logo_size LESS 30000)
    message(FATAL_ERROR "the picture with the logo in is only ${logo_size} bytes: the Draw file was not drawn")
endif()

message(STATUS "fireworkz drew a Draw file: ${logo_size} byte picture")
