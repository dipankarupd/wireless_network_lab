idf_component_register(
    SRCS "ttm4138_communication.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES esp_driver_gpio 
                  led_strip
                  nvs_flash
                  esp_wifi
                  global_variables
)

