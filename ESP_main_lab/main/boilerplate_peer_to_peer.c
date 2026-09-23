/* --------------------------------------------------------------------------------------------
TTM4138 Wireless Security lab project
NTNU Autumn semester 2026
Version 21.september
			----- BOILERPLATE/TEMPLATE -------
This code combines both the Initiator and the Responder program 
into a single file.  If you find it more convenient, you may split this into
two separate programs.  

The main_ap() starts the program, and that is where the MAC-addresses of
the Initiator and the Responder must be identified and distinguished. 

The call to setup() hides much of the complications of setting up the communication channel.
For instance, the global variable device_communication_role is assigned in the setup(),
where the function determines the role to be run by comparing the actual mac address of the device 
with the initiator_mac_addr_string and responder_mac_addr_string variables defined in app_main().   
================================================================================================*/ 


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


/* ---- TODO Define your own states here ----
These two enums shall contain all the states of your state machines
Naming the states makes the source code easier to read 
These are examples, make your own according to your FSM diagram
*/
enum InitiatorState{
    I_IDLE, 
    I_SENT_1, 
    I_RECEIVED_2, 
    I_SENT_3, 
    I_RECEIVED_4, 
    I_END, 
};

enum ResponderState{
    R_IDLE, 
    R_RECEIVED_1,
    R_SENT_2,
    R_RECEIVED_3,
    R_SENT_4,
    R_END,
};

/* ---- TODO Define your own events here ----
  An event shall initiate a state transition.
  Naming the events makes the source code easier to read
  These are examples, make your own according to your FSM diagram
  This enum contains all different events that your state_machine must handle.
  The two events TIMEOUT and RECEIVED_FRAME must listed first in this implementation
*/

typedef enum events{
    TIMEOUT, //Has to be label 0, but considered to be internal event
    RECEIVED_FRAME, //Has to be label 1, but considered to be external event
    
//List additional external event names here

//List additional internal event names here, such as
    START,
    MESSAGE_FAILED,
    MESSAGE_VALID,
    TERMINATE
} event_type_t;

/* -----Do not modify-----
   The RECEIVED_FRAME event is put in the event queue when a frame is received 
   This event-type will contain the received frame.
*/
    
typedef struct {
    event_type_t       event_type;
    ieee80211_frame_t* frame;
    uint16_t           length;
} protocol_event_t;

/* ---- TODO Define your own message types here ----
List the message types and declare the message structs
 you want to define. Simple examples given here
*/
typedef enum MessageType{
    M1, 
    M2, 
    M3, 
    M4, 
    // 
} message_type_t;

typedef struct M_Message{
    message_type_t type;
    //Add more fields for the message here
} M_t;



/* Initialisation of global variables
	Set the initial states 
    More global state needed can go here 
*/
enum InitiatorState current_initiator_state = I_IDLE;
enum ResponderState current_responder_state = R_IDLE;
uint32_t counter = 0;


/* ---- TODO The Initiator FSM code ----  
	Structured as a nested switch/case code
	The action happens within each event case handler
 
*/
void initiator_state_machine(protocol_event_t event){
    switch (current_initiator_state) {
    // Add what happens at each state here
    case I_IDLE:
        switch(event.event_type){
            case START:
                M_t m1 = {.type = M1};
                transmit(remote_mac_addr, (uint8_t*) &m1, sizeof(m1));
                current_initiator_state = SENT_1;
                start_timeout_timer();
                break;
            case RECEIVED_FRAME:
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
    case I_SENT_1:
        switch (event.event_type) {
            case START:
                break;
            case RECEIVED_FRAME:
                xTimerStop(timeout_timer, 0);
                //Checks if the message in the event is the expected one.
                message_type_t* m_type = (message_type_t*)event.frame->data;
                if (*m_type != M2){
                    ESP_LOGE("initiator_state_machine", "Invalid message type for state");
                    protocol_event_t wrong_packet_event = { .event_type =  MESSAGE_FAILED};
                    xQueueSend(fsm_event_queue, &wrong_packet_event, 0);
                    current_initiator_state = RECEIVED_2;
                    break;
                }
                protocol_event_t correct_packet_event = { .event_type =  MESSAGE_VALID};
                xQueueSend(fsm_event_queue, &correct_packet_event, 0);
                current_initiator_state = RECEIVED_2;
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


/* ---- TODO The Responder FSM code ----  
	Structured as a nested switch/case code
	The action happens within each event case handler
*/
void responder_state_machine(protocol_event_t event){
    switch (current_responder_state) {
    // Add what happens at each state here
    case R_IDLE:
        switch (event.event_type) {
            case START:
                //What happens when it encounters an unexpected event
                break;
            case RECEIVED_FRAME:
                message_type_t* m_type = (message_type_t*)event.frame->data;
                if (*m_type != M1){
                    ESP_LOGE("responder_state_machine", "Invalid message type for state");
                     // What happens on error? Add you own code.
                    protocol_event_t wrong_packet_event = { .event_type =  MESSAGE_FAILED};
                    xQueueSend(fsm_event_queue, &wrong_packet_event, 0);
                    current_responder_state = RECEIVED_1;
                    break;
                }
                protocol_event_t correct_packet_event = { .event_type =  MESSAGE_VALID};
                xQueueSend(fsm_event_queue, &correct_packet_event, 0);
                current_responder_state = RECEIVED_1;
                break;
            case MESSAGE_FAILED:
                //What happens when it encounters an unexpected event
                break;
        }
        break;
    }

}

/* This process is spawned in app_main().  It pops events from the event queue and
    dispatches it to the FSM handling.
*/
void fsm_event_dispatcher(void* arg) {
    protocol_event_t event;
    while (1) {
        if (xQueueReceive(fsm_event_queue, &event, pdMS_TO_TICKS(100))) {

            if (device_communication_role == ROLE_INITIATOR) {
                initiator_state_machine(event);
            } else if (device_communication_role == ROLE_RESPONDER) {
                responder_state_machine(event);
            }
            if (event.event_type == RECEIVED_FRAME) {
                free(event.frame); // id 1
            }
        }
    }
}


void app_main(void) {
    ESP_LOGI("app_main", "Starting app main");
    
//Define you Initiator and Responder devices here
    strcpy(initiator_mac_addr_string, "aa:aa:aa:aa:aa:aa");
    strcpy(responder_mac_addr_string, "bb:bb:bb:bb:bb:bb");
    
    setup(TIMEOUT_MS, REC_UNICAST);  // Much happens under the hood here
    ESP_LOGI("app_main", "Setup finished");
    
    // Push a start event to the event queue, this could be a button on the board event
    protocol_event_t start_event = { .event_type = START };
    
/* The variable device_communication_role is assigned in the setup(),
where it determines the role by comparing the actual mac address of the device 
with the initiator_mac_addr_string and responder_mac_addr_string variables defined above.   
*/
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


