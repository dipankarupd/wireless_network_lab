idf_component_register(SRCS "test.c"
                    PRIV_REQUIRES auth aead ecc hash ttm4138_ecc ttm4138_communication ttm4138_utils global_variables ttm4138_setup
                    INCLUDE_DIRS "."
                    REQUIRES nvs_flash driver esp_wifi esp_driver_gpio)

