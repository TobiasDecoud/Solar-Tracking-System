#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "rtc.h"
#include "bno085.h"
#include "opt3001.h"
#include "motor.h"

static rtc_time_t build_time(void)
{
    static const char *months[] = {
        "Jan","Feb","Mar","Apr","May","Jun",
        "Jul","Aug","Sep","Oct","Nov","Dec"
    };
    char mon_str[4];
    int day, year, hour, min, sec;
    sscanf(__DATE__, "%3s %d %d", mon_str, &day, &year);
    sscanf(__TIME__, "%d:%d:%d", &hour, &min, &sec);

    uint8_t month = 1;
    for (int i = 0; i < 12; i++) {
        if (__builtin_strcmp(mon_str, months[i]) == 0) { month = i + 1; break; }
    }

    return (rtc_time_t){
        .hours   = hour,
        .minutes = min,
        .seconds = sec,
        .day     = day,
        .weekday = 0,
        .month   = month,
        .year    = year,
    };
}

#define LUZ_SDA_PIN  16
#define LUZ_SCL_PIN  15
#define LUZ_I2C_PORT I2C_NUM_0

#define IMU_SDA_PIN  41
#define IMU_SCL_PIN  42
#define IMU_I2C_PORT I2C_NUM_1

void app_main(void)
{
    // Bus 0: sensores de luz OPT3001 (SDA=16, SCL=15)
    i2c_master_bus_config_t bus0_cfg = {
        .clk_source           = I2C_CLK_SRC_DEFAULT,
        .i2c_port             = LUZ_I2C_PORT,
        .scl_io_num           = LUZ_SCL_PIN,
        .sda_io_num           = LUZ_SDA_PIN,
        .glitch_ignore_cnt    = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus0;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus0_cfg, &bus0));
    ESP_ERROR_CHECK(opt3001_init(bus0));

    // Bus 1: IMU (hardware I2C)
    i2c_master_bus_config_t bus1_cfg = {
        .clk_source           = I2C_CLK_SRC_DEFAULT,
        .i2c_port             = IMU_I2C_PORT,
        .scl_io_num           = IMU_SCL_PIN,
        .sda_io_num           = IMU_SDA_PIN,
        .glitch_ignore_cnt    = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus1;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus1_cfg, &bus1));

    ESP_ERROR_CHECK(motor_init());

    printf("Inicializando BNO085...\n");
    esp_err_t imu_err = bno085_init(bus1);
    if (imu_err != ESP_OK)
        printf("BNO085 init fallo: %s\n", esp_err_to_name(imu_err));

    // RTC: software I2C en pines %d/%d (definidos en rtc.h)
    pcf2131_init();

    // Configurar PWRMNG = 000 (switchover directo habilitado)
    uint8_t ctrl3 = pcf2131_read_reg(PCF2131_REG_CTRL3);
    ctrl3 &= ~(0x07 << 5);
    pcf2131_write_reg(PCF2131_REG_CTRL3, ctrl3);
    printf("Control_3 nuevo: 0x%02X\n", pcf2131_read_reg(PCF2131_REG_CTRL3));

    bool valid = pcf2131_is_valid();
    printf("OSF al arrancar: %s\n", valid ? "valido" : "INVALIDO - hora perdida");

    rtc_time_t now;
    pcf2131_get_time(&now);
    printf("Hora leida: %02d:%02d:%02d\n", now.hours, now.minutes, now.seconds);

    if (!valid) {
        rtc_time_t set = build_time();
        pcf2131_set_time(&set);
    }

    // ---- Test de motores ----
    printf("--- Test Motor 1: adelante 50%% por 2s ---\n");
    motor_set(MOTOR_1, MOTOR_FORWARD, 50);
    vTaskDelay(pdMS_TO_TICKS(2000));

    printf("--- Test Motor 1: atras 75%% por 2s ---\n");
    motor_set(MOTOR_1, MOTOR_BACKWARD, 75);
    vTaskDelay(pdMS_TO_TICKS(2000));

    printf("--- Test Motor 1: freno ---\n");
    motor_brake(MOTOR_1);
    vTaskDelay(pdMS_TO_TICKS(500));
    motor_coast(MOTOR_1);

    printf("--- Test Motor 2: adelante 100%% por 2s ---\n");
    motor_set(MOTOR_2, MOTOR_FORWARD, 100);
    vTaskDelay(pdMS_TO_TICKS(2000));

    printf("--- Test Motor 2: adelante 30%% por 2s ---\n");
    motor_set(MOTOR_2, MOTOR_FORWARD, 30);
    vTaskDelay(pdMS_TO_TICKS(2000));

    printf("--- Test Motor 2: freno ---\n");
    motor_brake(MOTOR_2);
    vTaskDelay(pdMS_TO_TICKS(500));
    motor_coast(MOTOR_2);

    printf("--- Test ambos motores a la vez: 60%% adelante por 3s ---\n");
    motor_set(MOTOR_1, MOTOR_FORWARD, 60);
    motor_set(MOTOR_2, MOTOR_FORWARD, 60);
    vTaskDelay(pdMS_TO_TICKS(3000));
    motor_coast(MOTOR_1);
    motor_coast(MOTOR_2);
    printf("--- Test motores completo ---\n\n");
    // -------------------------

    rtc_time_t now2;
    float yaw, pitch, roll;
    float lux[OPT3001_COUNT];
    while (1) {
        pcf2131_get_time(&now2);
        printf("Hora: %02d:%02d:%02d  Fecha: %02d/%02d/%04d\n",
               now2.hours, now2.minutes, now2.seconds,
               now2.day, now2.month, now2.year);

        for (int i = 0; i < 5; i++) {
            if (bno085_read_euler(&yaw, &pitch, &roll)) {
                printf("IMU  yaw=%.1f  pitch=%.1f  roll=%.1f  (deg)\n", yaw, pitch, roll);
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        if (opt3001_read_all(lux))
            printf("Luz: IC1=%.2f  IC2=%.2f  IC3=%.2f  IC4=%.2f  lux\n",
                   lux[0], lux[1], lux[2], lux[3]);

        vTaskDelay(pdMS_TO_TICKS(970));
    }
}
