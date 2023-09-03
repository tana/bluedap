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
#include "DAP_config.h"
#include "DAP.h"

static const char* TAG = "main";

static const char* DEVICE_NAME = "bluedap";

uint8_t ble_addr_type;  // BLE address type

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
    adv_fields.name = (uint8_t*)DEVICE_NAME;
    adv_fields.name_len = strlen(DEVICE_NAME);
    adv_fields.name_is_complete = 1;    // Include device name

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
    switch (event->type) {
    case BLE_GAP_EVENT_ADV_COMPLETE:
        ESP_LOGI(TAG, "BLE advertisement completed (reason = %d)", event->adv_complete.reason);
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
        addr[0], addr[1], addr[2], addr[3], addr[4], addr[5],
        is_nrpa ? "(NRPA)" : ""
    );

    ble_advertise();
}

// Called when BLE stack is reset due to error
void on_ble_reset(int reason)
{
    ESP_LOGE(TAG, "BLE reset (reason = %d)", reason);
}

// A FreeRTOS task that runs BLE host
void ble_host_task(void *pvParameters)
{
    ESP_LOGI(TAG, "BLE host started");

    nimble_port_run();

    nimble_port_freertos_deinit();
}

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

    rc = ble_svc_gap_device_name_set(DEVICE_NAME);
    assert(rc == 0);

    // Start BLE task
    nimble_port_freertos_init(ble_host_task);

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
