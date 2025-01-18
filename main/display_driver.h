#pragma once

#include <constants.h>
#include <driver/i2c_master.h>
#include <driver/spi_master.h>
#include <esp_lcd_panel_io.h>

constexpr spi_host_device_t DP_HOST = SPI2_HOST;
constexpr u32 LVGL_TICK_PERIOD_MS = 2;

constexpr u32 LVGL_TASK_STACK_SIZE = 8 * 1024;

constexpr u32 GC9A01_CMD_ENTER_SLEEP_MODE = 0x10;

static esp_lcd_panel_io_handle_t g_panel_io_handle;
static esp_lcd_panel_handle_t g_panel_handle;

void init_display();
void init_display_touch(i2c_master_bus_handle_t i2c_handle);
void display_enter_sleep_mode();

void clear_display();

void set_display_backlight(bool enable);
