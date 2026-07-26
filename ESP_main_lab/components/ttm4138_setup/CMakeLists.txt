# set(COMPONENT_SRCS "global_variables.c")

# set(COMPONENT_ADD_INCLUDEDIRS ". include")

# register_component()
idf_component_register(
    SRCS "ttm4138_setup.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES esp_driver_gpio 
                  led_strip
                  nvs_flash
                  esp_wifi
                  global_variables
                  ttm4138_communication
                  ttm4138_utils
)
