#include "comms.h"
#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "cJSON.h"
#include "mesh.h"
#include "fsm.h"
#include "sensors.h"
#include "config.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_mac.h"

static const char *TAG = "COMMS";

static node_info_t s_nodes[MAX_NODES];
static size_t s_nodes_count = 0;

static uint64_t now_ms(void){ return esp_timer_get_time()/1000ULL; }

static int find_or_add_node_by_mac(const uint8_t mac[6]){
    for (size_t i=0;i<s_nodes_count;i++){
        if (memcmp(s_nodes[i].mac, mac, 6)==0) return (int)i;
    }
    if (s_nodes_count < MAX_NODES){
        memcpy(s_nodes[s_nodes_count].mac, mac, 6);
        s_nodes[s_nodes_count].online = false;
        s_nodes[s_nodes_count].last_seen_ms = 0;
        return (int)s_nodes_count++;
    }
    return -1;
}

void comms_send_periodic(void){
    // 60초마다 Periotic 메세지 전송 - Minimal
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", "heartbeat");
    cJSON_AddStringToObject(root, "hwid", config_get_hwid());
    cJSON_AddStringToObject(root, "expiry", config_get_expiry());
    cJSON_AddNumberToObject(root, "temp", sensors_get_temperature());
    cJSON_AddNumberToObject(root, "simple_state", fsm_simple_state());
    cJSON_AddNumberToObject(root, "fsm_state", fsm_get_state());

    char *out = cJSON_PrintUnformatted(root);
    if (mesh_is_root()){
        uint8_t mac[6]; esp_read_mac(mac, ESP_MAC_WIFI_STA);
        comms_on_rx_json(out, strlen(out), mac);

    } else {
        mesh_send_json_to_root(out, strlen(out));
    }
    cJSON_free(out);
    cJSON_Delete(root);
}

void comms_on_rx_json(const char *json, int len, const uint8_t mac[6]){
    (void)len;
    cJSON *root = cJSON_Parse(json);
    if (!root) return;
    const cJSON *type = cJSON_GetObjectItem(root, "type");
    if (type && strcmp(type->valuestring, "heartbeat") == 0){
        int idx = find_or_add_node_by_mac(mac);
        if (idx>=0){
            node_info_t *n = &s_nodes[idx];
            const cJSON *hwid = cJSON_GetObjectItem(root, "hwid");
            const cJSON *expiry = cJSON_GetObjectItem(root, "expiry");
            const cJSON *temp = cJSON_GetObjectItem(root, "temp");
            const cJSON *ss = cJSON_GetObjectItem(root, "simple_state");
            const cJSON *fs = cJSON_GetObjectItem(root, "fsm_state");
            if (hwid && cJSON_IsString(hwid)) strncpy(n->hwid, hwid->valuestring, sizeof(n->hwid));
            if (expiry && cJSON_IsString(expiry)) strncpy(n->expiry, expiry->valuestring, sizeof(n->expiry));
            if (temp && cJSON_IsNumber(temp)) n->temperature = (float)temp->valuedouble;
            if (ss && cJSON_IsNumber(ss)) n->simple_state = (uint8_t)ss->valueint;
            if (fs && cJSON_IsNumber(fs)) n->fsm_state = (uint8_t)fs->valueint;
            n->last_seen_ms = now_ms();
            n->online = true;
        }
    }
    cJSON_Delete(root);
}

const node_info_t *comms_get_nodes(size_t *out_count){
    // 노드 리스트를 반환하고, 90초 이상 안보인 노드는 온라인 상태를 false로 설정
    uint64_t t = now_ms();
    for (size_t i=0;i<s_nodes_count;i++){
        if (t - s_nodes[i].last_seen_ms > 90000ULL) s_nodes[i].online = false;
    }
    *out_count = s_nodes_count;
    return s_nodes;
}

// HTTP 요청 클라이언트에 /api/nodes 형태로 노드 리스트 전송 (CORS 포함)
esp_err_t comms_send_nodes_to_client(httpd_req_t *req){
    // CORS
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
    httpd_resp_set_hdr(req, "Access-Control-Max-Age", "600");

    size_t n; const node_info_t *arr = comms_get_nodes(&n);
    cJSON *root = cJSON_CreateArray();
    for (size_t i=0;i<n;i++){
        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "type", "heartbeat");
        cJSON_AddStringToObject(o, "hwid", arr[i].hwid);
        cJSON_AddStringToObject(o, "expiry", arr[i].expiry);
        cJSON_AddNumberToObject(o, "temp", arr[i].temperature);
        cJSON_AddNumberToObject(o, "simple_state", arr[i].simple_state);
        cJSON_AddNumberToObject(o, "fsm_state", arr[i].fsm_state);
        cJSON_AddItemToArray(root, o);
    }
    char *out = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, out);
    cJSON_free(out); cJSON_Delete(root);
    return ESP_OK;
}
