#pragma once

#include <constants.h>
#include <driver/i2c_master.h>
#include <driver/spi_master.h>

constexpr spi_host_device_t DP_HOST = SPI2_HOST;
constexpr u32 LVGL_TICK_PERIOD_MS = 2;

constexpr u32 LVGL_TASK_STACK_SIZE = 8 * 1024;

void init_display();
void init_display_touch(i2c_master_bus_handle_t i2c_handle);

void clear_display();

void set_display_backlight(bool enable);
