cmake_minimum_required(VERSION 3.16)

# The library is the repository itself: one component with the core and the ESP32 backend.
get_filename_component(CYDUI_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(EXTRA_COMPONENT_DIRS ${CYDUI_ROOT})

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(theme-gallery)
