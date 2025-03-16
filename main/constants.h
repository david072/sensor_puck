#pragma once

#include <driver/gpio.h>
#include <driver/ledc.h>
#include <stdint.h>
#include <types.h>

#define BASE_PATH "/home"

#define USING_CUSTOM_PCB true

static constexpr u32 DEEP_SLEEP_DISPLAY_INACTIVITY_MS = 60 * 1000;

// From a publication of the Umweltbundesamt on indoor CO2 levels (p. 1368,
// table 4)
// https://www.umweltbundesamt.de/sites/default/files/medien/pdfs/kohlendioxid_2008.pdf
static constexpr u32 EXCELLENT_CO2_PPM_LIMIT = 400;
static constexpr u32 FINE_CO2_PPM_LIMIT = 1000;
static constexpr u32 MODERATE_CO2_PPM_LIMIT = 1500;
static constexpr u32 POOR_CO2_PPM_LIMIT = 2000;
static constexpr u32 VERY_POOR_CO2_PPM_LIMIT = 5000;

// SeeedStudio display schematic:
// https://files.seeedstudio.com/wiki/round_display_for_xiao/SeeedStudio_Round_Display_for_XIAO_v1.1_SCH_230407.pdf

static constexpr uint8_t I2C_BME688_ADDR = 0x76;
static constexpr uint8_t I2C_CHSC6X_ADDR = 0x2e;

// NOTE: Make sure to turn the "1" switch of the double switch component on the
//  SeeedStudio display to the "ON" position, otherwise, this pin will be
//  floating.
#if USING_CUSTOM_PCB
static constexpr gpio_num_t BATTERY_READ_PIN = GPIO_NUM_10;
static constexpr gpio_num_t BUZZER_PIN = GPIO_NUM_9;

static constexpr gpio_num_t B_SDA = GPIO_NUM_1;
static constexpr gpio_num_t B_SCL = GPIO_NUM_2;

static constexpr gpio_num_t B_LCD_SDA = GPIO_NUM_8;
static constexpr gpio_num_t B_LCD_SCL = GPIO_NUM_7;

// NOTE: Make sure to turn the "2" switch of the double switch component on the
//  SeeedStudio display to the "KE" position, otherwise this pin will not be
//  connected to the backlight.
//
// The backlight has a 100k pullup. If a value on this pin should have effect
// during e.g. deep-sleep, make sure to either connect the pin to an RTC pin and
// enable an RTC pulldown or include a pulldown with a value of <100k.
/// Custom PCB
static constexpr gpio_num_t DP_BL = GPIO_NUM_48;
static constexpr gpio_num_t DP_DC = GPIO_NUM_35;
static constexpr gpio_num_t DP_CS = GPIO_NUM_37;
static constexpr gpio_num_t DP_SCK = GPIO_NUM_13;
static constexpr gpio_num_t DP_MOSI = GPIO_NUM_17;
static constexpr gpio_num_t DP_MISO = GPIO_NUM_12;
static constexpr gpio_num_t DP_TOUCH_INT = GPIO_NUM_14;
#else
static constexpr gpio_num_t BATTERY_READ_PIN = GPIO_NUM_1;

/// SeeedStudio XIAO ESP32S3
static constexpr gpio_num_t B_SDA = GPIO_NUM_5;
static constexpr gpio_num_t B_SCL = GPIO_NUM_6;

// static constexpr gpio_num_t DP_BL = GPIO_NUM_43;
// static constexpr gpio_num_t DP_DC = GPIO_NUM_4;
// static constexpr gpio_num_t DP_CS = GPIO_NUM_2;
// static constexpr gpio_num_t DP_SCK = GPIO_NUM_7;
// static constexpr gpio_num_t DP_MOSI = GPIO_NUM_9;
// static constexpr gpio_num_t DP_MISO = GPIO_NUM_8;
// static constexpr gpio_num_t DP_TOUCH_INT = GPIO_NUM_44;
#endif

static constexpr u16 DP_H_RES = 240;
static constexpr u16 DP_V_RES = 240;
static constexpr u16 DP_CMD_BIT_WIDTH = 8;
static constexpr u16 DP_PARAM_BIT_WIDTH = 8;
static constexpr u32 DP_PIXEL_CLOCK = 20 * 1000 * 1000; // 20 MHz
static constexpr size_t LVGL_DRAW_BUF_LINES = 100;

constexpr ledc_mode_t BUZZER_LEDC_MODE = LEDC_LOW_SPEED_MODE;
constexpr ledc_timer_bit_t BUZZER_LEDC_DUTY_RES = LEDC_TIMER_13_BIT;
constexpr ledc_timer_t BUZZER_LEDC_TIMER = LEDC_TIMER_0;
constexpr ledc_clk_cfg_t BUZZER_LEDC_CLK = LEDC_AUTO_CLK;
constexpr ledc_channel_t BUZZER_LEDC_CHANNEL = LEDC_CHANNEL_0;

static constexpr u32 MISC_TASK_PRIORITY = 0;
static constexpr u32 ENV_TASK_PRIORITY = 1;
static constexpr u32 BATTERY_TASK_PRIORITY = 1;
static constexpr u32 LVGL_TASK_PRIORITY = 2;
// This needs to be greater than or equal to LVGL's priority, since we need to
// be executed periodically while LVGL is running to properly detect gestures.
static constexpr u32 LSM_TASK_PRIORITY = LVGL_TASK_PRIORITY;
static constexpr u32 BEGIN_DEEP_SLEEP_PRIORITY = 10;
