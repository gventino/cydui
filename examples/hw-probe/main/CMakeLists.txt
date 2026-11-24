idf_component_register(
    SRCS "hw_probe.c"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_spi esp_driver_gpio esp_driver_ledc esp_timer
)
