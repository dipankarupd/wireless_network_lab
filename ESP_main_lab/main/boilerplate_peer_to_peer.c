#include <esp_random.h>
#include <esp_log.h>
#include <reent.h> 
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/_intsup.h>

#include "esp_log.h"
#include "esp_mac.h"
#include "freertos/idf_additions.h"
#include "global_variables.h"
#include "printstate.h"
#include "ttm4138_communication.h"
#include "ttm4138_setup.h"
#include "ttm4138_utils.h"
#include "aead.h"
#include "hash.h"
#include "auth.h"

#define TIMEOUT_MS 30000
#define MAC_ADDR_LEN 6
#define TAG_LEN 16
#define KEY_LEN 16
//Split into to ascon nonce and wpa2 nonce
#define ASCON_NONCE_LEN 16
#define WPA2_NONCE_LEN 8

enum EStationState{
    S_STATE_1, //This is an example, you can change it with your own
    // ---- TODO Define your own states here ----
};

enum EApState{
    AP_STATE_1, //This is an example, you can change it with your own
    // ---- TODO Define your own states here ----
};

enum events{
    TIMEOUT, //Has to be 0(first in enum)
    RECIEVED_FRAME, //Has to be 0(first in enum)
    START_MACHINE,
    // Add more events here
};

typedef enum MessageType{
    HELLO_MESSAGE, //This is an example, you can change it with your own
    // ---- TODO Define your own message types here ----
} message_type_t;

// This is an example of a message, change it to your needs
typedef struct HelloMessage{
    message_type_t type;
    uint8_t indentifier[MAC_ADDR_LEN];
    uint8_t nonce[WPA2_NONCE_LEN];
    char protocol[6];
} hello_message_t;


// Add stucts for other message types needed by the program


// Initial variables, add more global state here
enum EStationState current_station_state = S_STATE_1;
enum EApState current_ap_state = AP_STATE_1;

void station_state_machine(event_t event) {
    switch (current_station_state) {
    // Add what happens at each state here
    case S_STATE_1:
        ESP_LOGI("SStateMachine", "S_STATE_1");
        switch (event.event) {
        // Add what happens at each event here
        case RECIEVED_FRAME:
            hello_message_t hello_message;
            message_type_t* m_type = (message_type_t*)event.frame->data;
            if (*m_type != HELLO_MESSAGE){
                ESP_LOGE("station_state_machine", "Invalid message type for state");
                 // What happens on error? Add you own code.
                current_station_state = ;
                break;
            }
            memcpy(&hello_message, event.frame->data, sizeof(hello_message));
             // Add logic for what happen in the state, and how the state changes
            break;
        case TIMEOUT:
            // What happens on timeout? Add you own code.
            ESP_LOGE("station_state_machine", "Timeout");
            current_station_state = ;
            break;
        }
        break;
    //  ---- ADD more states here ----
}

void accesspoint_state_machine(event_t event){
    switch (current_ap_state) {
    // Add what happens at each state here
    case AP_STATE_1:
        switch(event.event){
            case START_MACHINE:
            // Sending the initial message, feel free to modify as needed based on the contents of the first message
            ESP_LOGI("ApStateMachine", "AP_STATE_1");
            hello_message_t message;
            message.type = HELLO_MESSAGE;
            // Add the correct data to your message before sending
            transmit(remote_mac_addr, (uint8_t*) &message, length);
            ESP_LOGI("ApStateMachine", "AP_STATE_1, Send hello message");
            // Add logic for state transission
            current_ap_state = ;
            break;
        }
        break
    //  ---- ADD more states here ----
}

void fsm_task(void* arg) {
    event_t event;
    while (1) {
        if (xQueueReceive(fsm_event_queue, &event, pdMS_TO_TICKS(TIMEOUT_MS))) {
            xQueueReset(fsm_event_queue);

            if (device_communication_role == ROLE_INITIATOR) {
                accesspoint_state_machine(event);
            } else if (device_communication_role == ROLE_RESPONDER) {
                station_state_machine(event);
            }
            if (event.event == 1) {
                free(event.frame); // id 1
            }
        }
    }
}

void start_initiator_fsm() {
    event_t start_event = { .event = START_MACHINE };
    xQueueSend(fsm_event_queue, &start_event, 0);
}

void app_main(void) {
    ESP_LOGI("app_main", "Starting app main");
    strcpy(initiator_mac_addr_string, "aa:aa:aa:aa:aa:aa");
    strcpy(responder_mac_addr_string, "bb:bb:bb:bb:bb:bb");
    setup(1000, REC_UNICAST);
    ESP_LOGI("app_main", "Setup finished");
    switch (device_communication_role) {
    case ROLE_INITIATOR:
        start_initiator_fsm();
        xTaskCreate(fsm_task, "ApStateMachine", 4096, NULL, 10, NULL);
        break;
    case ROLE_RESPONDER:
        xTaskCreate(fsm_task, "SStateMachine", 4096, NULL, 10, NULL);
        break;
    default:
        ESP_LOGE("app_main", "Failed to get valid role");
        break;
    }
}


