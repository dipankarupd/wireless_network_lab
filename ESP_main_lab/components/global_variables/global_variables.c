// standard C library headers
#include <stdint.h>

// ESP headers
// #include <freertos/queue.h>

// project headers
#include "global_variables.h"

// global variables
uint8_t local_mac_addr[MAC_ADDR_LEN];   // local mac address
uint8_t remote_mac_addr[MAC_ADDR_LEN];  // remote mac address
uint8_t target_mac_addr[MAC_ADDR_LEN];  // remote mac address
char initiator_mac_addr_string[] = "00:00:00:00:00:00";
char responder_mac_addr_string[] = "00:00:00:00:00:00";
char sniffer_mac_addr_string[] = "00:00:00:00:00:00";
char target_mac_addr_string[] = "00:00:00:00:00:00";
QueueHandle_t fsm_event_queue;
TimerHandle_t timeout_timer;
communication_role_t device_communication_role = ROLE_UNDEFINED;
