add_executable(hello main.c)
target_link_libraries(hello PRIVATE cydui cydui_sdl cydui_fonts)
set_target_properties(hello PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
target_compile_options(hello PRIVATE ${CYDUI_STRICT_FLAGS})
