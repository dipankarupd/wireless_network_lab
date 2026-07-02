/*
 * Filename: simkernel.c
 * Description: This is the kernel of the protocol simulator. 
 * It declares the roles, generates the initial state and starting event,
 * then run the simulation loop by dispatching events to the FSMs.
 * Author: Stig F. Mjølsnes, NTNU
 * Date: 2026-07-01
 * Version: 1.0
 */

#include "sim.h"

int run_simulator(void) {
    int tick = 0;
    initiator_t initiator_ctx;
    responder_t responder_ctx;

    init_initiator(&initiator_ctx);
    init_responder(&responder_ctx);

    printf("Ready to run, press return key\n");
    getchar();

    /* Seed the simulation with an initial start event for the initiator. */
    event_t startevent = {
        .timestamp = simtime,
        .id = EV_START,
        .target = INITIATOR_ID,
        .frame = { .msg_id = EV_START, .sender = 0, .receiver = 0, .seq = 0 }
    };

    event_push(startevent);

    event_t event;
    while (eventswaiting()) {
        event_pop(&event);  // Get the next event from the queue and advance the simulation time

        switch (event.target) { // Dispatch the event to the appropriate FSM based on the target role
            case INITIATOR_ID:
                initiator_fsm(&initiator_ctx, event);
                break;
            case RESPONDER_ID:
                responder_fsm(&responder_ctx, event);
                break;
            default:
                break;
        }

        sleep(1); // Simulate time passing between events, here set to 1 second for demonstration purposes
        printf("Tick %d \n", tick++); // Displays the loop is live
    }

    return 0;
}
