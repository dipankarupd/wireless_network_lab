// standard C library headers
#include <string.h>

// ESP headers
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>

// project headers
#include "global_variables.h"
#include "ttm4138_setup.h"
#include "ttm4138_utils.h"
#include "ttm4138_communication.h"

#define TIMEOUT_MS 3000

void app_main(void) {

    setup_promiscuous(1000);
    while (1) {
        wait_for_button_press();
        cycle_light();
        ESP_LOGI("test.c", "Cycling light");
    }

}
