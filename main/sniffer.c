// standard C library headers
#include <string.h>

// ESP headers
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// project headers
#include "global_variables.h"
#include "ttm4138_setup.h"
#include "ttm4138_utils.h"
#include "ttm4138_communication.h"

#define TIMEOUT_MS 3000

void app_main(void) {

    // Uncomment to listen to all packets
    setup_promiscuous(TIMEOUT_MS);

    // Uncomment to listen to traffci to/from a specified device
    // setup_target(TIMEOUT_MS, "d0:65:78:db:8f:2b");

    xTaskCreate(print_frame, "print_frame", 4096, NULL, 10, NULL);
}
