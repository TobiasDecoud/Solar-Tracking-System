#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "driver/i2c_master.h"
#include "esp_err.h"

#define OPT3001_COUNT  4

// Inicializa los 4 sensores en el bus dado y arranca conversion continua
esp_err_t opt3001_init(i2c_master_bus_handle_t bus);

// Lee los 4 sensores. lux[i] en lux; -1.0 si el sensor i fallo.
// Retorna true si al menos uno respondio.
bool opt3001_read_all(float lux[OPT3001_COUNT]);
