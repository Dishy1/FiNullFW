#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
#include "esp_http_server.h"

#define MAX_NODES 32

typedef struct {
    char hwid[32];
    char expiry[16];      // YYYY.MM.DD
    float temperature;    // 최신 온도
    uint8_t simple_state; // 0=normal,1=fire,2=expiry
    uint8_t fsm_state;    // 0..6
    bool online;
    uint64_t last_seen_ms;
    uint8_t mac[6];
} node_info_t;

void comms_send_periodic(void);
void comms_on_rx_json(const char *json, int len, const uint8_t mac[6]);
const node_info_t *comms_get_nodes(size_t *out_count);

//CORS + 노드 전송
esp_err_t comms_send_nodes_to_client(httpd_req_t *req);
