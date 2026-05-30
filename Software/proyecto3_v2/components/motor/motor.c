#include "motor.h"
#include "driver/ledc.h"
#include "esp_log.h"

#define TAG "MOTOR"

#define PWM_FREQ_HZ    5000
#define PWM_RES        LEDC_TIMER_10_BIT
#define PWM_MAX        ((1 << 10) - 1)   // 1023

// Cada motor tiene dos canales LEDC (uno por pin)
static const struct {
    int             in1_pin, in2_pin;
    ledc_channel_t  ch1,     ch2;
} cfg[2] = {
    { MOTOR1_IN1, MOTOR1_IN2, LEDC_CHANNEL_0, LEDC_CHANNEL_1 },
    { MOTOR2_IN1, MOTOR2_IN2, LEDC_CHANNEL_2, LEDC_CHANNEL_3 },
};


esp_err_t motor_init(void)
{
    ledc_timer_config_t timer = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .timer_num       = LEDC_TIMER_0,
        .duty_resolution = PWM_RES,
        .freq_hz         = PWM_FREQ_HZ,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    esp_err_t r = ledc_timer_config(&timer);
    if (r != ESP_OK) {
        ESP_LOGE(TAG, "Error configurando timer: %s", esp_err_to_name(r));
        return r;
    }

    ledc_channel_config_t ch = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_sel  = LEDC_TIMER_0,
        .intr_type  = LEDC_INTR_DISABLE,
        .duty       = 0,
        .hpoint     = 0,
    };

    for (int m = 0; m < 2; m++) {
        ch.channel  = cfg[m].ch1;
        ch.gpio_num = cfg[m].in1_pin;
        r = ledc_channel_config(&ch);
        if (r != ESP_OK) { ESP_LOGE(TAG, "Motor %d IN1: %s", m+1, esp_err_to_name(r)); return r; }

        ch.channel  = cfg[m].ch2;
        ch.gpio_num = cfg[m].in2_pin;
        r = ledc_channel_config(&ch);
        if (r != ESP_OK) { ESP_LOGE(TAG, "Motor %d IN2: %s", m+1, esp_err_to_name(r)); return r; }
    }

    ESP_LOGI(TAG, "Motores listos — %d Hz, 10 bits", PWM_FREQ_HZ);
    return ESP_OK;
}


static void set_ch(ledc_channel_t ch, uint32_t duty)
{
    ledc_set_duty(LEDC_LOW_SPEED_MODE, ch, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, ch);
}

void motor_set(int motor, motor_dir_t dir, uint8_t speed_pct)
{
    if (motor < 0 || motor > 1) return;
    if (speed_pct > 100) speed_pct = 100;
    uint32_t duty = ((uint32_t)speed_pct * PWM_MAX) / 100;

    switch (dir) {
        case MOTOR_FORWARD:
            set_ch(cfg[motor].ch1, duty);
            set_ch(cfg[motor].ch2, 0);
            break;
        case MOTOR_BACKWARD:
            set_ch(cfg[motor].ch1, 0);
            set_ch(cfg[motor].ch2, duty);
            break;
        case MOTOR_COAST:
            set_ch(cfg[motor].ch1, 0);
            set_ch(cfg[motor].ch2, 0);
            break;
        case MOTOR_BRAKE:
            set_ch(cfg[motor].ch1, PWM_MAX);
            set_ch(cfg[motor].ch2, PWM_MAX);
            break;
    }
}

void motor_coast(int motor) { motor_set(motor, MOTOR_COAST, 0); }
void motor_brake(int motor) { motor_set(motor, MOTOR_BRAKE, 0); }
