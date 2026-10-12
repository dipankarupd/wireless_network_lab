/*
 * Filename: sim.c
 * Description: Command-line front end for the P2P protocol simulator.
 *
 *   ./p2psim                      one run, default channel (20% loss, 5% duplicates)
 *   ./p2psim -l 0 -d 0            perfect channel: the ideal path
 *   ./p2psim -c 0.2               20% of frames get a bit error
 *   ./p2psim -r 0.1               attacker replays old frames
 *   ./p2psim -o 5000              R is switched on 5 s after I starts
 *   ./p2psim -b 3000              R reboots (loses its state) at t = 3 s
 *   ./p2psim -n 1000 -l 0.3       1000 runs with different seeds, checks invariants
 *
 * Build: make        (see Makefile; protocol settings come from p2p_config.h)
 */
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "sim.h"

#define MAX_KINDS 16

static void usage(const char* prog) {
    printf("usage: %s [options]\n"
           "  -l p    frame loss probability          (default 0.2)\n"
           "  -d p    frame duplication probability   (default 0.05)\n"
           "  -c p    bit-error probability per frame (default 0)\n"
           "  -r p    attacker replay probability     (default 0)\n"
           "  -D ms   channel delay per frame         (default 5)\n"
           "  -o ms   R is powered on at this time    (default 0)\n"
           "  -b ms   R reboots at this time          (default never)\n"
           "  -s n    random seed                     (default: time)\n"
           "  -n n    number of runs; >1 runs quietly and checks invariants\n"
           "  -q      quiet (summary only)   -v  debug output\n", prog);
}

static const char* name_or(const char* s, const char* fallback) {
    return s ? s : fallback;
}

/* Returns NULL if the run is consistent, otherwise a description of the violation. */
static const char* check_run(const sim_result_t* r) {
    if (r->hit_step_limit) return "livelock: step limit reached";
    if (r->i_state != I_DONE && r->i_state != I_ERROR) return "deadlock: I is still waiting with nothing scheduled";
    if (r->r_state != R_IDLE) return "deadlock: R is still in a session with nothing scheduled";
    int i_ok = r->i_outcome && strcmp(r->i_outcome, "SUCCESS") == 0;
    int r_ok = r->r_outcome && strcmp(r->r_outcome, "SUCCESS") == 0;
    if (i_ok && r->i_rounds != P2P_T) return "I reports SUCCESS with the wrong number of rounds";
    if (r_ok && r->r_rounds != P2P_T) return "R reports SUCCESS with the wrong number of rounds";
    if (r_ok && !i_ok) return "R finished successfully but I did not";
    return NULL;
}

int main(int argc, char** argv) {
    unsigned seed = (unsigned)time(NULL);
    int runs = 1;
    int opt;
    while ((opt = getopt(argc, argv, "l:d:c:r:D:o:b:s:n:qvh")) != -1) {
        switch (opt) {
        case 'l': cfg.loss = atof(optarg); break;
        case 'd': cfg.dup = atof(optarg); break;
        case 'c': cfg.corrupt = atof(optarg); break;
        case 'r': cfg.replay = atof(optarg); break;
        case 'D': cfg.delay_ms = (uint32_t)atol(optarg); break;
        case 'o': cfg.r_online_ms = (uint32_t)atol(optarg); break;
        case 'b': cfg.r_reboot_ms = (uint32_t)atol(optarg); break;
        case 's': seed = (unsigned)strtoul(optarg, NULL, 10); break;
        case 'n': runs = atoi(optarg); break;
        case 'q': cfg.verbose = 0; break;
        case 'v': cfg.verbose = 2; break;
        default:  usage(argv[0]); return opt == 'h' ? 0 : 2;
        }
    }

    printf("P2P simulator: T=%d n0=%lu AEAD=%s W=%dms retries=%d R-patience=%dxW | "
           "loss=%.2f dup=%.2f biterr=%.2f replay=%.2f delay=%lums",
           P2P_T, (unsigned long)P2P_N0, P2P_USE_AEAD ? "on" : "off", P2P_TIMEOUT_MS,
           P2P_MAX_RETRIES, P2P_RESPONDER_IDLE_TIMEOUTS,
           cfg.loss, cfg.dup, cfg.corrupt, cfg.replay, (unsigned long)cfg.delay_ms);
    if (cfg.r_online_ms) printf(" R-on@%lums", (unsigned long)cfg.r_online_ms);
    if (cfg.r_reboot_ms) printf(" R-reboot@%lums", (unsigned long)cfg.r_reboot_ms);
    printf("\n\n");

    if (runs <= 1) {
        sim_result_t r = run_simulation(seed);
        const char* bad = check_run(&r);
        printf("\n---- result (seed %u) ----\n", seed);
        printf("I: %-45s rounds %lu/%d\n", name_or(r.i_outcome, "(still running)"), (unsigned long)r.i_rounds, P2P_T);
        printf("R: %-45s rounds %lu/%d\n", name_or(r.r_outcome, "(no session completed)"), (unsigned long)r.r_rounds, P2P_T);
        printf("simulated time %lu ms, %lu events, consistency check: %s\n",
               (unsigned long)r.end_ms, r.events, bad ? bad : "OK");
        return bad ? 1 : 0;
    }

    /* Batch mode: many seeds, tally the outcome pairs and check invariants. */
    struct { const char* i; const char* r; int count; } kinds[MAX_KINDS];
    int nkinds = 0, violations = 0;
    cfg.verbose = 0;
    for (int k = 0; k < runs; k++) {
        sim_result_t r = run_simulation(seed + (unsigned)k);
        const char* bad = check_run(&r);
        if (bad) {
            if (violations < 5) printf("VIOLATION seed %u: %s\n", seed + (unsigned)k, bad);
            violations++;
        }
        const char* io = name_or(r.i_outcome, "(still running)");
        const char* ro = name_or(r.r_outcome, "(no session completed)");
        int j;
        for (j = 0; j < nkinds; j++) {
            if (strcmp(kinds[j].i, io) == 0 && strcmp(kinds[j].r, ro) == 0) break;
        }
        if (j == nkinds && nkinds < MAX_KINDS) {
            kinds[nkinds].i = io; kinds[nkinds].r = ro; kinds[nkinds].count = 0; nkinds++;
        }
        if (j < MAX_KINDS) kinds[j].count++;
    }
    printf("%d runs (seeds %u..%u)\n", runs, seed, seed + (unsigned)runs - 1);
    printf("  %6s  %-36s %s\n", "runs", "initiator outcome", "responder outcome");
    for (int j = 0; j < nkinds; j++) {
        printf("  %6d  %-36s %s\n", kinds[j].count, kinds[j].i, kinds[j].r);
    }
    printf("invariant violations (deadlock, livelock, inconsistent end): %d\n", violations);
    return violations ? 1 : 0;
}
