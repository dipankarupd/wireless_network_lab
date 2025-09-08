#include <stdint.h>
#include "global_variables.h"

// macros
#define STRIP_GPIO 8

// function declarations
void configure_led(void);
void cycle_light(void);
void print_bytes(uint8_t* start, uint16_t length);
int strmac_to_mac(uint8_t mac[MAC_ADDR_LEN], char* str_mac);
ieee80211_frame_t* create_frame(uint8_t dest_addr[], uint8_t* data, uint16_t data_size);
void on_timeout();
void start_timeout_timer();
void wait_for_button_press(void);