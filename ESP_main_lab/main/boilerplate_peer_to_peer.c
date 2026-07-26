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

typedef enum MessageType{
    HELLO_MESSAGE, //This is an example, you can change it with your own
    // ---- TODO Define your own message types here ----
} message_type_t;

typedef struct ResultType {
    result_t result;
    message_type_t msg_type;
} result_type_t;


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


result_type_t parse_frame(received_frame_info_t frame_info) {
    // protocol1_data_t* data = (protocol1_data_t*)frame_info.frame->data;
    message_type_t* m_type = (message_type_t*)frame_info.frame->data;
    result_type_t result_type = {.msg_type = *m_type};
    switch (*m_type) {
    //Write your own cases for each message type
    case HELLO_MESSAGE:
        hello_message_t* hello_message = (hello_message_t*)frame_info.frame->data;
        if (memcmp(&(hello_message->indentifier), remote_mac_addr, MAC_ADDR_LEN) != 0) {
            ESP_LOGE("parse_frame()", "Incorrect indentifier in packet");
            result_type.result = RESULT_INVALID_FORMAT;
            break;
        }
        if (memcmp(&(hello_message->protocol), "ASCON", MAC_ADDR_LEN) != 0) {
            ESP_LOGE("parse_frame()", "Invalid encryption type %s in packet, expected ASCON", hello_message->protocol);
            result_type.result = RESULT_INVALID_FORMAT;
            break;
        }
        result_type.result = RESULT_VALID_FORMAT;
        break;
    // ---- Casses for more message types here ----
    }

    return result_type;
}



void station_state_machine(result_type_t result_type, received_frame_info_t received_frame_info) {
    switch (current_station_state) {
    // Add what happens at each state here
    case S_STATE_1:
        ESP_LOGI("SStateMachine", "S_STATE_1");
        switch (result_type.result) {
        case RESULT_VALID_FORMAT:
            hello_message_t hello_message;
            if (result_type.msg_type != HELLO_MESSAGE){
                ESP_LOGE("station_state_machine", "Invalid message type for state");
                // What happens on error? Add you own code.
                current_station_state = ;
                break;
            }
            memcpy(&hello_message, received_frame_info.frame->data, sizeof(hello_message));
            // Add logic for what happen in the state, and how the state changes
            break;
        case RESULT_INVALID_FORMAT:
            ESP_LOGE("station_state_machine", "Invalid format");
            // What happens on error? Add you own code.
            current_station_state = ;
            break;
        case RESULT_TIMEOUT:
            // What happens on timeout? Add you own code.
            ESP_LOGE("station_state_machine", "Timeout");
            current_station_state = ;
            break;
        }
        break;
    //  ---- ADD more states here ----
    }
}

void accesspoint_state_machine(result_type_t result_type, received_frame_info_t received_frame_info){
    switch (current_ap_state) {
    // Add what happens at each state here
    case AP_STATE_1:
        // Sending the initial message, feel free to modify as needed based on the contents of the first message
        ESP_LOGI("ApStateMachine", "AP_STATE_1");
        hello_message_t message;
        message.type = HELLO_MESSAGE;
        // Add the correct data to your message before sending
        transmit(remote_mac_addr, (uint8_t*) &message, length);
        start_timeout_timer();
        ESP_LOGI("ApStateMachine", "AP_STATE_1, Send hello message");
        // Add logic for state transission
        current_ap_state = ;
        break;
    }
}

void fsm_task(void* arg) {
    received_frame_info_t received_frame_info;
    result_type_t result_type;
    while (1) {
        if (xQueueReceive(fsm_event_queue, &received_frame_info, pdMS_TO_TICKS(TIMEOUT_MS))) {
            xTimerStop(timeout_timer, 0);
            xQueueReset(fsm_event_queue);
            if (received_frame_info.contains_frame == 0) { // timeout
                result_type.result = RESULT_TIMEOUT;
            } else {
                result_type = parse_frame(received_frame_info);
            }

            if (device_communication_role == ROLE_INITIATOR) {
                accesspoint_state_machine(result_type, received_frame_info);
            } else if (device_communication_role == ROLE_RESPONDER) {
                station_state_machine(result_type, received_frame_info);
            }
            if (received_frame_info.contains_frame) {
                free(received_frame_info.frame); // id 1
            }
        }
    }
}

void start_initiator_fsm() {
    received_frame_info_t dummy = { .contains_frame = 0 };
    result_type_t result_type= {.msg_type = HELLO_MESSAGE};
    accesspoint_state_machine(result_type,dummy);
}

void app_main(void) {
    ESP_LOGI("app_main", "Running session 8 WAP2 handshake");
    strcpy(initiator_mac_addr_string, "aa:aa:aa:aa:aa:aa"); // Change with the correct mac addresses
    strcpy(responder_mac_addr_string, "bb:bb:bb:bb:bb:bb"); // Change with the correct mac addresses
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


