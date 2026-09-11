#include "bluetooth.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatt_common_api.h"
#include "esp_gatts_api.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "nvs_flash.h"

namespace bluetooth {
namespace {
enum Attribute { Service, TxDeclaration, TxValue, TxCccd,
                 RxDeclaration, RxValue, AttributeCount };
constexpr char TAG[] = "BLE";
// BLE transmits 128-bit UUIDs least-significant byte first.
uint8_t serviceUuid[] = {0x11,0x02,0xf1,0xc9,0xdb,0x2a,0xd9,0xa8,
                        0x3d,0x4f,0x3b,0x6c,0x01,0x00,0x7a,0x8c};
uint8_t txUuid[] = {0x11,0x02,0xf1,0xc9,0xdb,0x2a,0xd9,0xa8,
                   0x3d,0x4f,0x3b,0x6c,0x02,0x00,0x7a,0x8c};
uint8_t rxUuid[] = {0x11,0x02,0xf1,0xc9,0xdb,0x2a,0xd9,0xa8,
                   0x3d,0x4f,0x3b,0x6c,0x03,0x00,0x7a,0x8c};
uint16_t primaryUuid = ESP_GATT_UUID_PRI_SERVICE;
uint16_t declarationUuid = ESP_GATT_UUID_CHAR_DECLARE;
uint16_t cccdUuid = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
uint8_t txProperties = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY;
uint8_t rxProperties = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE |
                       ESP_GATT_CHAR_PROP_BIT_WRITE_NR;
uint8_t initialTx[] = "READY";
uint8_t cccd[2] = {};
uint8_t rx[512] = "WRITE PING";
uint16_t rxLength = 10;
uint8_t prepared[sizeof(rx)] = {};
uint16_t preparedLength = 0;
uint16_t handles[AttributeCount] = {};
QueueHandle_t events = nullptr;
std::atomic<bool> connected{false};
std::atomic<bool> subscribed{false};
std::atomic<bool> advertising{false};
std::atomic<bool> starting{false};
std::atomic<bool> ready{false};
std::atomic<uint16_t> connectionId{0};
std::atomic<esp_gatt_if_t> serverInterface{ESP_GATT_IF_NONE};
// Configuration flags are owned by the Bluetooth callback task.
bool serviceReady = false;
bool advReady = false;
bool scanReady = false;
esp_ble_adv_params_t advertisingParameters = {};

esp_gatts_attr_db_t attributes[AttributeCount] = {
  {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, reinterpret_cast<uint8_t *>(&primaryUuid),
    ESP_GATT_PERM_READ, 16, 16, serviceUuid}},
  {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, reinterpret_cast<uint8_t *>(&declarationUuid),
    ESP_GATT_PERM_READ, 1, 1, &txProperties}},
  {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_128, txUuid, ESP_GATT_PERM_READ,
    512, sizeof(initialTx) - 1, initialTx}},
  {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, reinterpret_cast<uint8_t *>(&cccdUuid),
    ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, 2, 2, cccd}},
  {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, reinterpret_cast<uint8_t *>(&declarationUuid),
    ESP_GATT_PERM_READ, 1, 1, &rxProperties}},
  {{ESP_GATT_RSP_BY_APP}, {ESP_UUID_LEN_128, rxUuid,
    ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, sizeof(rx), 10, rx}},
};

void publish(EventType type, const uint8_t *data = nullptr, size_t length = 0) {
  Event event = {};
  event.type = type;
  if (data != nullptr) {
    memcpy(event.text, data, std::min(length, sizeof(event.text) - 1));
  }
  if (xQueueSend(events, &event, 0) != pdTRUE) {
    ESP_LOGE(TAG, "UI event queue full");
  }
}

void beginAdvertising() {
  if (!ready || connected || advertising || starting.exchange(true)) return;
  const esp_err_t result = esp_ble_gap_start_advertising(&advertisingParameters);
  if (result != ESP_OK) {
    starting = false;
    ESP_LOGE(TAG, "Cannot start advertising: %s", esp_err_to_name(result));
  }
}

void startWhenReady() {
  if (serviceReady && advReady && scanReady) {
    ready = true;
    beginAdvertising();
  }
}

