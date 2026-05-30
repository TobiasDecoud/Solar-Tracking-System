#include "opt3001.h"
#include "esp_log.h"

#define TAG "OPT3001"
#define I2C_TIMEOUT_MS  50
#define I2C_SPEED_HZ    400000

// Registros
#define REG_RESULT  0x00
#define REG_CONFIG  0x01

// Configuracion: auto-range | CT=100ms | modo continuo | latch
// [15:12]=1100 [11]=0 [10:9]=11 [4]=1  =>  0xC610
#define CONFIG_CONTINUOUS  0xC610

// Direcciones I2C segun pin ADDR del sensor
static const uint8_t addrs[OPT3001_COUNT] = { 0x44, 0x45, 0x46, 0x47 };
static i2c_master_dev_handle_t devs[OPT3001_COUNT];


static esp_err_t write_reg16(i2c_master_dev_handle_t dev, uint8_t reg, uint16_t val)
{
    uint8_t buf[3] = { reg, (val >> 8) & 0xFF, val & 0xFF };
    return i2c_master_transmit(dev, buf, 3, I2C_TIMEOUT_MS);
}

static esp_err_t read_reg16(i2c_master_dev_handle_t dev, uint8_t reg, uint16_t *out)
{
    uint8_t buf[2];
    esp_err_t r = i2c_master_transmit_receive(dev, &reg, 1, buf, 2, I2C_TIMEOUT_MS);
    if (r == ESP_OK) *out = ((uint16_t)buf[0] << 8) | buf[1];
    return r;
}


esp_err_t opt3001_init(i2c_master_bus_handle_t bus)
{
    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .scl_speed_hz    = I2C_SPEED_HZ,
    };

    for (int i = 0; i < OPT3001_COUNT; i++) {
        cfg.device_address = addrs[i];
        esp_err_t r = i2c_master_bus_add_device(bus, &cfg, &devs[i]);
        if (r != ESP_OK) {
            ESP_LOGE(TAG, "IC%d (0x%02X): no se pudo agregar al bus: %s",
                     i + 1, addrs[i], esp_err_to_name(r));
            return r;
        }
        r = write_reg16(devs[i], REG_CONFIG, CONFIG_CONTINUOUS);
        if (r != ESP_OK) {
            ESP_LOGE(TAG, "IC%d (0x%02X): error configurando: %s",
                     i + 1, addrs[i], esp_err_to_name(r));
            return r;
        }
        ESP_LOGI(TAG, "IC%d en 0x%02X listo", i + 1, addrs[i]);
    }
    return ESP_OK;
}


bool opt3001_read_all(float lux[OPT3001_COUNT])
{
    bool any_ok = false;

    for (int i = 0; i < OPT3001_COUNT; i++) {
        uint16_t raw;
        if (read_reg16(devs[i], REG_RESULT, &raw) == ESP_OK) {
            // Resultado: E[15:12] exponente, R[11:0] mantisa
            // lux = 0.01 * 2^E * R
            uint8_t  e = (raw >> 12) & 0x0F;
            uint16_t r = raw & 0x0FFF;
            lux[i] = 0.01f * (float)(1 << e) * (float)r;
            any_ok = true;
        } else {
            lux[i] = -1.0f;
        }
    }
    return any_ok;
}
