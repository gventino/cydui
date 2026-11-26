# The application chooses its fonts and themes: only what it uses is compiled in.
get_filename_component(CYDUI_ROOT "@CYDUI_PATH@" ABSOLUTE)
get_filename_component(CYDUI_COMPONENT "${CYDUI_ROOT}" NAME)

idf_component_register(
    SRCS main.c
         ${CYDUI_ROOT}/fonts/cydui_sans_16.c
    INCLUDE_DIRS . ${CYDUI_ROOT}/fonts
    REQUIRES ${CYDUI_COMPONENT} nvs_flash esp_timer
)