void gapCallback(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  switch (event) {
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
      advReady = param->adv_data_raw_cmpl.status == ESP_BT_STATUS_SUCCESS;
      startWhenReady();
      break;
    case ESP_GAP_BLE_SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT:
      scanReady = param->scan_rsp_data_raw_cmpl.status == ESP_BT_STATUS_SUCCESS;
      startWhenReady();
      break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
      starting = false;
      advertising = param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS;
      if (advertising) {
        printf("[BLE] Advertising as %s\n", DEVICE_NAME);
        publish(EventType::Advertising);
      } else {
        ESP_LOGE(TAG, "Advertising failed: %d", param->adv_start_cmpl.status);
      }
      break;
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
      advertising = false;
      beginAdvertising();
      break;
    default:
      break;
  }
}

void receive(const uint8_t *data, uint16_t length) {
  memcpy(rx, data, length);
  rxLength = length;
  publish(EventType::Received, rx, rxLength);
  printf("[BLE RX] %.*s\n", static_cast<int>(std::min<uint16_t>(length, 24)), rx);
}

void gattsCallback(esp_gatts_cb_event_t event, esp_gatt_if_t interface,
                   esp_ble_gatts_cb_param_t *param) {
  switch (event) {
    case ESP_GATTS_REG_EVT: {
      ESP_ERROR_CHECK(param->reg.status == ESP_GATT_OK ? ESP_OK : ESP_FAIL);
      serverInterface = interface;
      ESP_ERROR_CHECK(esp_ble_gap_set_device_name(DEVICE_NAME));
      // Flags + complete service UUID fit in the 31-byte advertising packet.
      uint8_t advertisement[21] = {2, ESP_BLE_AD_TYPE_FLAG, 6,
                                  17, ESP_BLE_AD_TYPE_128SRV_CMPL};
      memcpy(advertisement + 5, serviceUuid, sizeof(serviceUuid));
      uint8_t scanResponse[sizeof(DEVICE_NAME) + 1] = {};
      scanResponse[0] = sizeof(DEVICE_NAME);
      scanResponse[1] = ESP_BLE_AD_TYPE_NAME_CMPL;
      memcpy(scanResponse + 2, DEVICE_NAME, sizeof(DEVICE_NAME) - 1);
      ESP_ERROR_CHECK(esp_ble_gap_config_adv_data_raw(advertisement, sizeof(advertisement)));
      ESP_ERROR_CHECK(esp_ble_gap_config_scan_rsp_data_raw(scanResponse, sizeof(scanResponse)));
      ESP_ERROR_CHECK(esp_ble_gatts_create_attr_tab(attributes, interface, AttributeCount, 0));
      break;
    }
    case ESP_GATTS_CREAT_ATTR_TAB_EVT:
      ESP_ERROR_CHECK(param->add_attr_tab.status == ESP_GATT_OK &&
                      param->add_attr_tab.num_handle == AttributeCount ? ESP_OK : ESP_FAIL);
      memcpy(handles, param->add_attr_tab.handles, sizeof(handles));
      ESP_ERROR_CHECK(esp_ble_gatts_start_service(handles[Service]));
      break;
    case ESP_GATTS_START_EVT:
      ESP_ERROR_CHECK(param->start.status == ESP_GATT_OK ? ESP_OK : ESP_FAIL);
      serviceReady = true;
      startWhenReady();
      break;
    case ESP_GATTS_CONNECT_EVT:
      connectionId = param->connect.conn_id;
      subscribed = false;
      advertising = false;
      connected = true;
      preparedLength = 0;
      publish(EventType::Connected);
      printf("[BLE] Phone connected\n");
      break;
    case ESP_GATTS_DISCONNECT_EVT:
      connected = false;
      subscribed = false;
      preparedLength = 0;
      cccd[0] = cccd[1] = 0;
      ESP_ERROR_CHECK(esp_ble_gatts_set_attr_value(handles[TxCccd], 2, cccd));
      publish(EventType::Disconnected);
      printf("[BLE] Phone disconnected; advertising will restart\n");
      beginAdvertising();
      break;
    case ESP_GATTS_READ_EVT:
      if (param->read.handle == handles[RxValue]) {
        esp_gatt_rsp_t response = {};
        response.attr_value.handle = param->read.handle;
        response.attr_value.offset = param->read.offset;
        const bool valid = param->read.offset <= rxLength;
        if (valid) {
          response.attr_value.len = rxLength - param->read.offset;
          memcpy(response.attr_value.value, rx + param->read.offset, response.attr_value.len);
        }
        ESP_ERROR_CHECK(esp_ble_gatts_send_response(interface, param->read.conn_id,
            param->read.trans_id, valid ? ESP_GATT_OK : ESP_GATT_INVALID_OFFSET, &response));
      }
      break;
    case ESP_GATTS_WRITE_EVT:
      if (param->write.handle == handles[TxCccd] && !param->write.is_prep &&
          param->write.len == 2) {
        subscribed = param->write.value[0] == 1 && param->write.value[1] == 0;
        printf("[BLE] Notifications %s\n", subscribed ? "enabled" : "disabled");
      } else if (param->write.handle == handles[RxValue]) {
        const auto &write = param->write;
        esp_gatt_status_t status = ESP_GATT_OK;
        if (write.offset > sizeof(rx) || (write.is_prep && write.offset != preparedLength)) {
          status = ESP_GATT_INVALID_OFFSET;
        } else if (write.len > sizeof(rx) - write.offset) {
          status = ESP_GATT_INVALID_ATTR_LEN;
        }
        esp_gatt_rsp_t response = {};
        if (status == ESP_GATT_OK) {
          if (write.is_prep) {
            memcpy(prepared + write.offset, write.value, write.len);
            preparedLength = write.offset + write.len;
            response.attr_value.handle = write.handle;
            response.attr_value.offset = write.offset;
            response.attr_value.len = write.len;
            memcpy(response.attr_value.value, write.value, write.len);
          } else {
            receive(write.value, write.len);
          }
        }
        if (write.need_rsp) {
          ESP_ERROR_CHECK(esp_ble_gatts_send_response(interface, write.conn_id,
              write.trans_id, status, write.is_prep ? &response : nullptr));
        }
      }
      break;
    case ESP_GATTS_EXEC_WRITE_EVT:
      if (param->exec_write.exec_write_flag == ESP_GATT_PREP_WRITE_EXEC) {
        receive(prepared, preparedLength);
      }
      preparedLength = 0;
      ESP_ERROR_CHECK(esp_ble_gatts_send_response(interface, param->exec_write.conn_id,
          param->exec_write.trans_id, ESP_GATT_OK, nullptr));
      break;
    default:
      break;
  }
}
}

