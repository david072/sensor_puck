#pragma once

#include <Arduino.h>

// SeeedStudio display schematic:
// https://files.seeedstudio.com/wiki/round_display_for_xiao/SeeedStudio_Round_Display_for_XIAO_v1.1_SCH_230407.pdf

static constexpr uint8_t I2C_BME688_ADDR = 0x76;
static constexpr uint8_t CHSC6X_I2C_ID = 0x2e;

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
