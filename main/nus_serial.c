#include "nus_serial.h"

#include "sdkconfig.h"
#ifdef CONFIG_ENABLE_SERIAL

#include "esp_log.h"
#include "driver/uart.h"
#include "freertos/task.h"
#include "host/ble_uuid.h"

static const char* TAG = "nus_serial";

static const uart_port_t UART_PORT = UART_NUM_1;
static const int UART_BUF_SIZE = 1024;

static TaskHandle_t serial_task_handle;

static uint16_t tx_handle;
static int tx_notify_enable = 0;
static uint16_t conn_handle;

static int on_rx_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);
static int on_tx_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);

static const struct ble_gatt_svc_def gatt_services[] = {
    // Nordic UART Service
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID128_DECLARE(SVC_UUID128_NUS),
        .characteristics = (struct ble_gatt_chr_def[]) {
            // RX (PC to device)
            {
                .uuid = BLE_UUID128_DECLARE(CHR_UUID128_NUS_RX),
                .access_cb = on_rx_access,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP
            },
            // TX (device to PC)
            {
                .uuid = BLE_UUID128_DECLARE(CHR_UUID128_NUS_TX),
                .flags = BLE_GATT_CHR_F_NOTIFY,
                .access_cb = on_tx_access,
                .val_handle = &tx_handle
                // Client Characteristic Configuration descriptor (CCCD) is not included because it is automatically added by stack.
            },
            // This indicates end of characteristic array
            {
                NULL
            }
        }
    },
    // This indicates end of service array
    {
        .type = 0
    }
};

static int on_rx_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    assert(ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR);   // Write-only characteristic

    int rc;

    static uint8_t buf[BLE_ATT_MTU_MAX];
    uint16_t copied_len;
    rc = ble_hs_mbuf_to_flat(ctxt->om, buf, sizeof(buf), &copied_len);
    assert(rc == 0);

    uart_write_bytes(UART_PORT, buf, copied_len);

    return 0;
}

static int on_tx_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    assert(ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR);   // Read-only characteristic
    return 0;
}

static void serial_task(void *pvParameters)
{
    uart_config_t config = {
        .baud_rate = CONFIG_SERIAL_INITIAL_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    ESP_ERROR_CHECK(uart_param_config(UART_PORT, &config));

    QueueHandle_t uart_queue;
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT, UART_BUF_SIZE, UART_BUF_SIZE, 10, &uart_queue, 0));

    static uint8_t buf[512];

    for (;;) {
        int read_len = uart_read_bytes(UART_PORT, buf, sizeof(buf), 1);
        if (read_len < 0) {
            ESP_LOGE(TAG, "UART read error");
            continue;
        }

        if (tx_notify_enable) {
            struct os_mbuf *om = ble_hs_mbuf_from_flat(buf, read_len);
            ble_gatts_notify_custom(conn_handle, tx_handle, om);
        }
    }
}

int nus_serial_init(void)
{
    int rc;

    rc = ble_gatts_count_cfg(gatt_services);
    if (rc != 0) {
        return rc;
    }

    rc = ble_gatts_add_svcs(gatt_services);
    if (rc != 0) {
        return rc;
    }

    // Serial communication is done in another FreeRTOS task
    xTaskCreate(serial_task, "serial", 4096, NULL, 2, &serial_task_handle);

    return 0;
}

int nus_serial_handle_subscribe_event(struct ble_gap_event *event)
{
    assert(event->type == BLE_GAP_EVENT_SUBSCRIBE);

    if (event->subscribe.attr_handle == tx_handle) {
        tx_notify_enable = event->subscribe.cur_notify;
        conn_handle = event->subscribe.conn_handle; // Remember connection handle for notification

        ESP_LOGI(TAG, "TX Characteristic notification %s", (event->subscribe.cur_notify) ? "enabled" : "disabled");
    }

    return 0;
}

#endif //   CONFIG_ENABLE_SERIAL