#include "MLX90640_I2C_Driver.h"
#include "driver/i2c.h"
#include "esp_timer.h"
#include "esp_err.h"

#define I2C_PORT        I2C_NUM_0
#define I2C_FREQ_HZ     400000
#define I2C_TIMEOUT_MS  50

void MLX90640_I2CInit(void) {
    // main/sensors.c 에서 이미 I2C_NUM_0 초기화/설치했다면 여기서 추가로 할 것 없음.
    // 별도 초기화가 필요하면 아래 코드를 사용하세요.
    // (중복 init 을 피하려면 sensors_init() 쪽에서만 init 하도록 하세요)
}

static inline esp_err_t i2c_write_read(uint8_t addr7, const uint8_t *wbuf, size_t wlen, uint8_t *rbuf, size_t rlen) {
    if (wlen) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr7 << 1) | I2C_MASTER_WRITE, true);
        i2c_master_write(cmd, (uint8_t*)wbuf, wlen, true);
        if (rlen) {
            i2c_master_start(cmd);
            i2c_master_write_byte(cmd, (addr7 << 1) | I2C_MASTER_READ, true);
            i2c_master_read(cmd, rbuf, rlen, I2C_MASTER_LAST_NACK);
        }
        i2c_master_stop(cmd);
        esp_err_t err = i2c_master_cmd_begin(I2C_PORT, cmd, I2C_TIMEOUT_MS / portTICK_PERIOD_MS);
        i2c_cmd_link_delete(cmd);
        return err;
    } else {
        if (!rlen) return ESP_OK;
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr7 << 1) | I2C_MASTER_READ, true);
        i2c_master_read(cmd, rbuf, rlen, I2C_MASTER_LAST_NACK);
        i2c_master_stop(cmd);
        esp_err_t err = i2c_master_cmd_begin(I2C_PORT, cmd, I2C_TIMEOUT_MS / portTICK_PERIOD_MS);
        i2c_cmd_link_delete(cmd);
        return err;
    }
}

int MLX90640_I2CRead(uint8_t slaveAddr, uint16_t startAddress, uint16_t nWords, uint16_t *data) {
    // MLX는 16-bit 주소, 16-bit 데이터 워드 배열
    uint8_t wbuf[2];
    wbuf[0] = (uint8_t)(startAddress >> 8);
    wbuf[1] = (uint8_t)(startAddress & 0xFF);
    size_t bytes = nWords * 2;
    uint8_t *rbuf = (uint8_t *)malloc(bytes);
    if (!rbuf) return -1;
    esp_err_t err = i2c_write_read(slaveAddr, wbuf, 2, rbuf, bytes);
    if (err != ESP_OK) { free(rbuf); return -1; }
    // big-endian → uint16_t
    for (int i=0;i<nWords;i++) {
        data[i] = ((uint16_t)rbuf[2*i] << 8) | rbuf[2*i+1];
    }
    free(rbuf);
    return 0;
}

int MLX90640_I2CWrite(uint8_t slaveAddr, uint16_t writeAddress, uint16_t dataWord) {
    uint8_t wbuf[4];
    wbuf[0] = (uint8_t)(writeAddress >> 8);
    wbuf[1] = (uint8_t)(writeAddress & 0xFF);
    wbuf[2] = (uint8_t)(dataWord >> 8);
    wbuf[3] = (uint8_t)(dataWord & 0xFF);

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (slaveAddr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, wbuf, sizeof(wbuf), true);
    i2c_master_stop(cmd);
    esp_err_t err = i2c_master_cmd_begin(I2C_PORT, cmd, I2C_TIMEOUT_MS / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return (err == ESP_OK) ? 0 : -1;
}

void MLX90640_Delay(uint32_t ms) {
    // SparkFun 드라이버는 밀리초 지연을 기대
    uint64_t start = esp_timer_get_time();
    while ((esp_timer_get_time() - start) < (uint64_t)ms * 1000ULL) {
        // busy-wait (짧은 지연이라면 OK; 길면 vTaskDelay로 바꾸세요)
    }
}
