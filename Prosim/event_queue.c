/*
 * Filename: event_queue.c
 * Description: Implements the priority event queue used to store and dispatch simulator events.
 * Author: Stig F. Mjølsnes
 * Date: 2026-07-01
 * Version: 1.0
 */

#include "sim.h"

uint32_t simtime = 0;
event_t elist[MAX_EVENTS];
int currentsize = 0;

int eventswaiting(void) {
    return (currentsize > 0);
}

// Inserts a new event into the queue in timestamp order
int event_push(event_t new) {
    /* Insert the event in timestamp order so the earliest event is serviced first. */
    if (currentsize >= MAX_EVENTS) {
        printf("List is full. %d events\n", currentsize);
        return 0;
    }
    // Find the correct position to insert the new event based on its timestamp
    int pos = 0;
    while (pos < currentsize && elist[pos].timestamp < new.timestamp) {
        pos++;
    }
    // Shift existing events to make room for the new event
    memmove(&elist[pos + 1], &elist[pos], (currentsize - pos) * sizeof(event_t));
    elist[pos] = new;
    currentsize++;
    return 1;
}

//Extracts the next event from the queue 
int event_pop(event_t *next) {
    if (currentsize == 0) {
        printf("List is empty.\n");
        next = NULL;
        return 0;
    }

    *next = elist[0];
    simtime = next->timestamp;  //Jump simulation time to the timestamp of the event being popped
    memmove(&elist[0], &elist[1], (currentsize - 1) * sizeof(event_t)); //Remaining events shift one position forward
    currentsize--;
    return 1;
}
