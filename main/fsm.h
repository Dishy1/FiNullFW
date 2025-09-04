#pragma once
#include <stdint.h>

// Simple-state for uplink: 0 normal, 1 fire, 2 expiry
uint8_t fsm_simple_state(void);
uint8_t fsm_get_state(void);
void    fsm_set_state(uint8_t st);
void    fsm_loop(void);