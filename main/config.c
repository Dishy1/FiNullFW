#include "config.h"
#include <string.h>
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_mac.h"

static char s_hwid[32] = "7호관 A동 1층 복도 소화기"; //
static char s_expiry[16] = "2026.12.31"; // 미설정 시 Default Expiry

static void default_hwid(void){
    uint8_t mac[6]; esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(s_hwid, sizeof(s_hwid), "HW-%02X%02X%02X%02X%02X%02X", mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
}

void config_init(void){
    nvs_handle_t h; if (nvs_open("cfg", NVS_READWRITE, &h)==ESP_OK){
        size_t len = sizeof(s_hwid);
        if (nvs_get_str(h, "hwid", s_hwid, &len)!=ESP_OK || s_hwid[0]==0) default_hwid();
        len = sizeof(s_expiry);
        if (nvs_get_str(h, "expiry", s_expiry, &len)!=ESP_OK) strcpy(s_expiry, "2026.12.31");
        nvs_close(h);
    } else default_hwid();
}

const char *config_get_hwid(void){ return s_hwid; }
const char *config_get_expiry(void){ return s_expiry; }

void config_set_hwid(const char *hwid){
    strncpy(s_hwid, hwid, sizeof(s_hwid)); s_hwid[sizeof(s_hwid)-1]='\0';
    nvs_handle_t h; if (nvs_open("cfg", NVS_READWRITE, &h)==ESP_OK){ nvs_set_str(h, "hwid", s_hwid); nvs_commit(h); nvs_close(h);} }

void config_set_expiry(const char *expiry){
    strncpy(s_expiry, expiry, sizeof(s_expiry)); s_expiry[sizeof(s_expiry)-1]='\0';
    nvs_handle_t h; if (nvs_open("cfg", NVS_READWRITE, &h)==ESP_OK){ nvs_set_str(h, "expiry", s_expiry); nvs_commit(h); nvs_close(h);} }
