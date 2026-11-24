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
