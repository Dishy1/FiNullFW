#pragma once
#include <stdint.h>
#include <stdbool.h>

void actuators_init(void);

// TB6612FNG (0~1023 속도 레인지)
void motorA_forward(int speed);
void motorA_reverse(int speed);
void motorA_stop(void);
void motorB_forward(int speed);
void motorB_reverse(int speed);
void motorB_stop(void);

// 릴레이 (Active-Low)
void relay_12v_led(bool on);
void relay_12v_buzzer(bool on);
void relay_5v_led(bool on);

// 서보 모터 (180degs)
void servo_floor_set_angle(float deg);
void servo_arm_set_angle(float deg);