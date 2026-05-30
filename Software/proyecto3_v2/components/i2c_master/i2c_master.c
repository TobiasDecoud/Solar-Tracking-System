#include "i2c_master.h"

#define I2C_TIMEOUT_MS 100

i2c_master_bus_handle_t i2c_bus_0;


// ============================================================
// INICIALIZAR EL BUS
// ============================================================
esp_err_t i2c_master_init(i2c_master_cfg_t cfg, i2c_master_bus_handle_t *bus_handle)
{
    i2c_master_bus_config_t bus_cfg = {
        .clk_source        = I2C_CLK_SRC_DEFAULT,
        .i2c_port          = cfg.port,
        .scl_io_num        = cfg.scl_pin,
        .sda_io_num        = cfg.sda_pin,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t ret = i2c_new_master_bus(&bus_cfg, bus_handle);
    if (ret == ESP_OK) i2c_bus_0 = *bus_handle;
    return ret;
}


// ============================================================
// AGREGAR UN DISPOSITIVO AL BUS
// ============================================================
esp_err_t i2c_master_add_device(i2c_master_bus_handle_t  bus_handle,
                                 i2c_device_cfg_t          cfg,
                                 i2c_master_dev_handle_t  *dev_handle)
{
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,  // La mayoria de dispositivos usan 7 bits
        .device_address  = cfg.device_address,
        .scl_speed_hz    = cfg.scl_speed_hz,
    };

    return i2c_master_bus_add_device(bus_handle, &dev_cfg, dev_handle);
}


// ============================================================
// ESCRIBIR AL DISPOSITIVO
// ============================================================
esp_err_t i2c_master_write_to_device(i2c_master_dev_handle_t dev_handle,
                                      const uint8_t          *data,
                                      size_t                  data_len)
{
    // i2c_master_transmit envia los bytes y espera el ACK del dispositivo
    return i2c_master_transmit(dev_handle, data, data_len, I2C_TIMEOUT_MS);
}


// ============================================================
// LEER DEL DISPOSITIVO
// ============================================================
esp_err_t i2c_master_read_from_device(i2c_master_dev_handle_t dev_handle,
                                       uint8_t                 write_reg,
                                       uint8_t                *read_buf,
                                       size_t                  read_len)
{
    // i2c_master_transmit_receive hace las dos operaciones en una sola transaccion:
    //   1. Envia el registro que queremos leer (write_reg)
    //   2. Lee read_len bytes en read_buf
    //
    // Esto es importante porque entre el paso 1 y 2 NO se libera el bus
    // (se usa un "repeated start"), lo cual es lo que esperan la mayoria
    // de sensores y RTCs.
    return i2c_master_transmit_receive(dev_handle,
                                       &write_reg, 1,
                                       read_buf,   read_len,
                                       I2C_TIMEOUT_MS);
}

