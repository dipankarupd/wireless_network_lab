/*
 * Filename: sim.h
 * Description: Discrete-event simulator for the Session 6 P2P protocol.
 *              Built on the structure of the course simulator in Prosim/
 *              (time-ordered event queue, lossy channel, kernel loop), with
 *              timers added and the REAL state machines from
 *              ESP_main_lab/main/session6/p2p_fsm.c plugged in.
 */
#ifndef P2P_SIM_H
#define P2P_SIM_H

#include <stdint.h>
#include <stdio.h>
#include "p2p_fsm.h"

#define MAX_EVENTS      512
#define SIM_PAYLOAD_MAX 64
#define PARTY_I         0
#define PARTY_R         1
#define EV_SIM_REBOOT   100   /* simulator-only event: R loses all its state */

typedef struct {
    uint32_t timestamp;                 /* simulated time in ms */
    int      target;                    /* PARTY_I or PARTY_R */
    uint8_t  event;                     /* EV_START / EV_FRAME / EV_TIMEOUT / EV_SIM_REBOOT */
    uint8_t  src_id[P2P_ID_LEN];        /* transmitter address (802.11 addr2) */
    uint16_t len;
    uint8_t  payload[SIM_PAYLOAD_MAX];
} sim_event_t;

typedef struct {
    double   loss;         /* probability a frame is lost */
    double   dup;          /* probability a frame is delivered twice */
    double   corrupt;      /* probability one bit of a frame is flipped */
    double   replay;       /* probability the attacker re-injects an old captured frame */
    uint32_t delay_ms;     /* channel delay per frame */
    uint32_t r_online_ms;  /* R hears nothing before this time ("R not ready") */
    uint32_t r_reboot_ms;  /* R loses all state at this time, 0 = never */
    int      verbose;      /* 0 quiet, 1 normal, 2 debug */
} sim_config_t;

typedef struct {
    const char*   i_outcome;
    const char*   r_outcome;
    int           i_state;
    int           r_state;
    uint32_t      i_rounds;
    uint32_t      r_rounds;
    uint32_t      end_ms;
    unsigned long events;
    int           hit_step_limit;
} sim_result_t;

extern uint32_t     simtime;
extern sim_config_t cfg;
extern p2p_party_t  parties[2];

/* event_queue.c */
void queue_reset(void);
int  queue_empty(void);
int  event_push(const sim_event_t* ev);
int  event_pop(sim_event_t* ev);
void queue_remove_timeouts(int target);

/* channel.c */
void   channel_reset(void);
void   channel_send(int from, int to, const uint8_t* src_id, const uint8_t* data, uint16_t len);
double sim_rand01(void);

/* simkernel.c */
void         sim_log(int level, const char* who, const char* fmt, ...) __attribute__((format(printf, 3, 4)));
sim_result_t run_simulation(unsigned seed);

#endif
