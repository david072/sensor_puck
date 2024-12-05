#pragma once

#include "data.h"
#include <host/ble_hs.h>
#include <services/gap/ble_svc_gap.h>
#include <services/gatt/ble_svc_gatt.h>
#include <types.h>

int access_cb(u16 conn_handle, u16 attr_handle, ble_gatt_access_ctxt* ctx,
              void* arg);

static u16 date_time_attr_handle;

/// UUIDs generated using https://www.uuidgenerator.net/

ble_uuid128_t const SYSTEM_SVC_UUID =
    BLE_UUID128_INIT(0x71, 0x41, 0x49, 0x15, 0x6d, 0x78, 0x47, 0x53, 0xb7, 0x2f,
                     0x35, 0x98, 0x11, 0xbd, 0x0f, 0xda);

ble_uuid128_t const DATE_TIME_CHR_UUID =
    BLE_UUID128_INIT(0x9e, 0x2f, 0x59, 0x94, 0x61, 0x20, 0x40, 0x8f, 0xae, 0x37,
                     0x2d, 0xb9, 0x33, 0xbd, 0xa1, 0x11);

ble_gatt_svc_def const GATT_SERVER_SERVICES[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &SYSTEM_SVC_UUID.u,
        .characteristics =
            (ble_gatt_chr_def[]){
                // system date & time
                {
                    .uuid = &DATE_TIME_CHR_UUID.u,
                    .access_cb = access_cb,
                    .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE |
                             BLE_GATT_CHR_F_WRITE_NO_RSP,
                    .val_handle = &date_time_attr_handle,
                },
                // no more characteristics
                {0},
            },
    },
    // no more services
    {0},
};

/// Manages BLE peripheral access using NimBLE.
/// This will begin advertising for the specified duration (or forever if not
/// provided) and stop once the first device has connected. Once that device
/// disconnects, it will disable Bluetooth again.
/// When enabling and disabling Bluetooth, it will post the appropriate events
/// to the default event loop.
class BlePeripheralManager {
public:
  static BlePeripheralManager& the();

  BlePeripheralManager();

  void start(u32 advertisement_duration_ms = BLE_HS_FOREVER);
  void stop();

  bool started() const;
  bool connected() const { return m_connected; }

private:
  /// Amount of time to wait after the device disconnected before disabling BLE.
  /// If we try to stop NimBLE immediately in the GAP event handler,
  /// nimble_port_stop() blocks, which I attribute to the connection not being
  /// closed properly and thus it waiting for that to happen. However, since the
  /// event handler is blocking execution, we are deadlocked. Therefore the
  /// event handler spawns a task that waits this amount of time before
  /// executing stop().
  static constexpr u32 BLE_STOP_GRACE_PERIOD_AFTER_DEVICE_DISCONNECT_MS = 1000;

  int on_gap_event(ble_gap_event* event, void* arg);

  void gatt_server_init();
  void begin_advertising();

  u32 m_advertisement_duration_ms = BLE_HS_FOREVER;
  bool m_started = false;
  SemaphoreHandle_t m_mutex;

  bool m_connected = false;
};
