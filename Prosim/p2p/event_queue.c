/*
 * Filename: event_queue.c
 * Description: Time-ordered event queue (as in the course simulator), plus
 *              removal of pending timeouts so a party can stop/restart its timer.
 *              Events with equal timestamps are served first-in first-out.
 */
#include <string.h>
#include "sim.h"

uint32_t simtime = 0;

static sim_event_t elist[MAX_EVENTS];
static int currentsize = 0;

void queue_reset(void) {
    currentsize = 0;
    simtime = 0;
}

int queue_empty(void) {
    return currentsize == 0;
}

int event_push(const sim_event_t* ev) {
    if (currentsize >= MAX_EVENTS) {
        sim_log(1, "QUEUE", "event list full, event dropped");
        return 0;
    }
    int pos = 0;
    while (pos < currentsize && elist[pos].timestamp <= ev->timestamp) {
        pos++;
    }
    memmove(&elist[pos + 1], &elist[pos], (currentsize - pos) * sizeof(sim_event_t));
    elist[pos] = *ev;
    currentsize++;
    return 1;
}

int event_pop(sim_event_t* ev) {
    if (currentsize == 0) {
        return 0;
    }
    *ev = elist[0];
    simtime = ev->timestamp;   /* jump simulated time to this event */
    memmove(&elist[0], &elist[1], (currentsize - 1) * sizeof(sim_event_t));
    currentsize--;
    return 1;
}

void queue_remove_timeouts(int target) {
    int out = 0;
    for (int i = 0; i < currentsize; i++) {
        if (!(elist[i].event == EV_TIMEOUT && elist[i].target == target)) {
            elist[out++] = elist[i];
        }
    }
    currentsize = out;
}
