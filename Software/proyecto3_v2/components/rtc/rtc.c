#include "rtc.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "esp_log.h"

static const char *TAG = "PCF2131";

// ---------------------------------------------------------------------------
// Software I2C  (~100 kHz, open-drain)
// ---------------------------------------------------------------------------
#define I2C_HALF_US  5

static inline void scl_hi(void) { gpio_set_level(RTC_SCL_PIN, 1); }
static inline void scl_lo(void) { gpio_set_level(RTC_SCL_PIN, 0); }
static inline void sda_hi(void) { gpio_set_level(RTC_SDA_PIN, 1); }
static inline void sda_lo(void) { gpio_set_level(RTC_SDA_PIN, 0); }
static inline int  sda_rd(void) { return gpio_get_level(RTC_SDA_PIN); }
static inline void dly(void)    { esp_rom_delay_us(I2C_HALF_US); }

static void sw_start(void)
{
    sda_hi(); scl_hi(); dly();
    sda_lo(); dly();        // SDA baja con SCL en alto = START
    scl_lo(); dly();
}

static void sw_stop(void)
{
    sda_lo(); dly();
    scl_hi(); dly();
    sda_hi(); dly();        // SDA sube con SCL en alto = STOP
}

static bool sw_write_byte(uint8_t b)
{
    for (int i = 7; i >= 0; i--) {
        (b >> i) & 1 ? sda_hi() : sda_lo();
        dly(); scl_hi(); dly(); scl_lo();
    }
    sda_hi();               // liberar SDA para que el esclavo ponga ACK
    dly(); scl_hi(); dly();
    bool ack = (sda_rd() == 0);
    scl_lo(); dly();
    return ack;
}

static uint8_t sw_read_byte(bool send_ack)
{
    uint8_t b = 0;
    sda_hi();               // liberar SDA
    for (int i = 7; i >= 0; i--) {
        dly(); scl_hi(); dly();
        if (sda_rd()) b |= (1 << i);
        scl_lo();
    }
    send_ack ? sda_lo() : sda_hi();
    dly(); scl_hi(); dly(); scl_lo();
    sda_hi();
    return b;
}

// ---------------------------------------------------------------------------
// Acceso a registros PCF2131
// ---------------------------------------------------------------------------
static uint8_t bcd_to_dec(uint8_t bcd) { return (bcd >> 4) * 10 + (bcd & 0x0F); }
static uint8_t dec_to_bcd(uint8_t dec) { return ((dec / 10) << 4) | (dec % 10); }

uint8_t pcf2131_read_reg(uint8_t reg)
{
    sw_start();
    sw_write_byte((PCF2131_ADDR << 1) | 0);  // direccion + write
    sw_write_byte(reg);
    sw_start();                               // repeated start
    sw_write_byte((PCF2131_ADDR << 1) | 1);  // direccion + read
    uint8_t val = sw_read_byte(false);        // NACK en ultimo byte
    sw_stop();
    return val;
}

void pcf2131_write_reg(uint8_t reg, uint8_t val)
{
    sw_start();
    sw_write_byte((PCF2131_ADDR << 1) | 0);
    sw_write_byte(reg);
    sw_write_byte(val);
    sw_stop();
}

// ---------------------------------------------------------------------------
// API publica
// ---------------------------------------------------------------------------
void pcf2131_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << RTC_SDA_PIN) | (1ULL << RTC_SCL_PIN),
        .mode         = GPIO_MODE_INPUT_OUTPUT_OD,  // open-drain
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);
    sda_hi(); scl_hi();     // liberar bus
    ESP_LOGI(TAG, "PCF2131 inicializado en 0x%02X (SW I2C pin SDA=%d SCL=%d)",
             PCF2131_ADDR, RTC_SDA_PIN, RTC_SCL_PIN);
}

bool pcf2131_is_valid(void)
{
    uint8_t sec = pcf2131_read_reg(PCF2131_REG_SEC);
    bool valid = !(sec & PCF2131_BIT_OSF);
    if (!valid) ESP_LOGW(TAG, "OSF activo: la hora no es confiable");
    return valid;
}

void pcf2131_get_time(rtc_time_t *t)
{
    t->seconds = bcd_to_dec(pcf2131_read_reg(PCF2131_REG_SEC)   & 0x7F);
    t->minutes = bcd_to_dec(pcf2131_read_reg(PCF2131_REG_MIN)   & 0x7F);
    t->hours   = bcd_to_dec(pcf2131_read_reg(PCF2131_REG_HRS)   & 0x3F);
    t->day     = bcd_to_dec(pcf2131_read_reg(PCF2131_REG_DAYS)  & 0x3F);
    t->weekday = bcd_to_dec(pcf2131_read_reg(PCF2131_REG_WDAYS) & 0x07);
    t->month   = bcd_to_dec(pcf2131_read_reg(PCF2131_REG_MON)   & 0x1F);
    t->year    = bcd_to_dec(pcf2131_read_reg(PCF2131_REG_YRS))  + 2000;
}

void pcf2131_set_time(const rtc_time_t *t)
{
    uint8_t ctrl1 = pcf2131_read_reg(PCF2131_REG_CTRL1);
    pcf2131_write_reg(PCF2131_REG_CTRL1, ctrl1 | PCF2131_BIT_STOP);

    pcf2131_write_reg(PCF2131_REG_SEC,   dec_to_bcd(t->seconds));
    pcf2131_write_reg(PCF2131_REG_MIN,   dec_to_bcd(t->minutes));
    pcf2131_write_reg(PCF2131_REG_HRS,   dec_to_bcd(t->hours));
    pcf2131_write_reg(PCF2131_REG_DAYS,  dec_to_bcd(t->day));
    pcf2131_write_reg(PCF2131_REG_WDAYS, dec_to_bcd(t->weekday));
    pcf2131_write_reg(PCF2131_REG_MON,   dec_to_bcd(t->month));
    pcf2131_write_reg(PCF2131_REG_YRS,   dec_to_bcd(t->year - 2000));

    pcf2131_write_reg(PCF2131_REG_CTRL1, ctrl1 & ~PCF2131_BIT_STOP);

    ESP_LOGI(TAG, "Hora seteada: %02d:%02d:%02d  %02d/%02d/%04d",
             t->hours, t->minutes, t->seconds,
             t->day, t->month, t->year);
}
