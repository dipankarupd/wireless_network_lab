/*
 * Filename: channel.c
 * Description: Simulates the communication channel, including frame loss and duplication.
 * Author: Stig F. Mjølsnes, NTNU
 * Date: 2026-07-01
 * Version: 1.0
 */

#include "sim.h"
// Simulates sending a frame from sender to receiver, with potential loss and duplication.
void send(uint8_t sender, uint8_t receiver, frame_t *f) {
    (void)sender;
    (void)receiver;
    send_frame(f);
}
//
void send_frame(frame_t *f) {
    /* Simulate a lossy and occasionally duplicated communication channel. */
    // TODO: The imperfection parameters should be easier to configure, e.g., via command line arguments or a configuration file. 

    float loss_probability = 0.2f;
    float duplicate_probability = 0.05f;
    float r = (float)arc4random() / (float)UINT32_MAX;

    if (r < loss_probability) {
        printf("Channel: dropped frame\n");
        return;
    }

    event_t ev;
    ev.timestamp = simtime;
    ev.id = (event_id_t)f->msg_id;
    ev.target = f->receiver;
    ev.frame = *f;

    event_push(ev);
// Simulate frame duplication with a certain probability
    r = (float)arc4random() / (float)UINT32_MAX;
    if (r < duplicate_probability) {
        printf("Channel: duplicated frame\n");
        event_push(ev);
    }
}
