#pragma once
#include <stdint.h>
#include <stdbool.h>

#define RTC_SDA_PIN  47
#define RTC_SCL_PIN  48

#define PCF2131_ADDR        0x53

#define PCF2131_REG_CTRL1   0x00
#define PCF2131_REG_CTRL2   0x01
#define PCF2131_REG_CTRL3   0x02
#define PCF2131_REG_SR_RST  0x05
#define PCF2131_REG_100TH   0x06
#define PCF2131_REG_SEC     0x07
#define PCF2131_REG_MIN     0x08
#define PCF2131_REG_HRS     0x09
#define PCF2131_REG_DAYS    0x0A
#define PCF2131_REG_WDAYS   0x0B
#define PCF2131_REG_MON     0x0C
#define PCF2131_REG_YRS     0x0D

#define PCF2131_BIT_STOP    (1 << 5)
#define PCF2131_BIT_OSF     (1 << 7)

typedef struct {
    uint8_t  seconds;
    uint8_t  minutes;
    uint8_t  hours;
    uint8_t  day;
    uint8_t  weekday;
    uint8_t  month;
    uint16_t year;
} rtc_time_t;

void     pcf2131_init(void);
bool     pcf2131_is_valid(void);
void     pcf2131_get_time(rtc_time_t *t);
void     pcf2131_set_time(const rtc_time_t *t);
uint8_t  pcf2131_read_reg(uint8_t reg);
void     pcf2131_write_reg(uint8_t reg, uint8_t val);
