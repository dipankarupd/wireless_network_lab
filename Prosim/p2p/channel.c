/*
 * Filename: channel.c
 * Description: The radio channel between I and R, with the "acts of nature"
 *              (loss, duplication, bit errors, R not powered yet) and one
 *              intentional attack: an eavesdropper that records every frame
 *              and replays old ones later.
 */
#include <stdlib.h>
#include <string.h>
#include "sim.h"

#define CAPTURE_MAX 16

static sim_event_t captured[CAPTURE_MAX];   /* what the attacker has recorded */
static int ncaptured = 0;

static const char* who(int party) {
    return party == PARTY_I ? "I" : "R";
}

double sim_rand01(void) {
    return rand() / ((double)RAND_MAX + 1.0);
}

void channel_reset(void) {
    ncaptured = 0;
}

void channel_send(int from, int to, const uint8_t* src_id, const uint8_t* data, uint16_t len) {
    if (len > SIM_PAYLOAD_MAX) {
        sim_log(1, "CHANNEL", "frame too long (%u bytes), dropped", len);
        return;
    }

    sim_event_t ev = { .timestamp = simtime + cfg.delay_ms, .target = to, .event = EV_FRAME, .len = len };
    memcpy(ev.src_id, src_id, P2P_ID_LEN);
    memcpy(ev.payload, data, len);

    /* The eavesdropper hears every frame on the air. */
    captured[ncaptured % CAPTURE_MAX] = ev;
    ncaptured++;

    if (to == PARTY_R && ev.timestamp < cfg.r_online_ms) {
        sim_log(1, "CHANNEL", "%s->%s lost: R is not powered on yet", who(from), who(to));
        return;
    }
    if (sim_rand01() < cfg.loss) {
        sim_log(1, "CHANNEL", "%s->%s frame lost", who(from), who(to));
        return;
    }
    if (sim_rand01() < cfg.corrupt) {
        int bit = rand() % (len * 8);
        ev.payload[bit / 8] ^= (uint8_t)(1u << (bit % 8));
        sim_log(1, "CHANNEL", "%s->%s bit %d flipped", who(from), who(to), bit);
    }
    event_push(&ev);

    if (sim_rand01() < cfg.dup) {
        ev.timestamp += 1;
        event_push(&ev);
        sim_log(1, "CHANNEL", "%s->%s frame duplicated", who(from), who(to));
    }

    if (ncaptured > 1 && sim_rand01() < cfg.replay) {
        int pick = rand() % (ncaptured < CAPTURE_MAX ? ncaptured : CAPTURE_MAX);
        sim_event_t old = captured[pick];
        old.timestamp = simtime + cfg.delay_ms + 1 + (uint32_t)(rand() % 500);
        event_push(&old);
        sim_log(1, "ATTACK", "replaying an old captured frame to %s", who(old.target));
    }
}
