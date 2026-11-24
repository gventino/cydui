# cydui — immediate-mode UI library for small embedded displays.
#
# Two modes:
#   * ESP-IDF component: picked up automatically via EXTRA_COMPONENT_DIRS / managed components.
#   * Plain CMake (PC): builds the core library, the backends, the examples and the tests.

set(CYDUI_CORE_SRCS src/arena.c src/ctx.c src/font.c src/raster.c src/text.c src/types.c src/ui.c src/utf8.c src/version.c)

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
option(CYDUI_BUILD_EXAMPLES "Build the PC examples" ON)
option(CYDUI_BUILD_SDL "Build the SDL2 backend (needs SDL2)" ON)
option(CYDUI_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
# libFuzzer needs clang; instrument the library too so the fuzzer sees its coverage.
if(CMAKE_C_COMPILER_ID STREQUAL "Clang")
    option(CYDUI_BUILD_FUZZ "Build the libFuzzer harness (clang only)" ON)
else()
    set(CYDUI_BUILD_FUZZ OFF)
endif()

set(CYDUI_STRICT_FLAGS -pedantic -Wall -Wextra -Wshadow -Wcast-qual -Werror)

add_library(cydui STATIC ${CYDUI_CORE_SRCS})
target_include_directories(cydui PUBLIC include)
# The PC builds always have the debug checks (id collisions, thread check), whatever the build type.
target_compile_definitions(cydui PUBLIC CYDUI_DEBUG_IDS=1 CYDUI_DEBUG_THREAD=1)
set_target_properties(cydui PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
target_compile_options(cydui PRIVATE ${CYDUI_STRICT_FLAGS})

if(CYDUI_BUILD_FUZZ)
    target_compile_options(cydui PRIVATE -fsanitize=fuzzer-no-link)
endif()

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

# Bundled bitmap fonts (SIL OFL, see fonts/LICENSE-OFL.txt). Optional and kept out of the core.
add_library(cydui_fonts STATIC fonts/cydui_sans_12.c fonts/cydui_sans_16.c fonts/cydui_sans_24.c)
target_include_directories(cydui_fonts PUBLIC fonts)
target_link_libraries(cydui_fonts PUBLIC cydui)
set_target_properties(cydui_fonts PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
target_compile_options(cydui_fonts PRIVATE ${CYDUI_STRICT_FLAGS})

# SDL2 backend: window and mouse.
if(CYDUI_BUILD_SDL)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(SDL2 QUIET IMPORTED_TARGET sdl2)
    endif()
    if(SDL2_FOUND)
        add_library(cydui_sdl STATIC backends/sdl/cydui_sdl.c)
        target_include_directories(cydui_sdl PUBLIC backends/sdl)
        target_link_libraries(cydui_sdl PUBLIC cydui PkgConfig::SDL2)
        set_target_properties(cydui_sdl PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON
                                                   C_EXTENSIONS OFF)
        target_compile_options(cydui_sdl PRIVATE ${CYDUI_STRICT_FLAGS})
    else()
        message(STATUS "SDL2 not found: building without the SDL backend")
    endif()
endif()

if(CYDUI_BUILD_EXAMPLES OR CYDUI_BUILD_TESTS)
    add_subdirectory(examples/shapes) # the golden test reuses its scene
    add_subdirectory(examples/text)
endif()

if(CYDUI_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
