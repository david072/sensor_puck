#pragma once

#include <Arduino.h>

static constexpr uint8_t I2C_BME688_ADDR = 0x76;
static constexpr uint8_t CHSC6X_I2C_ID = 0x2e;

static constexpr gpio_num_t BATTERY_READ_PIN = GPIO_NUM_9;

static constexpr gpio_num_t B_SDA = GPIO_NUM_8;
static constexpr gpio_num_t B_SCL = GPIO_NUM_7;

static constexpr gpio_num_t DP_BL = GPIO_NUM_48;
static constexpr gpio_num_t DP_DC = GPIO_NUM_35;
static constexpr gpio_num_t DP_CS = GPIO_NUM_36;
static constexpr gpio_num_t DP_SCK = GPIO_NUM_13;
static constexpr gpio_num_t DP_MOSI = GPIO_NUM_17;
static constexpr gpio_num_t DP_MISO = GPIO_NUM_14;
static constexpr gpio_num_t DP_TOUCH_INT = GPIO_NUM_12;
