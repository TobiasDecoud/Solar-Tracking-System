#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "driver/i2c_master.h"
#include "esp_err.h"

#define BNO085_NRST_PIN   39
#define BNO085_BOOTN_PIN  40

// Inicializa el IMU: reset GPIO, registra en el bus, lee Product ID, habilita rotation vector
esp_err_t bno085_init(i2c_master_bus_handle_t bus);

// Lee el proximo paquete I2C y extrae angulos de orientacion si hay uno disponible.
// yaw: rotacion en Z (0..360 deg), pitch: inclinacion adelante/atras, roll: inclinacion lateral.
// Retorna true si los valores fueron actualizados, false si no habia dato nuevo.
bool bno085_read_euler(float *yaw, float *pitch, float *roll);
