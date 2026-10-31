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
