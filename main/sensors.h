#pragma once
#include <stdbool.h>
#include <stdint.h>

void sensors_init(void);
float sensors_get_mq7(void);          // ADC 전압 프록시 (0..3.3)
bool sensors_emergency_pressed(void); // GPIO36 - Active-Low


float sensors_get_temperature(void);   // MLX90640 분기 예측 최소화를 통한 고효율 peak 온도 측정
float sensors_get_mlx_aim_deg(void);   // hottest-column - degrees (70..110)
