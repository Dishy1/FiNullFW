#include "fsm.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "sensors.h"
#include "actuators.h"
#include "mesh.h"
#include "comms.h"
#include "config.h"
#include "cJSON.h"

static const char *TAG = "FSM";

static uint8_t s_state = 0; // 0..6
static uint8_t s_simple = 0; // 0 normal,1 fire,2 expiry

static void push_state_uplink(void){
    cJSON *o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "type", "state_change");
    cJSON_AddStringToObject(o, "hwid", config_get_hwid());
    cJSON_AddNumberToObject(o, "fsm_state", s_state);
    cJSON_AddNumberToObject(o, "simple_state", s_simple);
    char *out = cJSON_PrintUnformatted(o);
    if (mesh_is_root()) mesh_broadcast_json(out, strlen(out));
    else mesh_send_json_to_root(out, strlen(out));
    cJSON_free(out); cJSON_Delete(o);
}

uint8_t fsm_simple_state(void){ return s_simple; }
uint8_t fsm_get_state(void){ return s_state; }
void fsm_set_state(uint8_t st){ s_state = st; push_state_uplink(); }

static void fire_alert_task_tick(void){
    // LED와 사이렌 활성화
    bool active = (s_state>=1 && s_state<=6);
    if (active){
        relay_12v_led(true); relay_5v_led(true); relay_12v_buzzer(true);
        if (sensors_emergency_pressed()){
            // 모든 동작 취소
            motorA_stop(); motorB_stop();
            relay_12v_led(false); relay_5v_led(false); relay_12v_buzzer(false);
            s_state = 0; s_simple = 0; push_state_uplink();
        }
    } else {
        relay_12v_led(false); relay_5v_led(false); relay_12v_buzzer(false);
    }
}

void fsm_loop(void){
    TickType_t last = xTaskGetTickCount();
    while (1){
        // ── State 0: 정상 상태
        if (s_state == 0){
            float mq7_v = sensors_get_mq7();
            float temp = sensors_get_temperature();
            bool expiry_due = false;

            if (expiry_due){ s_simple = 2; s_state = 2; push_state_uplink(); }
            else if (mq7_v > 2.0f || temp > 50.0f){ s_simple = 1; s_state = 1; push_state_uplink(); }
            else { s_simple = 0; }
        }

        // ── State 1: 화재 발생 
        if (s_state == 1){
            // 오작동 방지를 위해 30초간 사이렌을 울리고, 개입 없을 경우 자동 소화 과정으로 변경
            push_state_uplink();
            TickType_t t0 = xTaskGetTickCount();
            while (s_state==1){
                fire_alert_task_tick();
                if ((xTaskGetTickCount()-t0) > pdMS_TO_TICKS(30000)) { s_state = 3; break; }
                vTaskDelay(pdMS_TO_TICKS(100));
            }
        }

        // ── State 2: 소화기 내구연한 만료
        if (s_state == 2){
            s_simple = 2; push_state_uplink();
            // Keep alerting
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        // ── State 3: 안전핀 제거
        if (s_state == 3){
            motorA_forward(1000);
            TickType_t t0 = xTaskGetTickCount();
            while ((xTaskGetTickCount()-t0) < pdMS_TO_TICKS(5000)){
                fire_alert_task_tick(); vTaskDelay(pdMS_TO_TICKS(50));
            }
            motorA_stop(); s_state = 4; push_state_uplink();
        }

        // ── State 4: 바닥 회전 MLX
        if (s_state == 4){
            float deg = sensors_get_mlx_aim_deg(); // 70..110°
            servo_floor_set_angle(deg);
            TickType_t t0 = xTaskGetTickCount();
            while ((xTaskGetTickCount()-t0) < pdMS_TO_TICKS(5000)){
                fire_alert_task_tick(); vTaskDelay(pdMS_TO_TICKS(50));
            }
            s_state = 5; push_state_uplink();
        }

        // ── State 5: 팔 회전
        if (s_state == 5){
            servo_arm_set_angle(60.0f);
            TickType_t t0 = xTaskGetTickCount();
            while ((xTaskGetTickCount()-t0) < pdMS_TO_TICKS(5000)){
                fire_alert_task_tick(); vTaskDelay(pdMS_TO_TICKS(50));
            }
            s_state = 6; push_state_uplink();
        }

        // ── State 6: 레버 작동
        if (s_state == 6){
            motorB_forward(1000);
            TickType_t t0 = xTaskGetTickCount();
            while ((xTaskGetTickCount()-t0) < pdMS_TO_TICKS(5000)){
                fire_alert_task_tick(); vTaskDelay(pdMS_TO_TICKS(50));
            }
            motorB_stop();
            vTaskDelay(pdMS_TO_TICKS(10000));
            motorB_reverse(1000);
            vTaskDelay(pdMS_TO_TICKS(5000));
            motorB_stop();
            vTaskDelay(pdMS_TO_TICKS(500));
        }

        fire_alert_task_tick();
        vTaskDelayUntil(&last, pdMS_TO_TICKS(50));
    }
}
