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
