# cydui — immediate-mode UI library for small embedded displays.
#
# Two modes:
#   * ESP-IDF component: picked up automatically via EXTRA_COMPONENT_DIRS / managed components.
#   * Plain CMake (PC): builds the core library, the backends, the examples and the tests.

set(CYDUI_CORE_SRCS src/arena.c src/ctx.c src/raster.c src/types.c src/version.c)

if(ESP_PLATFORM)
    idf_component_register(
        SRCS ${CYDUI_CORE_SRCS}
        INCLUDE_DIRS "include"
    )
    return()
endif()

cmake_minimum_required(VERSION 3.20)
project(cydui VERSION 0.0.0 LANGUAGES C)

option(CYDUI_BUILD_TESTS "Build the test suite" ON)
option(CYDUI_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
set(CYDUI_STRICT_FLAGS -pedantic -Wall -Wextra -Wshadow -Wcast-qual -Werror)

add_library(cydui STATIC ${CYDUI_CORE_SRCS})
target_include_directories(cydui PUBLIC include)
set_target_properties(cydui PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
target_compile_options(cydui PRIVATE ${CYDUI_STRICT_FLAGS})

if(CYDUI_SANITIZE)
    target_compile_options(cydui PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(cydui PUBLIC -fsanitize=address,undefined)
endif()

# In-memory backend: no dependencies, used by tests and headless runs.
add_library(cydui_mem STATIC backends/mem/cydui_mem.c)
target_include_directories(cydui_mem PUBLIC backends/mem)
target_link_libraries(cydui_mem PUBLIC cydui)
set_target_properties(cydui_mem PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
target_compile_options(cydui_mem PRIVATE ${CYDUI_STRICT_FLAGS})

if(CYDUI_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