void initialize() {
  events = xQueueCreate(32, sizeof(Event));
  ESP_ERROR_CHECK(events != nullptr ? ESP_OK : ESP_ERR_NO_MEM);
  // NVS is required by the SDK's radio initialization. Never erase it silently.
  ESP_ERROR_CHECK(nvs_flash_init());
  esp_bt_controller_config_t config = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_bt_controller_init(&config));
  ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));
  ESP_ERROR_CHECK(esp_bluedroid_init());
  ESP_ERROR_CHECK(esp_bluedroid_enable());
  advertisingParameters.adv_int_min = 0x20;
  advertisingParameters.adv_int_max = 0x40;
  advertisingParameters.adv_type = ADV_TYPE_IND;
  advertisingParameters.own_addr_type = BLE_ADDR_TYPE_PUBLIC;
  advertisingParameters.channel_map = ADV_CHNL_ALL;
  advertisingParameters.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;
  ESP_ERROR_CHECK(esp_ble_gap_register_callback(gapCallback));
  ESP_ERROR_CHECK(esp_ble_gatts_register_callback(gattsCallback));
  ESP_ERROR_CHECK(esp_ble_gatt_set_local_mtu(185));
  ESP_ERROR_CHECK(esp_ble_gatts_app_register(0));
  printf("[BLE] Service: %s\n[BLE] TX notify: %s\n[BLE] RX write: %s\n",
         SERVICE_UUID, TX_UUID, RX_UUID);
}

bool poll(Event &event) {
  return xQueueReceive(events, &event, 0) == pdTRUE;
}

void restartAdvertising() {
  if (connected || serverInterface == ESP_GATT_IF_NONE) return;
  if (advertising) {
    ESP_ERROR_CHECK(esp_ble_gap_stop_advertising());
  } else if (ready) {
    beginAdvertising();
  }
}

bool notify(const char *text) {
  if (!connected) return false;
  const size_t length = strlen(text);
  if (length > 20) return false;
  auto *bytes = reinterpret_cast<uint8_t *>(const_cast<char *>(text));
  ESP_ERROR_CHECK(esp_ble_gatts_set_attr_value(handles[TxValue], length, bytes));
  if (!subscribed) return false;
  const esp_err_t result = esp_ble_gatts_send_indicate(serverInterface,
      connectionId, handles[TxValue], length, bytes, false);
  if (result != ESP_OK) ESP_LOGW(TAG, "Notification failed: %s", esp_err_to_name(result));
  return result == ESP_OK;
}

bool notificationsEnabled() {
  return connected && subscribed;
}
}
