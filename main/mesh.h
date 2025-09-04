#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

void mesh_init_start(void);
bool mesh_is_root(void);
void mesh_rx_loop(void); // blocking loop
esp_err_t mesh_send_json_to_root(const char *json, size_t len);
esp_err_t mesh_broadcast_json(const char *json, size_t len);
uint64_t mesh_get_node_mac64(void);