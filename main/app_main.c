#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "mesh.h"
#include "fsm.h"
#include "web.h"
#include "config.h"
#include "sensors.h"
#include "actuators.h"
#include "comms.h"

static const char *TAG = "APP";

// 태스크 핸들
static TaskHandle_t fire_fsm_task_h = NULL; //화재용 FSM
static TaskHandle_t web_host_task_h = NULL; //웹용 FSM
static TimerHandle_t heartbeat_timer; //1분 당 Heartbeat 모니터링

static void heartbeat_cb(TimerHandle_t xTimer) {
    (void)xTimer;
    comms_send_periodic();  // Uplink Heartbeat 테스크
}

static void web_host_task(void *arg) {
    // HTTP 테스크와 HeartBeat 리시빙 테스크
    vTaskDelay(pdMS_TO_TICKS(2000)); //네트워크 안정화 대기
    if (mesh_is_root()) {
        ESP_LOGI(TAG, "ROOT Node Detected. Web Server Starting..");
        web_start(); // Start Web Hosting
    } else {
        ESP_LOGI(TAG, "NON-ROOT Node Detected. Web Server disabled.");
    }
    mesh_rx_loop(); // ROOT RX 받기
    vTaskDelete(NULL);
}

//화재 감지 + 화재 소화 Task
static void fire_fsm_task(void *arg) {
    fsm_loop(); //FSM 기반 화재 감지 Loop
    vTaskDelete(NULL);
}

void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init()); // 스토리지 Initialzing, 오류 시 로그
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_netif_init()); //네트워크 Initializing, 오류 시 로그

    config_init();     // HWID 설정 + NVS 세팅
    actuators_init();  // 액추에이터 + GPIO 초기화
    sensors_init();    // ADC, button, MLX 센서 초기화

    mesh_init_start(); // Mesh + Wi-Fi start

    // Pinned Task를 통해 특정 코어에서 각자 FSM 형태로 루프 진행.
    xTaskCreatePinnedToCore(fire_fsm_task, "fire_fsm", 8192, NULL, 5, &fire_fsm_task_h, 0); // 코어 0
    xTaskCreatePinnedToCore(web_host_task, "web_host", 8192, NULL, 4, &web_host_task_h, 1); // 코어 1

    // 1분마다 Heartbeat 제공.
    heartbeat_timer = xTimerCreate("hb", pdMS_TO_TICKS(60000), pdTRUE, NULL, heartbeat_cb);
    xTimerStart(heartbeat_timer, 0);
}