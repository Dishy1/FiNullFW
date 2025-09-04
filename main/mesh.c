#include "mesh.h"
#include <string.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_mesh.h"
#include "esp_mesh_internal.h"
#include "nvs_flash.h"
#include "comms.h"

static const char *TAG = "MESH";
static bool s_is_root = false;
static mesh_addr_t s_parent_addr;

static void wifi_init(void) {
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
    ESP_ERROR_CHECK(esp_wifi_start());
}

static void mesh_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    switch (event_id) {
        case MESH_EVENT_STARTED: {
            ESP_LOGI(TAG, "MESH_EVENT_STARTED");
            break;
        }
        case MESH_EVENT_PARENT_CONNECTED: {
            mesh_event_connected_t *conn = (mesh_event_connected_t *)event_data;
            memcpy(&s_parent_addr, &conn->connected.bssid, sizeof(mesh_addr_t));
            ESP_LOGI(TAG, "Parent connected. Layer=%d, rssi=%d", conn->self_layer, conn->rssi);
            s_is_root = esp_mesh_is_root();
            if (s_is_root) ESP_LOGI(TAG, "I am ROOT (Exit node)");
            break;
        }
        case MESH_EVENT_PARENT_DISCONNECTED: {
            ESP_LOGW(TAG, "Parent disconnected");
            break;
        }
        case MESH_EVENT_ROUTING_TABLE_ADD: {
            ESP_LOGI(TAG, "Routing table add");
            break;
        }
        case MESH_EVENT_ROOT_SWITCH_ADDR: {
            ESP_LOGW(TAG, "Root switched");
            break;
        }
        default:
            break;
    }
}

void mesh_init_start(void) {
    // Wi-Fi init
    wifi_init();

    // Mesh init
    ESP_ERROR_CHECK(esp_mesh_init());
    ESP_ERROR_CHECK(esp_event_handler_register(MESH_EVENT, ESP_EVENT_ANY_ID, &mesh_event_handler, NULL));

    mesh_cfg_t cfg = MESH_INIT_CONFIG_DEFAULT();

    // Router (backbone) credentials
    wifi_config_t router = { 0 };
    strcpy((char *)router.sta.ssid, CONFIG_MESH_ROUTER_SSID);
    strcpy((char *)router.sta.password, CONFIG_MESH_ROUTER_PASSWD);

    memcpy(&cfg.router.ssid, &router.sta.ssid, sizeof(cfg.router.ssid));
    memcpy(&cfg.router.password, &router.sta.password, sizeof(cfg.router.password));

    // Mesh ID (derive from SSID hash for demo)
    uint8_t mesh_id[6] = {0x11,0x22,0x33,0x44,0x55,0x66};
    memcpy(cfg.mesh_id, mesh_id, 6);

    cfg.channel = CONFIG_MESH_CHANNEL; // 0 auto
    cfg.mesh_ap.max_connection = CONFIG_MESH_AP_CONNECTIONS;
    cfg.mesh_ap.nonmesh_max_connection = 0;
    cfg.mesh_ap.channel_switch_disable = false;

    // Enable RSSI based parent selection & root election
    mesh_vote_cfg_t vote_cfg = {
        .percentage = CONFIG_MESH_VOTE_PERCENTAGE,
        .is_all = false,
    };
    ESP_ERROR_CHECK(esp_mesh_set_vote_percentage(&vote_cfg));
    ESP_ERROR_CHECK(esp_mesh_set_xon_xoff(true));

    ESP_ERROR_CHECK(esp_mesh_set_config(&cfg));
    ESP_ERROR_CHECK(esp_mesh_start());
}

bool mesh_is_root(void) { return s_is_root; }

uint64_t mesh_get_node_mac64(void) {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    uint64_t v = 0;
    for (int i=0;i<6;i++) v = (v<<8) | mac[i];
    return v;
}

esp_err_t mesh_send_json_to_root(const char *json, size_t len) {
    if (s_is_root) {
        // If already root, broadcast to self-handling (optional)
        return mesh_broadcast_json(json, len);
    }
    mesh_data_t data = {
        .data = (uint8_t *)json,
        .size = len,
        .proto = MESH_PROTO_JSON,
        .toss = 0,
    };
    mesh_addr_t root = {0};
    ESP_ERROR_CHECK(esp_mesh_get_root_addr(&root));
    return esp_mesh_send(&root, &data, MESH_DATA_FROMDS | MESH_DATA_TODS, NULL, 0);
}

esp_err_t mesh_broadcast_json(const char *json, size_t len) {
    mesh_data_t data = {
        .data = (uint8_t *)json,
        .size = len,
        .proto = MESH_PROTO_JSON,
        .toss = 0,
    };
    return esp_mesh_send(NULL, &data, MESH_DATA_FROMDS | MESH_DATA_TODS, NULL, 0);
}

void mesh_rx_loop(void) {
    mesh_addr_t from = {0};
    mesh_data_t data = {0};
    uint8_t rx_buf[1024];
    data.data = rx_buf;
    data.size = sizeof(rx_buf);

    while (1) {
        memset(rx_buf, 0, sizeof(rx_buf));
        data.size = sizeof(rx_buf);
        int flag = 0;
        esp_err_t err = esp_mesh_recv(&from, &data, portMAX_DELAY, &flag, NULL, 0);
        if (err == ESP_OK) {
            // Only root needs to aggregate
            if (s_is_root) {
                comms_on_rx_json((const char *)data.data, data.size, from.addr);
            }
        }
    }
}