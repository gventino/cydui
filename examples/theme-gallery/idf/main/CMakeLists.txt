# The application chooses its fonts and themes (FR-10): only what it uses is compiled in.
get_filename_component(CYDUI_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../.." ABSOLUTE)
get_filename_component(CYDUI_COMPONENT "${CYDUI_ROOT}" NAME)

idf_component_register(
    SRCS app_main.c
         ${CYDUI_ROOT}/examples/theme-gallery/gallery.c
         ${CYDUI_ROOT}/examples/theme-gallery/rating.c
         ${CYDUI_ROOT}/themes/clean.c
         ${CYDUI_ROOT}/fonts/cydui_sans_16.c
    INCLUDE_DIRS . ${CYDUI_ROOT}/examples/theme-gallery ${CYDUI_ROOT}/themes ${CYDUI_ROOT}/fonts
    REQUIRES ${CYDUI_COMPONENT} nvs_flash esp_timer esp_system
)
# CT-13: format string checks for our code (the stack protector and the heap poisoning are in sdkconfig.defaults)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wformat -Wformat-security)
