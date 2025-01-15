#pragma once

#include <driver/gpio.h>
#include <stdint.h>
#include <types.h>

static constexpr u32 DEEP_SLEEP_DISPLAY_INACTIVITY_MS = 60 * 1000;

// SeeedStudio display schematic:
// https://files.seeedstudio.com/wiki/round_display_for_xiao/SeeedStudio_Round_Display_for_XIAO_v1.1_SCH_230407.pdf

static constexpr uint8_t I2C_BME688_ADDR = 0x76;
static constexpr uint8_t I2C_CHSC6X_ADDR = 0x2e;

// NOTE: Make sure to turn the "1" switch of the double switch component on the
//  SeeedStudio display to the "ON" position, otherwise, this pin will be
//  floating.
static constexpr gpio_num_t BATTERY_READ_PIN = GPIO_NUM_9;

static constexpr gpio_num_t B_SDA = GPIO_NUM_8;
static constexpr gpio_num_t B_SCL = GPIO_NUM_7;

// NOTE: Make sure to turn the "2" switch of the double switch component on the
//  SeeedStudio display to the "KE" position, otherwise this pin will not be
//  connected to the backlight.
//
// The backlight has a 100k pullup. If a value on this pin should have effect
// during e.g. deep-sleep, make sure to either connect the pin to an RTC pin and
// enable an RTC pulldown or include a pulldown with a value of <100k.
static constexpr gpio_num_t DP_BL = GPIO_NUM_48;
static constexpr gpio_num_t DP_DC = GPIO_NUM_35;
static constexpr gpio_num_t DP_CS = GPIO_NUM_36;
static constexpr gpio_num_t DP_SCK = GPIO_NUM_13;
static constexpr gpio_num_t DP_MOSI = GPIO_NUM_17;
static constexpr gpio_num_t DP_MISO = GPIO_NUM_14;
static constexpr gpio_num_t DP_TOUCH_INT = GPIO_NUM_12;

static constexpr u16 DP_H_RES = 240;
static constexpr u16 DP_V_RES = 240;
static constexpr u16 DP_CMD_BIT_WIDTH = 8;
static constexpr u16 DP_PARAM_BIT_WIDTH = 8;
static constexpr u32 DP_PIXEL_CLOCK = 20 * 1000 * 1000; // 20 MHz
static constexpr size_t LVGL_DRAW_BUF_LINES = 100;

static constexpr u32 MISC_TASK_PRIORITY = 0;
static constexpr u32 DISPLAY_INACTIVITY_TASK_PRIORITY = 0;
static constexpr u32 ENV_TASK_PRIORITY = 1;
static constexpr u32 BATTERY_TASK_PRIORITY = 1;
static constexpr u32 LVGL_TASK_PRIORITY = 2;
// This needs to be greater than or equal to LVGL's priority, since we need to
// be executed periodically while LVGL is running to properly detect gestures.
static constexpr u32 LSM_TASK_PRIORITY = LVGL_TASK_PRIORITY;
