/*
 * Filename: fsm.c
 * Description: Contains the initiator and responder finite-state machine logic for the protocol simulation.
 * Author: Stig F. Mjølsnes, NTNU
 * Date: 2026-07-01
 * Version: 1.0
 */

#include "sim.h"

void init_initiator(initiator_t *ctx) {
    /* Reset the initiator state before the simulation begins. */
    ctx->state = SI_READY;
    ctx->seq = 0;
    ctx->retries = 0;
}

void init_responder(responder_t *ctx) {
    /* Reset the responder state before the simulation begins. */
    ctx->state = SR_IDLE;
    ctx->seq = 0;
    ctx->retries = 0;
}

void initiator_fsm(initiator_t *ctx, event_t event) {
    /* Drive the initiator state machine based on incoming events. */
    switch (ctx->state) {
        case SI_READY:
            if (event.id == EV_START) {
                printf("Initiator received START\n");
                frame_t hello = {
                    .msg_id = EV_MSG_HELLO,
                    .sender = INITIATOR_ID,
                    .receiver = RESPONDER_ID,
                    .seq = 1,
                };
                memcpy(hello.payload, "howdoyoudo", 11);
                send_frame(&hello);
                ctx->state = SI_WAITCHALL;
            }
            break;

        case SI_WAITCHALL:
            if (event.id == EV_MSG_CHALLENGE) {
                printf("Initiator received CHALLENGE\n");
                frame_t resp = {
                    .msg_id = EV_MSG_RESPONSE,
                    .sender = INITIATOR_ID,
                    .receiver = RESPONDER_ID,
                    .seq = 1,
                };
                memcpy(resp.payload, "Itakeyouon", 10);
                send(INITIATOR_ID, RESPONDER_ID, &resp);
                ctx->state = SI_WAITCONFIRM;
            }
            break;

        case SI_WAITCONFIRM:
            if (event.id == EV_MSG_CONFIRM) {
                printf("Initiator received CONFIRM\n");
                printf("Authentication success\n");
                ctx->state = SI_ACCEPTED;
            }
            break;

        case SI_ACCEPTED:
        case SI_FAILED:
        default: printf("Initiator in state %d received unexpected event %d\n", ctx->state, event.id);
            break;
    }
}

void responder_fsm(responder_t *ctx, event_t event) {
    /* Drive the responder state machine based on incoming events. */
    switch (ctx->state) {
        case SR_IDLE:
            if (event.id == EV_MSG_HELLO) {
                printf("Responder received HELLO\n");
                frame_t challenge = {
                    .msg_id = EV_MSG_CHALLENGE,
                    .sender = RESPONDER_ID,
                    .receiver = INITIATOR_ID,
                    .seq = 1,
                };
                memcpy(challenge.payload, "Idareyou", 8);
                send(RESPONDER_ID, INITIATOR_ID, &challenge);
                ctx->state = SR_WAITRESP;
            }
            break;

        case SR_WAITRESP:
            if (event.id == EV_MSG_RESPONSE) {
                printf("Responder received RESPONSE\n");
                frame_t confirm = {
                    .msg_id = EV_MSG_CONFIRM,
                    .sender = RESPONDER_ID,
                    .receiver = INITIATOR_ID,
                    .seq = 1,
                };
                memcpy(confirm.payload, "Accessadmitted", 15);
                send(RESPONDER_ID, INITIATOR_ID, &confirm);
                ctx->state = SR_CHECKDONE;
            }
            break;

        case SR_CHECKDONE:
        case SR_ACCESS:
        case SR_REJECT:
        default: printf("Responder in state %d received unexpected event %d\n", ctx->state, event.id);
            break;
    }
}
