# P2P protocol simulator (Session 6, Appendix C)

A laptop simulator for the Session 6 P2P ping-pong protocol. It is built on the structure of the course simulator one folder up (`Prosim/`): a time-ordered event queue, a lossy channel, and a kernel loop. It adds what P2P needs:

- **Timers:** each party has one timer, and timeouts are scheduled events that can be stopped and restarted.
- **The real state machines:** it compiles `ESP_main_lab/main/session6/p2p_fsm.c`, the exact file that runs on the ESP32 boards, plus the real Ascon code from `components/aead`.
- **Channel faults and an attacker:** loss, duplication, bit errors, R powered on late, R rebooting, and an eavesdropper that replays recorded frames.

The course files in `Prosim/` are unchanged.

## Requirements

Only a C compiler: `clang` from the Xcode Command Line Tools on macOS (already installed if `cc --version` works), or `gcc` on Linux. Nothing else to download.

## Build and run

```sh
cd Prosim/p2p
make                 # builds ./p2psim from p2p_config.h settings
./p2psim -h          # options
```

| Command | What it shows |
|---|---|
| `./p2psim -l 0 -d 0` | The ideal path (no faults) |
| `./p2psim -s 7` | One run on the default channel: 20% loss, 5% duplicates |
| `./p2psim -l 0 -d 0 -c 0.5` | Bit errors caught by the Ascon tag, followed by resends |
| `./p2psim -l 0 -d 0 -o 5000` | R switched on 5 s late, so I resends until R answers |
| `./p2psim -l 0 -d 0 -b 25` | R reboots mid-session, sends ERROR, and I stops |
| `./p2psim -r 0.2` | Attacker replaying recorded frames |
| `./p2psim -n 10000 -l 0.4 -d 0.1` | 10,000 runs with different seeds, checking for deadlock, livelock and inconsistent endings |
| `make -B T=1000` | Rebuild with 1000 rounds (`AEAD=0` turns encryption off) |
| `make test` | Three standard checks |

Use the same `-s` seed to repeat a run exactly.

## Files

| File | Role |
|---|---|
| `sim.c` | Command-line options, single run or batch, consistency checks |
| `simkernel.c` | Creates I and R, runs the event loop, and implements the `plat_*` functions the state machines call |
| `event_queue.c` | Events ordered by time (FIFO for equal times); removes a party's pending timeout |
| `channel.c` | Loss, duplication, bit errors, R offline, attacker replay |
| `sim.h` | Shared types and configuration |

## What the simulator found

Every batch below ran 10,000 seeds. None ended in deadlock, livelock or an inconsistent state.

Three problems in the protocol were found and fixed before any board was flashed:

1. **R gave up before I did.** R abandoned a silent session after 5 × W = 10 s, while I keeps resending for 9 × W = 18 s. I's late resend then hit an idle R and the session died with an ERROR: 15 of 1000 runs at 20% loss. Fix: R now waits `P2P_MAX_RETRIES + 2` timeouts (20 s).
2. **A duplicate PING did not reset R's silence counter.** R counted silence from its last *accepted* PING, even though I's resends proved I was alive. Fix: reset the counter on a duplicate.
3. **A replayed old PING made R abort the session.** This was a cheap denial-of-service attack. Fix: in a session, R drops old or out-of-range PINGs instead of aborting. With 5% replays, failed sessions fell from about 12% to about 6%.

What remains:

- **About as many runs as the loss rate end with "R: ABANDONED (END lost)".** The final END is not acknowledged. This is the Two Generals problem: whoever sends the last message can never know that it arrived.
- **The remaining replay failures (42 of 43 checked) come from a replayed PING(n0) resetting R's session.** n0 is a fixed pre-shared value, so R cannot tell a replay from a genuine restart. A fresh random challenge per session, as in Session 7, fixes this.
- **On very lossy channels (40–60% loss), I eventually gives up with a timeout.** That is the intended behaviour.
