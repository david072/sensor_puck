#include "ble_peripheral_manager.h"
#include <constants.h>
#include <host/ble_hs.h>
#include <modlog/modlog.h>
#include <nimble/ble.h>
#include <nimble/nimble_port.h>
#include <nimble/nimble_port_freertos.h>
#include <os/endian.h>
#include <services/gap/ble_svc_gap.h>
#include <wifi_manager.h>

#include <data.h>
#include <esp_event.h>

char const* addr_str(u8* addr) {
  // 2 chars * 6 fields + 5x ':' + \0
  static char buf[2 * 6 + 5 + 1];
  sprintf(buf, "%02x:%02x:%02x:%02x:%02x:%02x", addr[5], addr[4], addr[3],
          addr[2], addr[1], addr[0]);
  return buf;
}

// TODO: Properly handle host endianness here. Since ESP ist LE and BLE
// communication is also LE, this works at the moment, but would break on
// a BE system.
int access_cb(u16 conn_handle, u16 attr_handle, ble_gatt_access_ctxt* ctx,
              void* arg) {
#define ASSERT_OR_RETURN(x, err)                                               \
  if (!(x)) {                                                                  \
    ESP_LOGE("BLE(access_cb)",                                                 \
             "Assert failed at %s:%d. Returning " #err "\nAssert " #x          \
             " failed.",                                                       \
             __FILE__, __LINE__);                                              \
    return err;                                                                \
  }

  switch (ctx->op) {
  case BLE_GATT_ACCESS_OP_WRITE_CHR: {
    if (attr_handle == date_time_attr_handle) {
      u16 len = os_mbuf_len(ctx->om);
      if (len > sizeof(time_t)) {
        return BLE_ATT_ERR_VALUE_NOT_ALLOWED;
      }

      time_t time = 0;

      ASSERT_OR_RETURN(os_mbuf_copydata(ctx->om, 0, len, &time) == 0,
                       BLE_ATT_ERR_UNLIKELY);
      Data::set_time(time);
      return 0;
    } else if (attr_handle == wifi_ssid_attr_handle) {
      u16 len = os_mbuf_len(ctx->om);
      std::string s(len, '-');
      ASSERT_OR_RETURN(os_mbuf_copydata(ctx->om, 0, len, s.data()) == 0,
                       BLE_ATT_ERR_UNLIKELY);
      WifiManager::the().set_ssid(s);
      return 0;
    } else if (attr_handle == wifi_password_attr_handle) {
      u16 len = os_mbuf_len(ctx->om);
      std::string s(len, '-');
      ASSERT_OR_RETURN(os_mbuf_copydata(ctx->om, 0, len, s.data()) == 0,
                       BLE_ATT_ERR_UNLIKELY);
      ESP_LOGI("ble", "setting wifi password to %s", s.c_str());
      WifiManager::the().set_password(s);
      return 0;
    }

    return BLE_ATT_ERR_ATTR_NOT_FOUND;
  }
  case BLE_GATT_ACCESS_OP_READ_CHR: {
    if (attr_handle == date_time_attr_handle) {
      // return little-endian
      auto unix_ts = htole64(time(NULL));
      ASSERT_OR_RETURN(os_mbuf_append(ctx->om, &unix_ts, sizeof(unix_ts)) == 0,
                       BLE_ATT_ERR_UNLIKELY);
      return 0;
    } else if (attr_handle == wifi_ssid_attr_handle) {
      auto ssid = WifiManager::the().ssid();
      if (!ssid)
        return 0;
      ASSERT_OR_RETURN(os_mbuf_append(ctx->om, ssid->data(), ssid->length()) ==
                           0,
                       BLE_ATT_ERR_UNLIKELY);
      return 0;
    }

    return BLE_ATT_ERR_ATTR_NOT_FOUND;
  }
  }
  return 0;

#undef ASSERT_OR_RETURN
}

void ble_peripheral_host_task(void* arg) {
  ESP_LOGI("BLE", "BLE host task started");
  // this function will return when `nimble_port_stop()` is executed
  nimble_port_run();
  nimble_port_freertos_deinit();
}

BlePeripheralManager& BlePeripheralManager::the() {
  static BlePeripheralManager bpm;
  return bpm;
}

BlePeripheralManager::BlePeripheralManager() {
  m_mutex = xSemaphoreCreateMutex();
}

