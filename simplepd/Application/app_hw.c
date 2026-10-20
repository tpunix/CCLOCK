#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

#include <ti/sysbios/knl/Clock.h>   // Clock_tickPeriod
#include <ti/sysbios/knl/Task.h>    // Task_sleep

#include <ti/drivers/GPIO.h>
#include <ti/drivers/PIN.h>
#include <ti/drivers/UART.h>

#include <driverlib/timer.h>
#include <driverlib/prcm.h>
#include <inc/hw_memmap.h>

#include <board.h>

#include "app_hw.h"

static UART_Handle uart = NULL;

static PIN_Handle appPinsHandle = NULL;
static PIN_State appPinsState;
static PIN_Config appPinsTable[] = {
    EPD_RST_PIN       | PIN_GPIO_OUTPUT_EN  | PIN_GPIO_HIGH | PIN_PUSHPULL | PIN_DRVSTR_MIN,
    EPD_CS_PIN        | PIN_GPIO_OUTPUT_EN  | PIN_GPIO_HIGH | PIN_PUSHPULL | PIN_DRVSTR_MIN,
    EPD_BUSY_PIN      | PIN_GPIO_OUTPUT_DIS | PIN_INPUT_EN  | PIN_NOPULL,
    EPD_DAT_PIN       | PIN_GPIO_OUTPUT_EN  | PIN_GPIO_HIGH | PIN_PUSHPULL | PIN_DRVSTR_MIN,
    EPD_CLK_PIN       | PIN_GPIO_OUTPUT_EN  | PIN_GPIO_HIGH | PIN_PUSHPULL | PIN_DRVSTR_MIN,
	EPD_DC_PIN        | PIN_GPIO_OUTPUT_EN  | PIN_GPIO_HIGH | PIN_PUSHPULL | PIN_DRVSTR_MIN,

	APP_LED_RED_PIN   | PIN_GPIO_OUTPUT_EN  | PIN_GPIO_LOW  | PIN_PUSHPULL | PIN_DRVSTR_MIN,
    APP_LED_BLUE_PIN  | PIN_GPIO_OUTPUT_EN  | PIN_GPIO_HIGH | PIN_PUSHPULL | PIN_DRVSTR_MIN,

	PIN_TERMINATE
};

void app_hw_init(void)
{

    //GPIO_init();

    if (appPinsHandle == NULL)
    {
        appPinsHandle = PIN_open(&appPinsState, appPinsTable);
    }

#ifdef DEBUG_LOG
    if (uart == NULL)
    {
        UART_init();

		UART_Params uartParams;
        UART_Params_init(&uartParams);

		uartParams.writeDataMode = UART_DATA_BINARY;
        uartParams.readDataMode = UART_DATA_BINARY;
        uartParams.readReturnMode = UART_RETURN_FULL;
        uartParams.readEcho = UART_ECHO_OFF;
        uartParams.baudRate = 115200;

        uart = UART_open(Board_UART0, &uartParams);
    }
#endif

    led_set(APP_LED_RED_PIN, 1);
    led_set(APP_LED_BLUE_PIN, 0);
    printk("app hw init\n");
}

void gpio_config(int index, int mode, int value)
{
    PIN_Config cfg = (PIN_Config)index;

    if (mode==0)
    {
        cfg |= PIN_GPIO_OUTPUT_DIS | PIN_INPUT_EN | PIN_NOPULL;
    }
    else
    {
        cfg |= PIN_GPIO_OUTPUT_EN | PIN_PUSHPULL | PIN_DRVSTR_MIN;
        if (value)
        {
            cfg |= PIN_GPIO_HIGH;
        }
    }

    if (appPinsHandle != NULL)
    {
        PIN_setConfig(appPinsHandle, PIN_BM_ALL, cfg);
    }
}

void gpio_set(int index, int value)
{
    if (appPinsHandle != NULL)
    {
        PIN_setOutputValue(appPinsHandle, (PIN_Id)index, value);
    }
}

int gpio_get(int index)
{
    return PIN_getInputValue((PIN_Id)index);
}

void delay_ms(int ms)
{
	Task_sleep((ms*1000)/Clock_tickPeriod);
}

void led_set(int index, int value)
{
    gpio_set(index, value);
}

void led_toggle(int index)
{
    if (appPinsHandle != NULL)
    {
        PIN_setOutputValue(appPinsHandle, (PIN_Id)index,
                           !PIN_getOutputValue((PIN_Id)index));
    }
}


/******************************************************************************/


static char *format_buf;

static void format_write_char(char ch)
{
    if (format_buf != NULL)
    {
        *format_buf++ = ch;
        return;
    }

    if (uart == NULL)
    {
        return;
    }

    if (ch == '\n')
    {
        char cr = '\r';
        UART_write(uart, &cr, 1);
    }

    UART_write(uart, &ch, 1);
}

static int format_write_str(const char *str)
{
    int len = 0;

    if (str == NULL)
    {
        str = "(null)";
    }

    while (*str)
    {
        format_write_char(*str++);
        len++;
    }

    return len;
}

static int format_write_uint(unsigned int value, unsigned int base, int width, char pad)
{
    char buf[10];
    int i = 0;
    int len = 0;

    do
    {
        unsigned int digit = value % base;
        buf[i++] = (char)((digit < 10) ? ('0' + digit) : ('a' + digit - 10));
        value /= base;
    } while (value && i < (int)sizeof(buf));

    while (i < width)
    {
        format_write_char(pad);
        len++;
        width--;
    }

    while (i > 0)
    {
        format_write_char(buf[--i]);
        len++;
    }

    return len;
}

static int vformat_write(const char *fmt, va_list args)
{
    int len = 0;

    if (fmt == NULL)
    {
        return 0;
    }

    while (*fmt)
    {
        int width = 0;
        char pad = ' ';

        if (*fmt != '%')
        {
            format_write_char(*fmt++);
            len++;
            continue;
        }

        fmt++;
        if (*fmt == '0')
        {
            pad = '0';
            fmt++;
        }

        while (*fmt >= '0' && *fmt <= '9')
        {
            width = (width * 10) + (*fmt++ - '0');
        }

        switch (*fmt)
        {
        case 'd':
        case 'i':
            len += format_write_uint(va_arg(args, int), 10, width, pad);
            break;
        case 'x':
            len += format_write_uint(va_arg(args, unsigned int), 16, width, pad);
            break;
        case 's':
            len += format_write_str(va_arg(args, const char *));
            break;
        case '%':
            format_write_char('%');
            len++;
            break;
        default:
            format_write_char('%');
            len++;
            if (*fmt)
            {
                format_write_char(*fmt);
                len++;
            }
            break;
        }

        if (*fmt)
        {
            fmt++;
        }
    }

    return len;
}

int __printk(const char *fmt, ...)
{
    va_list args;
    int len;

    format_buf = NULL;

    va_start(args, fmt);
    len = vformat_write(fmt, args);
    va_end(args);

    return len;
}

int sprintk(char *buf, const char *fmt, ...)
{
    va_list args;
    int len;

    if (buf == NULL)
    {
        return 0;
    }

    format_buf = buf;

    va_start(args, fmt);
    len = vformat_write(fmt, args);
    va_end(args);

    *format_buf = '\0';
    format_buf = NULL;

    return len;
}

/******************************************************************************/
