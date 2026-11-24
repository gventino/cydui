add_library(unity STATIC ${CMAKE_SOURCE_DIR}/third_party/unity/unity.c)
target_include_directories(unity PUBLIC ${CMAKE_SOURCE_DIR}/third_party/unity)

# One executable per test file; each is registered with ctest under the same name.
add_library(testutil STATIC testutil.c)
target_link_libraries(testutil PUBLIC cydui cydui_mem)
target_include_directories(testutil PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(testutil PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
target_compile_options(testutil PRIVATE ${CYDUI_STRICT_FLAGS})

function(cydui_test name)
    add_executable(${name} ${name}.c ${ARGN})
    target_link_libraries(${name} PRIVATE testutil unity)
    set_target_properties(${name} PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
    target_compile_options(${name} PRIVATE ${CYDUI_STRICT_FLAGS})
    add_test(NAME ${name} COMMAND ${name})
endfunction()

cydui_test(test_smoke)
cydui_test(test_arena)
cydui_test(test_color)
cydui_test(test_ctx)
cydui_test(test_raster)

# Golden images: hashes live in tests/golden, PNGs for inspection go to <build>/golden.
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/golden)
add_library(golden STATIC golden.c)
target_link_libraries(golden PUBLIC testutil unity)
target_compile_definitions(golden PUBLIC GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden"
                                         GOLDEN_OUT_DIR="${CMAKE_BINARY_DIR}/golden")
set_target_properties(golden PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
target_compile_options(golden PRIVATE ${CYDUI_STRICT_FLAGS})
cydui_test(test_golden)
target_link_libraries(test_golden PRIVATE golden shapes_scene)

# Fuzzing (CT-12): the harness is also a short ctest run so that every build exercises it.
if(CYDUI_BUILD_FUZZ)
    add_executable(fuzz_cmds fuzz_cmds.c)
    target_link_libraries(fuzz_cmds PRIVATE cydui cydui_mem)
    target_compile_options(fuzz_cmds PRIVATE -fsanitize=fuzzer ${CYDUI_STRICT_FLAGS})
    target_link_options(fuzz_cmds PRIVATE -fsanitize=fuzzer)
    set_target_properties(fuzz_cmds PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
    add_test(NAME fuzz_cmds_smoke COMMAND fuzz_cmds -runs=3000 -max_len=1200)
endif()
