// standard C library headers
#include <stdint.h>

// ESP headers
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <driver/gpio.h>
#include <led_strip.h>

// project headers
#include "global_variables.h"
#include "ttm4138_utils.h"

// global variables
uint8_t light_state = 0; // used for cycling LED color
led_strip_handle_t led_strip;


/**
 * @brief Initialize and configure the onboard LED strip.
 *
 * Sets up the LED strip driver with RMT configuration and clears the LED.
 */
void configure_led(void) {
    static const led_strip_config_t strip_config = {
        .strip_gpio_num = STRIP_GPIO,
        .max_leds = 1, // at least one LED on board
    };
    static const led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .flags.with_dma = false,
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    led_strip_clear(led_strip);
    led_strip_refresh(led_strip);
}

/**
 * @brief Wait for a button press and release.
 *
 * Blocks until the button is pressed (logic low) and then released.
 * Assumes button press on button marked BOOT (GPIO 9).
 */
void wait_for_button_press(void) {
    int key_pressed = 0;
    while (1) {
        int button_state = gpio_get_level(GPIO_NUM_9); // register keypress
        if (button_state == 0 && key_pressed == 0) {
            key_pressed = 1;
        } else if (button_state != 0 && key_pressed == 1) {
            // return on button release
            return;
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

/**
 * @brief Print bytes in hexadecimal format.
 *
 * @param start   Pointer to the byte array.
 * @param length  Number of bytes to print.
 */
void print_bytes(uint8_t* start, uint16_t length) {
    for (int i = 0; i < length; i++) {
        printf("%02X ", *(start + i));
    }
    printf("\n");
}

/**
 * @brief Convert MAC address string to byte array.
 *
 * Parses a MAC address in format "xx:xx:xx:xx:xx:xx" into 6-byte array.
 *
 * @param mac      Output byte array (6 bytes).
 * @param str_mac  Input MAC address string in hex.
 *
 * @return 1 on success, 0 on failure.
 */
int strmac_to_mac(uint8_t mac[MAC_ADDR_LEN], char* str_mac) {
    unsigned int temp[6];
    if (6 == sscanf(str_mac, "%x:%x:%x:%x:%x:%x",
        &temp[0], &temp[1], &temp[2], &temp[3], &temp[4], &temp[5]))
    {
        for (int i = 0; i < 6; i++) {
            mac[i] = (uint8_t)temp[i];
        }
        return 1;  // success
    }
    return 0;  // failure
}

/**
 * @brief Cycle the LED through red, green, and blue states.
 *
 * Updates the LED color based on the current light_state and advances it.
 * Useful for visualizing state changes.
 */
void cycle_light() {
    uint8_t num_states = 3;
    if (light_state == 0){
        led_strip_set_pixel(led_strip, 0 , 100, 0, 0);
        led_strip_refresh(led_strip);
        light_state = (light_state+1) % num_states;
    }
    else if (light_state == 1){
        led_strip_set_pixel(led_strip, 0 , 0, 100, 0);
        led_strip_refresh(led_strip);
        light_state = (light_state+1) % num_states;
    }
    else if (light_state == 2){
        led_strip_set_pixel(led_strip, 0 , 0, 0, 100);
        led_strip_refresh(led_strip);
        light_state = (light_state+1) % num_states;
    }
}

/**
 * @brief Timeout callback that sends a timeout event to the FSM queue.
 *
 * Sends a frame info struct with contains_frame = 0 to signal a timeout.
 */
void on_timeout() {
    event_t received_frame_info = { .event = 0 };
    xQueueSend(fsm_event_queue, &received_frame_info, 0);
}

/**
 * @brief (Re)start the timeout timer.
 */
void start_timeout_timer() {
    xTimerStop(timeout_timer, 0);
    xTimerStart(timeout_timer, 0);
}
