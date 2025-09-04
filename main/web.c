#include "web.h"
#include <string.h>
#include "esp_log.h"
#include "esp_http_server.h"
#include "comms.h"
#include "cJSON.h"
#include "config.h"
#include "esp_heap_caps.h"

static const char *TAG = "WEB";

static httpd_handle_t s_server = NULL;

// CORS
static void set_cors_common(httpd_req_t *req){
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
    httpd_resp_set_hdr(req, "Access-Control-Max-Age", "600");
}

static esp_err_t options_handler(httpd_req_t *req){
    set_cors_common(req);
    httpd_resp_sendstr(req, "");
    return ESP_OK;
}

// /api/nodes (GET)
static esp_err_t api_nodes_handler(httpd_req_t *req){
    set_cors_common(req);
    return comms_send_nodes_to_client(req);
}

// /api/config (GET)
static esp_err_t api_config_get_handler(httpd_req_t *req){
    set_cors_common(req);

    cJSON *o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "hwid", config_get_hwid());
    cJSON_AddStringToObject(o, "expiry", config_get_expiry());
    char *out = cJSON_PrintUnformatted(o);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, out);
    cJSON_free(out); cJSON_Delete(o);
    return ESP_OK;
}

// /api/config (POST)
static esp_err_t api_config_post_handler(httpd_req_t *req){
    set_cors_common(req);

    char buf[256]; int r = httpd_req_recv(req, buf, sizeof(buf)-1);
    if (r<=0){ httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "no body"); return ESP_OK; }
    buf[r]=0;
    cJSON *o = cJSON_Parse(buf);
    if (!o){ httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "json"); return ESP_OK; }
    cJSON *h = cJSON_GetObjectItem(o, "hwid");
    cJSON *e = cJSON_GetObjectItem(o, "expiry");
    if (h && cJSON_IsString(h)) config_set_hwid(h->valuestring);
    if (e && cJSON_IsString(e)) config_set_expiry(e->valuestring);
    cJSON_Delete(o);
    return api_config_get_handler(req);
}

void web_start(void){
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port = 80;
    cfg.stack_size = 8192;
    ESP_ERROR_CHECK(httpd_start(&s_server, &cfg));

    // OPTIONS 프리플라이트(와일드카드)
    httpd_uri_t opt_any = { .uri="/*", .method=HTTP_OPTIONS, .handler=options_handler };
    httpd_register_uri_handler(s_server, &opt_any);

    // JSON 전용 라우트
    httpd_uri_t nodes = { .uri="/api/nodes", .method=HTTP_GET, .handler=api_nodes_handler };
    httpd_register_uri_handler(s_server, &nodes);

    httpd_uri_t cfgget = { .uri="/api/config", .method=HTTP_GET, .handler=api_config_get_handler };
    httpd_register_uri_handler(s_server, &cfgget);

    httpd_uri_t cfgpost = { .uri="/api/config", .method=HTTP_POST, .handler=api_config_post_handler };
    httpd_register_uri_handler(s_server, &cfgpost);
}
