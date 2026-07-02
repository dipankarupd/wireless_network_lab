/*
 * Filename: sim.c

 * Compilegcc -Wall -Wextra -std=c99 -o sim sim.c simkernel.c fsm.c channel.c event_queue.c
 * Description: Minimal wrapper that calls the simulator entry point from the modular source files.
 * Author: Stig F. Mjølsnes, NTNU
 * Date: 2026-07-01
 * Version: 1.0
 */

#include "sim.h"

int main(void) {
    return run_simulator();
}

