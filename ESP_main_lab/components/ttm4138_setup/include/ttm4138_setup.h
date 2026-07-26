#include <stdint.h>
#include "global_variables.h"

void setup(uint16_t timeout_ms, receive_mode_t receive_mode);
void setup_promiscuous(uint16_t timeout_ms);
void setup_target(uint16_t timeout_ms, char* target);
void setup_unicast(uint16_t timeout_ms, char* peer);
void setup_responder(uint16_t timeout_ms, char* receiver);
void setup_initiator(uint16_t timeout_ms, char* receiver);
