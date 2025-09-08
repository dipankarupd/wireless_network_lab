#include <stdint.h>
#include <esp_wifi.h>
#include <esp_err.h>

void configure_wifi(receive_mode_t receive_mode);
esp_err_t transmit(uint8_t dest_addr[], uint8_t data[], uint16_t data_size);
void print_frame(void* arg);
void fsm_callback_unicast(void* buf, wifi_promiscuous_pkt_type_t type);
void fsm_callback_promiscious(void* buf, wifi_promiscuous_pkt_type_t type);
void fsm_callback_targeted_listen(void* buf, wifi_promiscuous_pkt_type_t type);
