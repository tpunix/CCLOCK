#ifndef APP_HW_H
#define APP_HW_H

#include <stdint.h>

#include <board.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DEBUG_LOG
#undef  DEBUG_LOG

#define EPD_BUSY_PIN            IOID_10
#define EPD_RST_PIN             IOID_9
#define EPD_DC_PIN              IOID_8
#define EPD_CS_PIN              IOID_7
#define EPD_CLK_PIN             IOID_6
#define EPD_DAT_PIN             IOID_5

#define APP_LED_RED_PIN         IOID_13
#define APP_LED_BLUE_PIN        IOID_12

void app_hw_init(void);

void gpio_config(int index, int mode, int value);
void gpio_set(int index, int value);
int gpio_get(int index);
void delay_ms(int ms);

void led_set(int index, int value);
void led_toggle(int index);

#ifdef DEBUG_LOG
int __printk(const char *fmt, ...);
#define printk __printk
#else
#define printk(...)
#endif

int sprintk(char *buf, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* APP_HW_H */
