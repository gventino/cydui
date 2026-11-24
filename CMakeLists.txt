# cydui — immediate-mode UI library for small embedded displays.
#
# Two modes:
#   * ESP-IDF component: picked up automatically via EXTRA_COMPONENT_DIRS / managed components.
#   * Plain CMake (PC): builds the core library and, optionally, the tests.

if(ESP_PLATFORM)
    idf_component_register(
        SRCS "src/version.c"
        INCLUDE_DIRS "include"
    )
    return()
endif()

cmake_minimum_required(VERSION 3.20)
project(cydui VERSION 0.0.0 LANGUAGES C)

option(CYDUI_BUILD_TESTS "Build the test suite" ON)
option(CYDUI_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

add_library(cydui STATIC src/version.c)
target_include_directories(cydui PUBLIC include)
set_target_properties(cydui PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
target_compile_options(cydui PRIVATE -pedantic -Wall -Wextra -Werror)

if(CYDUI_SANITIZE)
    target_compile_options(cydui PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(cydui PUBLIC -fsanitize=address,undefined)
endif()

if(CYDUI_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
