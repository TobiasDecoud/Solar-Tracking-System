#include "bno085.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <math.h>

#define TAG             "BNO085"
#define BNO085_ADDR     0x4A
#define I2C_SPEED_HZ    400000
#define TIMEOUT_MS      100
#define MAX_CARGO       256
#define SHTP_READ_LEN   64  // lectura unica: BNO085 padea con 0x00 si el paquete es mas corto

// SHTP channels
#define CH_CONTROL  2
#define CH_REPORTS  3

// Report IDs
#define RPT_PRODUCT_ID_RESP     0xF8
#define RPT_BASE_TIMESTAMP      0xFB
#define RPT_SET_FEATURE_CMD     0xFD

// Sensor report IDs (canal CH_REPORTS)
#define SENSOR_ROTATION_VECTOR  0x05
#define ROTATION_Q              14      // escala Q14: divide por 16384 para obtener float unitario

static i2c_master_dev_handle_t imu_dev;
static uint8_t seq[6];  // numero de secuencia por canal


// ---------------------------------------------------------------------------
// SHTP: leer un paquete completo en una sola transaccion I2C.
// El BNO085 re-envia el paquete desde el inicio en cada transaccion nueva,
// asi que la lectura en dos pasos daria el header de nuevo en el segundo read.
// Leemos SHTP_READ_LEN bytes de una vez: header[4] + cargo[SHTP_READ_LEN-4].
// El dispositivo padea con 0x00 si el paquete es mas corto que lo pedido.
// ---------------------------------------------------------------------------
static esp_err_t shtp_read(uint8_t *ch, uint8_t *buf, uint16_t *len)
{
    uint8_t raw[SHTP_READ_LEN];
    esp_err_t r = i2c_master_receive(imu_dev, raw, SHTP_READ_LEN, TIMEOUT_MS);
    if (r != ESP_OK) { *len = 0; return r; }

    uint16_t plen = ((uint16_t)(raw[1] & 0x7F) << 8) | raw[0];
    *ch  = raw[2];
    *len = 0;

    if (plen <= 4) return ESP_OK;

    uint16_t cargo = plen - 4;
    uint16_t avail = SHTP_READ_LEN - 4;
    if (cargo > avail) cargo = avail;
    memcpy(buf, raw + 4, cargo);
    *len = cargo;
    return ESP_OK;
}


// ---------------------------------------------------------------------------
// SHTP: enviar un paquete en el canal dado.
// ---------------------------------------------------------------------------
static esp_err_t shtp_write(uint8_t channel, const uint8_t *payload, uint16_t plen)
{
    uint8_t buf[MAX_CARGO + 4];
    uint16_t total = plen + 4;
    buf[0] = total & 0xFF;
    buf[1] = (total >> 8) & 0x7F;
    buf[2] = channel;
    buf[3] = seq[channel]++;
    memcpy(buf + 4, payload, plen);
    return i2c_master_transmit(imu_dev, buf, total, TIMEOUT_MS);
}


