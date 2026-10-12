# P2P protocol simulator. Needs only a C compiler (clang from Xcode CLT on macOS, gcc on Linux).
#
#   make                  build ./p2psim with the settings in p2p_config.h
#   make -B T=1000        rebuild with another number of rounds
#   make -B AEAD=0        rebuild with encryption off
#   make test             a few standard scenarios

ESP     = ../../ESP_main_lab
CORE    = $(ESP)/main/session6
AEADDIR = $(ESP)/components/aead

CC     ?= cc
CFLAGS += -std=gnu11 -Wall -Wextra -O2 -I. -I$(CORE) -I$(AEADDIR)/include -I$(AEADDIR)

ifdef T
CFLAGS += -DP2P_T=$(T)
endif
ifdef AEAD
CFLAGS += -DP2P_USE_AEAD=$(AEAD)
endif

SRCS = sim.c simkernel.c event_queue.c channel.c $(CORE)/p2p_fsm.c $(AEADDIR)/aead.c
DEPS = sim.h $(CORE)/p2p_fsm.h $(CORE)/p2p_config.h

p2psim: $(SRCS) $(DEPS)
	$(CC) $(CFLAGS) -o $@ $(SRCS)

test: p2psim
	./p2psim -s 1 -l 0 -d 0 -q
	./p2psim -n 1000 -s 1 -l 0.2 -d 0.05
	./p2psim -n 1000 -s 1 -l 0.4 -d 0.1 -c 0.1

clean:
	rm -f p2psim

.PHONY: test clean
