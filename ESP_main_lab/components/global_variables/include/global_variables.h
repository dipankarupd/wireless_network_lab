#include <stdint.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// check if already included
#ifndef GLOBAL_VARIABLES
#define GLOBAL_VARIABLES

// macros
#define MAC_ADDR_LEN 6
#define MAC_ADDR_STR_LEN 18
#define EVENT_QUEUE_SIZE 10
#define TAG_FSM "FSM"

// enums
typedef enum {
    ROLE_UNDEFINED,
    ROLE_INITIATOR,
    ROLE_RESPONDER,
    ROLE_SNIFFER
} communication_role_t;

typedef enum {
    RESULT_VALID_FORMAT,
    RESULT_INVALID_FORMAT,
    RESULT_TIMEOUT
} result_t;

typedef enum {
    REC_UNICAST,
    REC_PROMISCUOUS,
    REC_TARGET
} receive_mode_t;

// structs
typedef struct {
    uint16_t frame_ctrl;         // frame control field
    uint16_t duration_id;        // duration/id field
    uint8_t  addr1[MAC_ADDR_LEN]; // receiver address
    uint8_t  addr2[MAC_ADDR_LEN]; // transmitter address
    uint8_t  addr3[MAC_ADDR_LEN]; // bssid
    uint16_t seq_ctrl;           // sequence control field
    uint8_t  LLC_DSAP;
    uint8_t  LLC_SSAP;
    uint8_t  LLC_control;
    uint8_t  SNAP_OUI[3];
    uint8_t  SNAP_prot_ID[2];
    uint8_t  data[0];
} ieee80211_frame_t;

typedef struct {
    uint8_t            contains_frame;
    ieee80211_frame_t* frame;
    uint16_t           length;
} received_frame_info_t;

// global variables
extern uint8_t local_mac_addr[MAC_ADDR_LEN];   // local mac address
extern uint8_t remote_mac_addr[MAC_ADDR_LEN];  // remote mac address
extern uint8_t target_mac_addr[MAC_ADDR_LEN];  // remote mac address
extern char initiator_mac_addr_string[MAC_ADDR_STR_LEN];
extern char responder_mac_addr_string[MAC_ADDR_STR_LEN];
extern char sniffer_mac_addr_string[MAC_ADDR_STR_LEN];
extern char target_mac_addr_string[MAC_ADDR_STR_LEN];
extern QueueHandle_t fsm_event_queue;
extern communication_role_t device_communication_role;
extern TimerHandle_t timeout_timer;

#endif
