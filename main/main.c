// Roughly based on esp-idf bluehr example: https://github.com/espressif/esp-idf/tree/master/examples/bluetooth/nimble/blehr

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "DAP_config.h"
#include "DAP.h"
#include "hid_dap.h"

static const char* TAG = "main";

static const char* DEVICE_NAME_SHORT = "bluedap";
// To be recognized as a CMSIS-DAP, the Device Name (Product String in USB) must contain it
// Reference: https://arm-software.github.io/CMSIS_5/DAP/html/group__DAP__ConfigUSB__gr.html
static const char* DEVICE_NAME_COMPLETE = "bluedap CMSIS-DAP";

static uint8_t ble_addr_type;  // BLE address type
static uint16_t ble_conn_handle;    // BLE connection handle

void ble_store_config_init(void);   // It seems implemented in NimBLE but not declared in public headers...

int on_adv_event(struct ble_gap_event *event, void *arg);

void ble_advertise()
{
    int rc; // NimBLE return code

    // Set fields of advertisement
    struct ble_hs_adv_fields adv_fields;
    memset(&adv_fields, 0, sizeof(adv_fields));
    adv_fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;    // Always discoverable. Classic BT (BR/EDR) is not supported
    adv_fields.tx_pwr_lvl_is_present = true;    // Advertisement include TX power info
    adv_fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO; // TX power info is automatically filled by BLE stack
    adv_fields.name = (uint8_t*)DEVICE_NAME_SHORT;
    adv_fields.name_len = strlen(DEVICE_NAME_SHORT);
    adv_fields.name_is_complete = 0;    // This is a shortened name
    adv_fields.appearance = BLE_SVC_GAP_APPEARANCE_GEN_HID; // Generic HID appearance
    adv_fields.appearance_is_present = 1;   // Include appearance
    // Declare implemented services
    ble_uuid16_t service_uuids16[] = { BLE_UUID16_INIT(SVC_UUID16_HID), BLE_UUID16_INIT(SVC_UUID16_BATTERY), BLE_UUID16_INIT(SVC_UUID16_DEVICE_INFO) };
    adv_fields.uuids16 = service_uuids16;
    adv_fields.num_uuids16 = sizeof(service_uuids16) / sizeof(ble_uuid16_t);
    adv_fields.uuids16_is_complete = 1;

    rc = ble_gap_adv_set_fields(&adv_fields);
    assert(rc == 0);

    // Set BLE advertising parametes
    struct ble_gap_adv_params adv_params;
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;   // This device can be connected by any devices
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;   // This device is always discoverable

    rc = ble_gap_adv_start(ble_addr_type, NULL, BLE_HS_FOREVER, &adv_params, on_adv_event, NULL);
    assert(rc == 0);

    ESP_LOGI(TAG, "BLE advertising started");
}

// BLE advertisement event handler
int on_adv_event(struct ble_gap_event *event, void *arg)
{
    int rc;
    struct ble_gap_conn_desc conn_desc;

    switch (event->type) {
    case BLE_GAP_EVENT_ADV_COMPLETE:
        ESP_LOGI(TAG, "BLE advertisement completed (reason = 0x%04x)", event->adv_complete.reason);
        ble_advertise();
        break;
    
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            ESP_LOGI(TAG, "BLE connection established");
            ble_conn_handle = event->connect.conn_handle;
        } else {
            ESP_LOGI(TAG, "BLE connection failed (status = 0x%04x)", event->connect.status);
            ble_advertise();
        }
        break;
    
    case BLE_GAP_EVENT_REPEAT_PAIRING:
        // Replace bond when the device is already bonded to some device
        // Reference: esp-idf bleprph example https://github.com/espressif/esp-idf/blob/62ee4135e033cc85eb0d7572e5b5d147bcb4349e/examples/bluetooth/nimble/bleprph/main/main.c#L335-L349
        ESP_LOGI(TAG, "Replacing old bond with new bond");
        rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &conn_desc);
        assert(rc == 0);
        ble_store_util_delete_peer(&conn_desc.peer_id_addr);
        return BLE_GAP_REPEAT_PAIRING_RETRY;    // Old bond is removed and pairing process should be done again
    
    case BLE_GAP_EVENT_SUBSCRIBE:
        rc = hid_dap_handle_subscribe_event(event);
        assert(rc == 0);
        break;
    
    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "BLE disconnect (reason = 0x%04x)", event->disconnect.reason);
        ble_conn_handle = BLE_HS_CONN_HANDLE_NONE;
        ble_advertise();
        break;

    default:
        break;
    }

    return 0;
}

// Called when BLE Host and Controller become in sync (i.e. ready)
void on_ble_sync()
{
    int rc; // NimBLE return code
    
    // Get optimal address type given we don't use BLE privacy
    rc = ble_hs_id_infer_auto(0, &ble_addr_type);
    assert(rc == 0);

    // Get device address
    uint8_t addr[6];
    int is_nrpa;
    rc = ble_hs_id_copy_addr(ble_addr_type, addr, &is_nrpa);
    assert(rc == 0);

    ESP_LOGI(TAG, "BLE address: %02X:%02X:%02X:%02X:%02X:%02X %s",
        addr[5], addr[4], addr[3], addr[2], addr[1], addr[0],
        is_nrpa ? "(NRPA)" : ""
    );

    ble_advertise();
}

// Called when BLE stack is reset due to error
void on_ble_reset(int reason)
{
    ESP_LOGE(TAG, "BLE reset (reason = 0x%04x)", reason);
}

// A FreeRTOS task that runs BLE host
void ble_host_task(void *pvParameters)
{
    ESP_LOGI(TAG, "BLE host started");

    nimble_port_run();

    nimble_port_freertos_deinit();
}

void ble_store_config_init(void);

void app_main(void)
{
    esp_err_t ret;  // ESP-IDF return code
    int rc; // NimBLE return code

    ret = nvs_flash_init();
    ESP_ERROR_CHECK(ret);

    ret = nimble_port_init();
    ESP_ERROR_CHECK(ret);
    
    // Set callbacks for NimBLE host
    ble_hs_cfg.sync_cb = on_ble_sync;
    ble_hs_cfg.reset_cb = on_ble_reset;
    // Set security/bonding related settings
    // Reference: esp-idf bleprph example https://github.com/espressif/esp-idf/blob/62ee4135e033cc85eb0d7572e5b5d147bcb4349e/examples/bluetooth/nimble/bleprph/main/main.c#L517-L540
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;  // TODO:
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO; // This device has no IO (e.g. display, keyboard) for pairing
    ble_hs_cfg.sm_bonding = 1;  // Enable bonding
    ble_hs_cfg.sm_our_key_dist |= BLE_SM_PAIR_KEY_DIST_ENC; // ?
    ble_hs_cfg.sm_their_key_dist |= BLE_SM_PAIR_KEY_DIST_ENC;   // ?
    ble_hs_cfg.sm_mitm = 1; // Use man-in-the-middle protection
    ble_hs_cfg.sm_sc = 1;   // Use LE Secure Connections

    ble_svc_gap_init();
    ble_svc_gatt_init();

    rc = ble_svc_gap_device_name_set(DEVICE_NAME_COMPLETE);
    assert(rc == 0);

    ble_store_config_init();

    // Increase ATT_MTU as large as possible (because maximum length of HID report in notification is limited by ATT_MTU)
    rc = ble_att_set_preferred_mtu(BLE_ATT_MTU_MAX);
    assert(rc == 0);

    rc = hid_dap_init();
    assert(rc == 0);

    // Start BLE task
    nimble_port_freertos_init(ble_host_task);
}
