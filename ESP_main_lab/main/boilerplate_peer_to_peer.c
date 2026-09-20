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



enum ResponderState{
    RESPONDER_IDLE, //This is an example, you can change it with your own
    RECIEVED_1,
    SENDT_2,
    RECIEVED_3,
    SEND_4,
    RESPONDER_END,
    // ---- TODO Define your own states here ----
};


// This enum contains all different states that your state machine can be in
enum InitiatorState{
    INITIATOR_IDLE, //This is an example, you can change it with your own
    SENDT_1, //This is an example, you can change it with your own
    RECIEVED_2, //This is an example, you can change it with your own
    SENDT_3, //This is an example, you can change it with your own
    RECIEVED_4, //This is an example, you can change it with your own
    INITIATOR_END, //This is an example, you can change it with your own
    // ---- TODO Define your own states here ----
};

// This enum contains all different events that your state_machine handles
typedef enum events{
    //STATIC Internal
    TIMEOUT, //Has to be 0 (first in enum)
    //STATIC EVENTS,  EXTERNAL
    RECIEVED_FRAME, //Has to be 1(secound in enum)
    //EXTERNAL EVENTS
    //
    //Internal events, Change them according to your needs
    START,
    MESSAGE_FAILED,
    MESSAGE_VALID,
    TERMINATE
    // Add more events here
    // ---- TODO Define your own events here ----
} event_type_t;

// Do not change
typedef struct {
    event_type_t       event_type;
    // Protocol events are added when the ESP recieves a packet, those will contain the frame that was recieved.
    ieee80211_frame_t* frame;
    uint16_t           length;
} protocol_event_t;

typedef enum MessageType{
    M1, //This is an example, you can change it with your own
    M2, 
    M3, 
    M4, 
    // ---- TODO Define your own message types here ----
} message_type_t;

// This is an example of a message, change it to your needs
typedef struct M_Message{
    message_type_t type;
    //Add more data for the message here
} M_t;

// Add stucts for other message types needed by the program


// Initial variables, add more global state here
enum ResponderState current_responder_state = RESPONDER_IDLE;
enum InitiatorState current_initiator_state = INITIATOR_IDLE;
uint32_t counter = 0;

void responder_state_machine(protocol_event_t event){
    switch (current_responder_state) {
    // Add what happens at each state here
    case RESPONDER_IDLE:
        switch (event.event_type) {
            case START:
                //What happens when it encounters an unexpected event
                break;
            case RECIEVED_FRAME:
                message_type_t* m_type = (message_type_t*)event.frame->data;
                if (*m_type != M1){
                    ESP_LOGE("responder_state_machine", "Invalid message type for state");
                     // What happens on error? Add you own code.
                    protocol_event_t wrong_packet_event = { .event_type =  MESSAGE_FAILED};
                    xQueueSend(fsm_event_queue, &wrong_packet_event, 0);
                    current_responder_state = RECIEVED_1;
                    break;
                }
                protocol_event_t correct_packet_event = { .event_type =  MESSAGE_VALID};
                xQueueSend(fsm_event_queue, &correct_packet_event, 0);
                current_responder_state = RECIEVED_1;
                break;
            case MESSAGE_FAILED:
                //What happens when it encounters an unexpected event
                break;
        }
        break;
    }

}

void initiator_state_machine(protocol_event_t event){
    switch (current_initiator_state) {
    // Add what happens at each state here
    case INITIATOR_IDLE:
        switch(event.event_type){
            case START:
                M_t m1 = {.type = M1};
                transmit(remote_mac_addr, (uint8_t*) &m1, sizeof(m1));
                current_initiator_state = SENDT_1;
                start_timeout_timer();
                break;
            case RECIEVED_FRAME:
                //What happens when it encounters an unexpected event
                break;
            case TIMEOUT:
                //What happens when it encounters an unexpected event
                break;
            case MESSAGE_FAILED:
                //What happens when it encounters an unexpected event
                break;
            case MESSAGE_VALID:
                //What happens when it encounters an unexpected event
                break;
            case TERMINATE:
                //What happens when it encounters an unexpected event
                break;
        }
        break;
    case SENDT_1:
        switch (event.event_type) {
            case START:
                break;
            case RECIEVED_FRAME:
                xTimerStop(timeout_timer, 0);
                //Checks if the message in the event is the expected one.
                message_type_t* m_type = (message_type_t*)event.frame->data;
                if (*m_type != M2){
                    ESP_LOGE("initiator_state_machine", "Invalid message type for state");
                    protocol_event_t wrong_packet_event = { .event_type =  MESSAGE_FAILED};
                    xQueueSend(fsm_event_queue, &wrong_packet_event, 0);
                    current_initiator_state = RECIEVED_2;
                    break;
                }
                protocol_event_t correct_packet_event = { .event_type =  MESSAGE_VALID};
                xQueueSend(fsm_event_queue, &correct_packet_event, 0);
                current_initiator_state = RECIEVED_2;
                break;
            case TIMEOUT:
                //TODO implement locic for timeout, what should happen
                ESP_LOGI("Idle STATE","Packet timed out");
                start_timeout_timer();
                break;
            case MESSAGE_FAILED:
                //What happens when it encounters an unexpected event
                break;
            case MESSAGE_VALID:
                //What happens when it encounters an unexpected event
                break;
            case TERMINATE:
                //What happens when it encounters an unexpected event
                break;
        }
        break;
    }
    //  ---- ADD more states here ----
}

void fsm_event_dispatcher(void* arg) {
    protocol_event_t event;
    while (1) {
        if (xQueueReceive(fsm_event_queue, &event, pdMS_TO_TICKS(100))) {

            if (device_communication_role == ROLE_INITIATOR) {
                initiator_state_machine(event);
            } else if (device_communication_role == ROLE_RESPONDER) {
                responder_state_machine(event);
            }
            if (event.event_type == RECIEVED_FRAME) {
                free(event.frame); // id 1
            }
        }
    }
}


void app_main(void) {
    ESP_LOGI("app_main", "Starting app main");
    strcpy(initiator_mac_addr_string, "aa:aa:aa:aa:aa:aa");
    strcpy(responder_mac_addr_string, "bb:bb:bb:bb:bb:bb");
    setup(TIMEOUT_MS, REC_UNICAST);
    ESP_LOGI("app_main", "Setup finished");
    // The variable device_communication_role is based set on setup, it is created by a comperison between the mac address of the device and initiator_mac_addr_string/responder_mac_addr_string
    // Add a start event to the event queue
    protocol_event_t start_event = { .event_type = START };
    switch (device_communication_role) {
    case ROLE_INITIATOR:
        xQueueSend(fsm_event_queue, &start_event, 0);
        xTaskCreate(fsm_event_dispatcher, "InitiatorStateMachine", 4096, NULL, 10, NULL);
        break;
    case ROLE_RESPONDER:
        xTaskCreate(fsm_event_dispatcher, "ResponderStateMachine", 4096, NULL, 10, NULL);
        break;
    default:
        ESP_LOGE("app_main", "Failed to get valid role");
        break;
    }
}


