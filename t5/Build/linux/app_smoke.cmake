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
