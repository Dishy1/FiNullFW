#include "sensors.h"
#include "driver/adc.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "MLX90640_API.h"
#include "MLX90640_I2C_Driver.h"

static const char *TAG = "SENS";

#define GPIO_EMERGENCY 36
#define ADC_MQ7_GPIO   34
#define I2C_SDA        21
#define I2C_SCL        20

static adc_oneshot_unit_handle_t adc1;
static int mq7_ch = ADC_CHANNEL_6; // GPIO34


static const uint8_t MLX_ADDR = 0x33;
static paramsMLX90640 g_params;
static bool g_mlx_ready = false;
static int16_t g_frame_raw[834];
static float g_frame_to[24*32];   // last computed temperatures (°C)

static void mlx_init_if_needed(void) {
    if (g_mlx_ready) return;
    MLX90640_I2CInit();
    int16_t eedata[834];
    if (MLX90640_DumpEE(MLX_ADDR, eedata) != 0) return;
    if (MLX90640_ExtractParameters(eedata, &g_params) != 0) return;
    // 기본 0.95 방사율
    g_mlx_ready = true;
}

static void mlx_read_update(void) {
    if (!g_mlx_ready) mlx_init_if_needed();
    if (!g_mlx_ready) return; // init 실패 시 그냥 탈출

    // 두 서브페이지를 연속으로 읽고, 마지막 계산 결과를 유지
    int16_t fr[834];
    for (int sub = 0; sub < 2; ++sub) {
        if (MLX90640_GetFrameData(MLX_ADDR, fr) != 0) continue;
        float Ta = MLX90640_GetTa(fr, &g_params);
        MLX90640_CalculateTo(fr, &g_params, 0.95f, Ta, g_frame_to);
    }
}

void sensors_init(void){
    // 비상 버튼
    gpio_config_t io = {
        .pin_bit_mask = 1ULL<<GPIO_EMERGENCY,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE, 
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io);

    // MQ-7 측정 위한 ADC OneShot
    adc_oneshot_unit_init_cfg_t unit_cfg = { .unit_id = ADC_UNIT_1 };
    adc_oneshot_new_unit(&unit_cfg, &adc1);
    adc_oneshot_chan_cfg_t cfg = { .bitwidth = ADC_BITWIDTH_12, .atten = ADC_ATTEN_DB_11};
    adc_oneshot_config_channel(adc1, mq7_ch, &cfg);

    // I2C 
    i2c_config_t i2ccfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA,
        .scl_io_num = I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000
    };
    i2c_param_config(I2C_NUM_0, &i2ccfg);
    i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
}

float sensors_get_mq7(void){
    int val=0; adc_oneshot_read(adc1, mq7_ch, &val);
    return (3.3f * val) / 4095.0f;
}

bool sensors_emergency_pressed(void){
    int level = gpio_get_level(GPIO_EMERGENCY);
    return level==0; // Active-Low
}

// 분기 예측 최적화 최대값 인식
static inline float max_reduce_branchlight(const float *arr, int n, int *out_idx){
    float maxv = -1000.0f;
    int idx = 0;
    for (int i=0;i<n;i++){
        float ai = arr[i];
        // 최소 분기: 어셈블리 CMOV와 비슷하게, 최대한 분기 줄여 최적화
        if (ai > maxv){ maxv = ai; idx = i; }
    }
    if (out_idx) *out_idx = idx;
    return maxv;
}

static void mlx_read_update(void){
    if (!g_mlx_ready) mlx_init_if_needed();
    if (!g_mlx_ready) return;

    for (int sub=0; sub<2; ++sub){
        if (MLX90640_GetFrameData(MLX_ADDR, g_frame_raw) != 0) continue;
        float Ta = MLX90640_GetTa(g_frame_raw, &g_params);
        MLX90640_CalculateTo(g_frame_raw, &g_params, 0.95f, Ta, g_to);
    }
}

float sensors_get_temperature(void){
#ifdef CONFIG_USE_SPARKFUN_MLX90640
    mlx_read_update();                         // g_frame_to 갱신
    int hot_idx = 0;
    float maxv = max_reduce_branchlight(g_frame_to, 24*32, &hot_idx);
    return maxv;                               // 최댓값(peak)
#else
    // Stub: MQ-7로 20~50°C 시뮬
    float v = sensors_get_mq7();
    return 20.0f + v*30.0f;
#endif
}

float sensors_get_mlx_aim_deg(void){
#ifdef CONFIG_USE_SPARKFUN_MLX90640
    // 프레임을 최신으로 (온도 읽기와 중복되어도 가벼움)
    mlx_read_update();
    int hot_idx = 0;
    (void)max_reduce_branchlight(g_frame_to, 24*32, &hot_idx);
    int cols = 32;
    int col = hot_idx % cols;          // 0..31
    int col64 = (col * 63) / 31;       // 0..63
    // 0→70°, 63→110°, 32 근처 → 90°
    return 70.0f + (110.0f - 70.0f) * ((float)col64 / 63.0f);
#else
    // Stub: 중앙
    int col64 = (16 * 63) / 31;
    return 70.0f + (110.0f - 70.0f) * ((float)col64 / 63.0f);
#endif
}
