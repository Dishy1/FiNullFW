#include "actuators.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"

static const char *TAG = "ACT";

// Pins
#define GPIO_PWMA 32
#define GPIO_PWMB 33
#define GPIO_BIN1 13
#define GPIO_BIN2 23
#define GPIO_AIN1 16
#define GPIO_AIN2 17
#define GPIO_STBY 25

#define GPIO_R_12V_LED 12
#define GPIO_R_12V_BZZ 14
#define GPIO_R_5V_LED  27

#define GPIO_SERVO_FLOOR 19
#define GPIO_SERVO_ARM   18


#define CH_PWMA LEDC_CHANNEL_0
#define CH_PWMB LEDC_CHANNEL_1
#define CH_SERVO1 LEDC_CHANNEL_2
#define CH_SERVO2 LEDC_CHANNEL_3

static void pwm_setup(int gpio, ledc_channel_t ch, int freq, int duty_res_bits){
    ledc_timer_config_t tcfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = (ch==CH_PWMA||ch==CH_PWMB)?LEDC_TIMER_0:LEDC_TIMER_1,
        .duty_resolution = duty_res_bits,
        .freq_hz = freq,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&tcfg);

    ledc_channel_config_t ccfg = {
        .gpio_num = gpio,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = ch,
        .timer_sel = (ch==CH_PWMA||ch==CH_PWMB)?LEDC_TIMER_0:LEDC_TIMER_1,
        .duty = 0,
        .hpoint = 0,
        .flags.output_invert = 0,
    };
    ledc_channel_config(&ccfg);
}

void actuators_init(void){
    gpio_config_t io = {0};
    io.mode = GPIO_MODE_OUTPUT;
    io.pin_bit_mask = (1ULL<<GPIO_BIN1)|(1ULL<<GPIO_BIN2)|(1ULL<<GPIO_AIN1)|(1ULL<<GPIO_AIN2)|(1ULL<<GPIO_STBY)|
                      (1ULL<<GPIO_R_12V_LED)|(1ULL<<GPIO_R_12V_BZZ)|(1ULL<<GPIO_R_5V_LED);
    gpio_config(&io);

    gpio_set_level(GPIO_STBY, 1);

    // 릴레이 Active-Low 세팅
    gpio_set_level(GPIO_R_12V_LED, 1);
    gpio_set_level(GPIO_R_12V_BZZ, 1);
    gpio_set_level(GPIO_R_5V_LED, 1);

    // TB6612 PWM 
    pwm_setup(GPIO_PWMA, CH_PWMA, 20000, LEDC_TIMER_10_BIT);
    pwm_setup(GPIO_PWMB, CH_PWMB, 20000, LEDC_TIMER_10_BIT);

    // 서보 PWM 50hz
    pwm_setup(GPIO_SERVO_FLOOR, CH_SERVO1, 50, LEDC_TIMER_16_BIT);
    pwm_setup(GPIO_SERVO_ARM,   CH_SERVO2, 50, LEDC_TIMER_16_BIT);
}

static void set_motor_AB(int ain1, int ain2, ledc_channel_t ch, int speed){
    if (speed<0) speed=0; if (speed>1023) speed=1023;
    gpio_set_level(ain1, 1);
    gpio_set_level(ain2, 0);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, ch, speed);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, ch);
}

static void set_motor_AB_rev(int ain1, int ain2, ledc_channel_t ch, int speed){
    if (speed<0) speed=0; if (speed>1023) speed=1023;
    gpio_set_level(ain1, 0);
    gpio_set_level(ain2, 1);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, ch, speed);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, ch);
}

static void stop_motor_AB(int ain1, int ain2, ledc_channel_t ch){
    ledc_set_duty(LEDC_LOW_SPEED_MODE, ch, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, ch);
    gpio_set_level(ain1, 0);
    gpio_set_level(ain2, 0);


servo_floor_set_angle(90.0f);
}

void motorA_forward(int speed){ set_motor_AB(GPIO_AIN1, GPIO_AIN2, CH_PWMA, speed); }
void motorA_reverse(int speed){ set_motor_AB_rev(GPIO_AIN1, GPIO_AIN2, CH_PWMA, speed); }
void motorA_stop(void){ stop_motor_AB(GPIO_AIN1, GPIO_AIN2, CH_PWMA); }
void motorB_forward(int speed){ set_motor_AB(GPIO_BIN1, GPIO_BIN2, CH_PWMB, speed); }
void motorB_reverse(int speed){ set_motor_AB_rev(GPIO_BIN1, GPIO_BIN2, CH_PWMB, speed); }
void motorB_stop(void){ stop_motor_AB(GPIO_BIN1, GPIO_BIN2, CH_PWMB); }

void relay_12v_led(bool on){ gpio_set_level(GPIO_R_12V_LED, on?0:1); }
void relay_12v_buzzer(bool on){ gpio_set_level(GPIO_R_12V_BZZ, on?0:1); }
void relay_5v_led(bool on){ gpio_set_level(GPIO_R_5V_LED, on?0:1); }

static void servo_write_deg(ledc_channel_t ch, float deg){
    if (deg<0) deg=0; if (deg>180) deg=180;
    float us = 500.0f + (deg/180.0f)*2000.0f;
    uint32_t duty = (uint32_t)((us/20000.0f) * 65535.0f);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, ch, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, ch);
}

void servo_floor_set_angle(float deg){ servo_write_deg(CH_SERVO1, deg); }
void servo_arm_set_angle(float deg){ servo_write_deg(CH_SERVO2, deg); }