// ---------------------------------------------------------------------------
// PUBLICO: inicializacion
// ---------------------------------------------------------------------------
esp_err_t bno085_init(i2c_master_bus_handle_t bus)
{
    // Configurar pines NRST y BOOTN
    gpio_config_t io = {
        .pin_bit_mask  = (1ULL << BNO085_NRST_PIN) | (1ULL << BNO085_BOOTN_PIN),
        .mode          = GPIO_MODE_OUTPUT,
        .pull_up_en    = GPIO_PULLUP_DISABLE,
        .pull_down_en  = GPIO_PULLDOWN_DISABLE,
        .intr_type     = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);

    // BOOTN alto = modo I2C normal; pulso en NRST para resetear
    gpio_set_level(BNO085_BOOTN_PIN, 1);
    gpio_set_level(BNO085_NRST_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(BNO085_NRST_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(700));  // esperar startup

    // Registrar dispositivo en el bus
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = BNO085_ADDR,
        .scl_speed_hz    = I2C_SPEED_HZ,
    };
    esp_err_t ret = i2c_master_bus_add_device(bus, &dev_cfg, &imu_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "No se pudo agregar al bus: %s", esp_err_to_name(ret));
        return ret;
    }

    // Leer paquetes de arranque; loguear cada uno para diagnostico
    uint8_t ch;
    uint8_t cargo[MAX_CARGO];
    uint16_t clen;
    bool got_id = false;

    for (int i = 0; i < 15; i++) {
        esp_err_t r = shtp_read(&ch, cargo, &clen);
        if (r != ESP_OK) {
            ESP_LOGW(TAG, "[%d] shtp_read err: %s", i, esp_err_to_name(r));
        } else if (clen > 0) {
            ESP_LOGI(TAG, "[%d] pkt ch=%d len=%d rpt=0x%02X", i, ch, clen, cargo[0]);
            if (ch == CH_CONTROL && clen >= 16 && cargo[0] == RPT_PRODUCT_ID_RESP) {
                uint32_t build = (uint32_t)(cargo[8] | cargo[9] << 8 | cargo[10] << 16 | cargo[11] << 24);
                ESP_LOGI(TAG, "BNO085 OK - FW v%d.%d build %lu", cargo[2], cargo[3], (unsigned long)build);
                got_id = true;
                break;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    if (!got_id) {
        // Pedir product ID explicitamente
        uint8_t req[] = { 0xF9, 0x00 };
        ESP_LOGI(TAG, "Enviando Product ID Request...");
        shtp_write(CH_CONTROL, req, sizeof(req));
        vTaskDelay(pdMS_TO_TICKS(100));
        for (int i = 0; i < 10; i++) {
            esp_err_t r = shtp_read(&ch, cargo, &clen);
            if (r != ESP_OK) {
                ESP_LOGW(TAG, "[req %d] shtp_read err: %s", i, esp_err_to_name(r));
            } else if (clen > 0) {
                ESP_LOGI(TAG, "[req %d] pkt ch=%d len=%d rpt=0x%02X", i, ch, clen, cargo[0]);
                if (ch == CH_CONTROL && clen >= 16 && cargo[0] == RPT_PRODUCT_ID_RESP) {
                    uint32_t build = (uint32_t)(cargo[8] | cargo[9] << 8 | cargo[10] << 16 | cargo[11] << 24);
                    ESP_LOGI(TAG, "BNO085 OK - FW v%d.%d build %lu", cargo[2], cargo[3], (unsigned long)build);
                    got_id = true;
                    break;
                }
            }
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }

    if (!got_id) {
        ESP_LOGW(TAG, "No se recibio Product ID");
    }

    // Habilitar Rotation Vector a 50 Hz (intervalo = 20000 us)
    // Set Feature Command (0xFD): featureId, flags, changeSens(2), interval(4), batch(4), specific(4)
    uint8_t set_feat[] = {
        RPT_SET_FEATURE_CMD,
        SENSOR_ROTATION_VECTOR,
        0x00,                           // feature flags
        0x00, 0x00,                     // change sensitivity
        0x20, 0x4E, 0x00, 0x00,         // report interval: 20000 us = 50 Hz
        0x00, 0x00, 0x00, 0x00,         // batch interval
        0x00, 0x00, 0x00, 0x00,         // sensor specific config
    };
    ret = shtp_write(CH_CONTROL, set_feat, sizeof(set_feat));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error habilitando rotation vector: %s", esp_err_to_name(ret));
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_LOGI(TAG, "Rotation Vector habilitado a 50 Hz");
    return ESP_OK;
}


// ---------------------------------------------------------------------------
// PUBLICO: leer angulos de orientacion (yaw/pitch/roll) en grados.
//
// El cargo del canal 3 puede empezar con un Base Timestamp (0xFB, 5 bytes)
// seguido del Rotation Vector report (0x05, 14 bytes):
//   [0] reportId  [1] seq  [2] status  [3] delay
//   [4-5] i  [6-7] j  [8-9] k  [10-11] real  (todos int16 Q14)
//   [12-13] accuracy estimate
//
// Conversion cuaternion -> Euler (convencion ZYX):
//   yaw   = atan2(2(wr*wz + wx*wy), 1 - 2(wy^2 + wz^2))  [0..360]
//   pitch = asin (2(wr*wy - wz*wx))
//   roll  = atan2(2(wr*wx + wy*wz), 1 - 2(wx^2 + wy^2))
// ---------------------------------------------------------------------------
bool bno085_read_euler(float *yaw, float *pitch, float *roll)
{
    uint8_t ch;
    uint8_t cargo[MAX_CARGO];
    uint16_t clen;

    if (shtp_read(&ch, cargo, &clen) != ESP_OK) return false;
    if (ch != CH_REPORTS || clen == 0) return false;

    uint8_t *p = cargo;
    uint16_t rem = clen;

    // Saltar base timestamp si esta presente
    if (rem >= 5 && p[0] == RPT_BASE_TIMESTAMP) {
        p   += 5;
        rem -= 5;
    }

    if (rem < 14 || p[0] != SENSOR_ROTATION_VECTOR) return false;

    float scale = 1.0f / (1 << ROTATION_Q);
    float qi = (int16_t)((uint16_t)p[4]  | ((uint16_t)p[5]  << 8)) * scale;
    float qj = (int16_t)((uint16_t)p[6]  | ((uint16_t)p[7]  << 8)) * scale;
    float qk = (int16_t)((uint16_t)p[8]  | ((uint16_t)p[9]  << 8)) * scale;
    float qr = (int16_t)((uint16_t)p[10] | ((uint16_t)p[11] << 8)) * scale;

    *roll  = atan2f(2.0f*(qr*qi + qj*qk), 1.0f - 2.0f*(qi*qi + qj*qj)) * (180.0f / M_PI);
    *pitch = asinf (2.0f*(qr*qj - qk*qi))                               * (180.0f / M_PI);
    *yaw   = atan2f(2.0f*(qr*qk + qi*qj), 1.0f - 2.0f*(qj*qj + qk*qk)) * (180.0f / M_PI);

    // Convertir yaw de [-180, 180] a [0, 360]
    if (*yaw < 0.0f) *yaw += 360.0f;

    return true;
}