void BlePeripheralManager::start(u32 advertisement_duration_ms) {
  lock();

  if (m_started) {
    unlock();
    return;
  }
  m_started = true;
  m_advertisement_duration_ms = advertisement_duration_ms;

  initialize_nvs_flash();

  ESP_ERROR_CHECK(nimble_port_init());

  ble_hs_cfg.reset_cb = [](int reason) {
    ESP_LOGE("BLE", "Resetting state; reason: %d", reason);
  };
  ble_hs_cfg.sync_cb = []() {
    BlePeripheralManager::the().begin_advertising();
  };
  ble_hs_cfg.gatts_register_cb = [](ble_gatt_register_ctxt*, void*) {};
  ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

  gatt_server_init();

  // https://github.com/espressif/esp-idf/blob/c8fc5f643b7a7b0d3b182d3df610844e3dc9bd74/examples/bluetooth/nimble/ble_multi_conn/ble_multi_conn_prph/main/main.c#L300
  // "Need to have template for store" ???
  // ble_store_config_init();

  nimble_port_freertos_init(ble_peripheral_host_task);

  lock();

  esp_event_post(DATA_EVENT_BASE, Data::Event::BluetoothEnabled, NULL, 0,
                 portMAX_DELAY);

  ESP_LOGI("BLE", "Bluetooth enabled. Advertising for %ld ms...",
           advertisement_duration_ms);
}

void BlePeripheralManager::stop() {
  lock();
  if (!m_started) {
    unlock();
    return;
  }

  ble_gatts_stop();
  ble_gap_adv_stop();
  nimble_port_stop();
  nimble_port_deinit();

  m_connected = false;
  m_started = false;
  unlock();

  esp_event_post(DATA_EVENT_BASE, Data::Event::BluetoothDisabled, NULL, 0,
                 portMAX_DELAY);

  ESP_LOGI("BLE", "Bluetooth disabled!");
}

bool BlePeripheralManager::started() const {
  lock();
  auto res = m_started;
  unlock();
  return res;
}

void BlePeripheralManager::gatt_server_init() {
  ble_svc_gap_init();
  ble_svc_gatt_init();

  assert(ble_gatts_count_cfg(GATT_SERVER_SERVICES) == 0);
  assert(ble_gatts_add_svcs(GATT_SERVER_SERVICES) == 0);
}

void BlePeripheralManager::begin_advertising() {
  // TODO: if too many connections (i.e. more than 1 ig?!), return

  if (ble_gap_adv_active())
    return;

  ble_gap_adv_params adv_params;
  ble_hs_adv_fields fields;
  ble_addr_t addr;

  assert(ble_hs_id_gen_rnd(0, &addr) == 0);
  assert(ble_hs_id_set_rnd(addr.val) == 0);

  // set advertising fields
  memset(&fields, 0, sizeof(fields));

  fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
  // include the TX power level field and fill it automatically
  fields.tx_pwr_lvl_is_present = 1;
  fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

  // auto* name = ble_svc_gap_device_name();
  auto* name = "Sensor Puck";
  fields.name = reinterpret_cast<u8 const*>(static_cast<char const*>(name));
  fields.name_len = strlen(name);
  fields.name_is_complete = 1;

  auto* mfg_data = "d72sp";
  fields.mfg_data =
      reinterpret_cast<u8 const*>(static_cast<char const*>(mfg_data));
  fields.mfg_data_len = strlen(mfg_data);

  assert(ble_gap_adv_set_fields(&fields) == 0);

  // begin advertising
  memset(&adv_params, 0, sizeof(adv_params));
  adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
  adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
  assert(ble_gap_adv_start(
             BLE_OWN_ADDR_RANDOM, NULL, m_advertisement_duration_ms,
             &adv_params,
             [](ble_gap_event* event, void* arg) -> int {
               return BlePeripheralManager::the().on_gap_event(event, arg);
             },
             NULL) == 0);

  ESP_LOGI("BLE", "Advertisement started! Device address: %s",
           addr_str(addr.val));
}

int BlePeripheralManager::on_gap_event(ble_gap_event* event, void* arg) {
  switch (event->type) {
  case BLE_GAP_EVENT_CONNECT: {
    if (event->connect.status == 0) {
      ESP_LOGI("BLE", "Connection established!");
      m_connected = true;

      esp_event_post(DATA_EVENT_BASE, Data::Event::BluetoothConnected, NULL, 0,
                     portMAX_DELAY);
      break;
    }

    begin_advertising();
    break;
  }
  case BLE_GAP_EVENT_DISCONNECT: {
    ESP_LOGI("BLE", "Device disconnected. Disabling Bluetooth");
    m_connected = false;
    // Add grace period before disabling Bluetooth to allow NimBLE to fully
    // close the connection or something. If we call stop() here immediately, it
    // blocks on nimble_port_stop(), which I assume to be due to the connection
    // not being closed properly because of the event handler blocking
    // execution.
    xTaskCreate(
        [](void*) {
          vTaskDelay(
              pdMS_TO_TICKS(BLE_STOP_GRACE_PERIOD_AFTER_DEVICE_DISCONNECT_MS));
          BlePeripheralManager::the().stop();
          vTaskDelete(NULL);
        },
        "BLE dis dev dc", 5 * 1024, NULL, MISC_TASK_PRIORITY, NULL);
    break;
  }
  case BLE_GAP_EVENT_ADV_COMPLETE: {
    ESP_LOGI("BLE", "Advertising complete. Disabling Bluetooth");
    stop();
    break;
  }
  }

  return 0;
}
