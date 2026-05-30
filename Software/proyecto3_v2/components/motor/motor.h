#pragma once

#include <stdint.h>
#include "esp_err.h"

// Indices de motor
#define MOTOR_1  0
#define MOTOR_2  1

// Pines
#define MOTOR1_IN1  3
#define MOTOR1_IN2  46
#define MOTOR2_IN1  9
#define MOTOR2_IN2  10

typedef enum {
    MOTOR_FORWARD,   // IN1=PWM, IN2=0    — gira hacia adelante
    MOTOR_BACKWARD,  // IN1=0,   IN2=PWM  — gira hacia atras
    MOTOR_COAST,     // IN1=0,   IN2=0    — suelta (gira por inercia)
    MOTOR_BRAKE,     // IN1=MAX, IN2=MAX  — freno activo
} motor_dir_t;

// Inicializa el periferico LEDC y los 4 canales PWM
esp_err_t motor_init(void);

// Controla un motor. motor: MOTOR_1 o MOTOR_2
//                          speed_pct: 0-100 (se ignora en COAST y BRAKE)
void motor_set(int motor, motor_dir_t dir, uint8_t speed_pct);

// Atajos
void motor_coast(int motor);
void motor_brake(int motor);
