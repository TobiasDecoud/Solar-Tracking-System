#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"

extern i2c_master_bus_handle_t i2c_bus_0;

// ============================================================
// STRUCT DE CONFIGURACION DEL BUS
// Agrupa todo lo necesario para inicializar el bus I2C.
// Usamos un struct en vez de pasar 4 parametros sueltos.
// ============================================================
typedef struct {
    int          sda_pin;   // Pin GPIO para SDA
    int          scl_pin;   // Pin GPIO para SCL
    i2c_port_t   port;      // Puerto I2C: I2C_NUM_0 o I2C_NUM_1
} i2c_master_cfg_t;


// ============================================================
// STRUCT DE CONFIGURACION DEL DISPOSITIVO
// Cada dispositivo en el bus tiene su propia direccion y velocidad.
// Por eso esta separado de la configuracion del bus.
// ============================================================
typedef struct {
    uint16_t device_address; // Direccion I2C del dispositivo (ej: 0x68 para DS3231)
    uint32_t scl_speed_hz;   // Velocidad del clock (ej: 100000 para 100kHz)
} i2c_device_cfg_t;


// ============================================================
// INICIALIZAR EL BUS
//
// Configura el periferico I2C del ESP32 con los pines y puerto indicados.
// Devuelve un handle que representa ese bus — se usa en todas las
// operaciones posteriores.
//
// Retorna ESP_OK si todo salio bien, o un codigo de error de ESP-IDF.
// ============================================================
esp_err_t i2c_master_init(i2c_master_cfg_t cfg, i2c_master_bus_handle_t *bus_handle);


// ============================================================
// AGREGAR UN DISPOSITIVO AL BUS
//
// Registra un dispositivo en el bus ya inicializado.
// Cada dispositivo tiene su propio handle, que se usa para
// leer y escribir especificamente con ese dispositivo.
//
// Retorna ESP_OK si todo salio bien.
// ============================================================
esp_err_t i2c_master_add_device(i2c_master_bus_handle_t  bus_handle,
                                 i2c_device_cfg_t          cfg,
                                 i2c_master_dev_handle_t  *dev_handle);


// ============================================================
// ESCRIBIR AL DISPOSITIVO
//
// Envia una secuencia de bytes al dispositivo indicado por dev_handle.
//
// - data     : puntero al buffer con los bytes a enviar
// - data_len : cuantos bytes enviar
//
// Retorna ESP_OK si la escritura fue exitosa.
// ============================================================
esp_err_t i2c_master_write_to_device(i2c_master_dev_handle_t dev_handle,
                                      const uint8_t          *data,
                                      size_t                  data_len);


// ============================================================
// LEER DEL DISPOSITIVO
//
// Primero le indica al dispositivo que registro quiere leer (write_reg),
// luego lee la cantidad de bytes indicada en read_len.
//
// Este patron "escribir registro, luego leer" es el flujo tipico de I2C:
//   1. Master envia la direccion del registro que quiere
//   2. Dispositivo responde con el valor de ese registro
//
// - write_reg : registro que queremos leer (ej: 0x00 para segundos en DS3231)
// - read_buf  : puntero al buffer donde guardar la respuesta
// - read_len  : cuantos bytes leer
//
// Retorna ESP_OK si la lectura fue exitosa.
// ============================================================
esp_err_t i2c_master_read_from_device(i2c_master_dev_handle_t dev_handle,
                                       uint8_t                 write_reg,
                                       uint8_t                *read_buf,
                                       size_t                  read_len);