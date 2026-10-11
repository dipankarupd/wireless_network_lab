/*
 * Filename: simkernel.c
 * Description: Simulation kernel: creates I and R, seeds the START event,
 *              dispatches events to the P2P state machines, and implements the
 *              plat_* functions that p2p_fsm.c calls (the ESP32 versions of the
 *              same functions are in ESP_main_lab/main/session6/p2p_protocol.c).
 */
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "sim.h"

#define MAX_STEPS 2000000UL   /* guard against livelock: a correct run ends long before */

sim_config_t cfg = {
    .loss = 0.2, .dup = 0.05, .corrupt = 0.0, .replay = 0.0,
    .delay_ms = 5, .r_online_ms = 0, .r_reboot_ms = 0, .verbose = 1,
};

p2p_party_t parties[2];

static const uint8_t MAC_I[P2P_ID_LEN] = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x01 };
static const uint8_t MAC_R[P2P_ID_LEN] = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x02 };

static int index_of(const p2p_party_t* p) {
    return p == &parties[PARTY_I] ? PARTY_I : PARTY_R;
}

void sim_log(int level, const char* who, const char* fmt, ...) {
    if (level > cfg.verbose) {
        return;
    }
    va_list args;
    va_start(args, fmt);
    printf("[%7lu ms] %-7s    ", (unsigned long)simtime, who);
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
}

/* ================================================================== */
/* Platform layer used by p2p_fsm.c                                    */
/* ================================================================== */

void plat_send(p2p_party_t* p, const uint8_t* data, uint16_t len) {
    int from = index_of(p);
    channel_send(from, 1 - from, p->local_id, data, len);
}

void plat_timer_start(p2p_party_t* p) {
    int t = index_of(p);
    queue_remove_timeouts(t);   /* one timer per party: restart = stop + start */
    sim_event_t ev = { .timestamp = simtime + P2P_TIMEOUT_MS, .target = t, .event = EV_TIMEOUT };
    event_push(&ev);
}

void plat_timer_stop(p2p_party_t* p) {
    queue_remove_timeouts(index_of(p));
}

void plat_indicate(p2p_party_t* p) {
    sim_log(2, p->tag, "(LED changes colour)");
}

void plat_random(uint8_t* buf, uint16_t len) {
    for (uint16_t i = 0; i < len; i++) {
        buf[i] = (uint8_t)(rand() & 0xFF);
    }
}

uint32_t plat_now_ms(void) {
    return simtime;
}

void plat_delay_ms(uint32_t ms) {
    (void)ms;   /* not simulated: it would block the single event loop */
}

void plat_log(const p2p_party_t* p, char level, const char* fmt, ...) {
    int lv = (level == 'D') ? 2 : 1;
    if (lv > cfg.verbose) {
        return;
    }
    va_list args;
    va_start(args, fmt);
    printf("[%7lu ms] %-7s %c  ", (unsigned long)simtime, p->tag, level);
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
}

/* ================================================================== */
/* Kernel loop                                                         */
/* ================================================================== */

sim_result_t run_simulation(unsigned seed) {
    srand(seed);
    queue_reset();
    channel_reset();
    p2p_party_init(&parties[PARTY_I], P2P_INITIATOR, "P2P-I", MAC_I, MAC_R);
    p2p_party_init(&parties[PARTY_R], P2P_RESPONDER, "P2P-R", MAC_R, MAC_I);

    sim_event_t start = { .timestamp = 0, .target = PARTY_I, .event = EV_START };
    event_push(&start);
    if (cfg.r_reboot_ms) {
        sim_event_t reboot = { .timestamp = cfg.r_reboot_ms, .target = PARTY_R, .event = EV_SIM_REBOOT };
        event_push(&reboot);
    }

    unsigned long steps = 0;
    sim_event_t ev;
    while (!queue_empty() && steps < MAX_STEPS) {
        event_pop(&ev);
        steps++;
        p2p_party_t* p = &parties[ev.target];

        switch (ev.event) {
        case EV_SIM_REBOOT:
            sim_log(1, "SIM", "*** R reboots: all its protocol state is lost ***");
            queue_remove_timeouts(PARTY_R);
            p2p_party_init(p, P2P_RESPONDER, "P2P-R", MAC_R, MAC_I);
            break;
        case EV_FRAME:
            p2p_handle_event(p, EV_FRAME, ev.src_id, ev.payload, ev.len);
            break;
        default:
            p2p_handle_event(p, ev.event, NULL, NULL, 0);
            break;
        }
    }

    sim_result_t r = {
        .i_outcome = parties[PARTY_I].outcome,
        .r_outcome = parties[PARTY_R].outcome,
        .i_state   = parties[PARTY_I].i_state,
        .r_state   = parties[PARTY_R].r_state,
        .i_rounds  = parties[PARTY_I].rounds,
        .r_rounds  = parties[PARTY_R].rounds,
        .end_ms    = simtime,
        .events    = steps,
        .hit_step_limit = steps >= MAX_STEPS,
    };
    return r;
}
