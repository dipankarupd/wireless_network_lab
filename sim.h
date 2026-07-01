/*
 * Filename: sim.h
 * Description: Shared declarations for the protocol simulator, 
 * including state types, event structures, and function prototypes.
 * Author: Stig F. Mjølsnes, NTNU
 * Date: 2026-07-01
 * Version: 1.0
 */

#ifndef SIM_H
#define SIM_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MAX_EVENTS 20
#define INITIATOR_ID 1
#define RESPONDER_ID 2

typedef enum {
    SI_READY,
    SI_WAITCHALL,
    SI_WAITCONFIRM,
    SI_ACCEPTED,
    SI_FAILED
} initiator_state_t;

typedef struct {
    initiator_state_t state;
    uint32_t seq;
    int retries;
} initiator_t;

typedef enum {
    SR_IDLE,
    SR_WAITRESP,
    SR_CHECKDONE,
    SR_ACCESS,
    SR_REJECT
} responder_state_t;

typedef struct {
    responder_state_t state;
    uint32_t seq;
    int retries;
} responder_t;

typedef union {
    responder_state_t resp_role;
    initiator_state_t init_role;
    uint8_t raw;
} state_t;

typedef enum {
    EV_START,
    EV_TIMEOUT,
    EV_ERROR,
    EV_MSG_HELLO,
    EV_MSG_CHALLENGE,
    EV_MSG_RESPONSE,
    EV_MSG_CONFIRM,
    EV_MSG_REJECT
} event_id_t;

typedef struct {
    event_id_t msg_id;
    uint8_t sender;
    uint8_t receiver;
    uint32_t seq;
    uint8_t payload[32];
} frame_t;

typedef struct {
    uint32_t timestamp;
    event_id_t id;
    uint8_t target;
    frame_t frame;
} event_t;

extern uint32_t simtime;
extern event_t elist[MAX_EVENTS];
extern int currentsize;

void init_initiator(initiator_t *ctx);
void init_responder(responder_t *ctx);
void initiator_fsm(initiator_t *ctx, event_t event);
void responder_fsm(responder_t *ctx, event_t event);
void send_frame(frame_t *f);
void send(uint8_t sender, uint8_t receiver, frame_t *f);
int eventswaiting(void);
int event_push(event_t new);
int event_pop(event_t *next);
int run_simulator(void);

#endif
