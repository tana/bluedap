// Roughly based on esp-idf bluehr example: https://github.com/espressif/esp-idf/tree/master/examples/bluetooth/nimble/blehr

#include "hid_dap.h"

#include "esp_log.h"
#include "host/ble_hs.h"
#include "services/gatt/ble_svc_gatt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "DAP_config.h"

static const char* TAG = "hid_dap";

static const char REPORT_DESCRIPTOR[] = {
    0x06, 0x00, 0xff,              // USAGE_PAGE (Vendor Defined Page 1)
    0x09, 0x01,                    // USAGE (Vendor Usage 1)
    0xa1, 0x01,                    // COLLECTION (Application)
    0x09, 0x01,                    //   USAGE (Vendor Usage 1)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x26, 0xff, 0x00,              //   LOGICAL_MAXIMUM (255)
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x95, DAP_PACKET_SIZE,         //   REPORT_COUNT (DAP_PACKET_SIZE)
    0x81, 0x02,                    //   INPUT (Data,Var,Abs)
    0x09, 0x02,                    //   USAGE (Vendor Usage 2)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x26, 0xff, 0x00,              //   LOGICAL_MAXIMUM (255)
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x95, DAP_PACKET_SIZE,         //   REPORT_COUNT (DAP_PACKET_SIZE)
    0x91, 0x02,                    //   OUTPUT (Data,Var,Abs)
    0xc0                           // END_COLLECTION
};

static int on_hid_input_report_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);
static int on_hid_input_report_reference_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);
static int on_hid_output_report_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);
static int on_hid_output_report_reference_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);
static int on_hid_report_map_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);
static int on_hid_information_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);
static int on_hid_control_point_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);
static int on_battery_level_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);
static int on_device_info_pnp_id_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);

static void on_output_report_written(void);

static void on_timer(TimerHandle_t xTimer);

static uint16_t input_report_handle;
static int input_report_notify_enable = 0;
static uint16_t conn_handle;

// Input Report data to be sent to PC
uint8_t input_report_data[DAP_PACKET_SIZE];
// Output Report data received from PC
uint8_t output_report_data[DAP_PACKET_SIZE];

static const struct ble_gatt_svc_def gatt_services[] = {
    // HID service
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(SVC_UUID16_HID),
        .characteristics = (struct ble_gatt_chr_def[]) {
            // Input Report (device to PC)
            {
                .uuid = BLE_UUID16_DECLARE(CHR_UUID16_HID_REPORT),
                .val_handle = &input_report_handle,
                .access_cb = on_hid_input_report_access,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                .descriptors = (struct ble_gatt_dsc_def[]) {
                    // Client Characteristic Configuration descriptor (CCCD) is not included because it is automatically added by stack.
                    // Report Reference descriptor
                    {
                        .uuid = BLE_UUID16_DECLARE(DSC_UUID16_REPORT_REFERENCE),
                        .access_cb = on_hid_input_report_reference_access,
                        .att_flags = BLE_ATT_F_READ
                    },
                    // This indicates end of descriptor array
                    {
                        NULL
                    }
                }
            },
            // Output Report (PC to device)
            {
                .uuid = BLE_UUID16_DECLARE(CHR_UUID16_HID_REPORT),
                .access_cb = on_hid_output_report_access,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
                .descriptors = (struct ble_gatt_dsc_def[]) {
                    // Report Reference descriptor
                    {
                        .uuid = BLE_UUID16_DECLARE(DSC_UUID16_REPORT_REFERENCE),
                        .access_cb = on_hid_output_report_reference_access,
                        .att_flags = BLE_ATT_F_READ
                    },
                    // This indicates end of descriptor array
                    {
                        NULL
                    }
                }
            },
            // Report Map characteristic
            {
                .uuid = BLE_UUID16_DECLARE(CHR_UUID16_HID_REPORT_MAP),
                .access_cb = on_hid_report_map_access,
                .flags = BLE_GATT_CHR_F_READ
            },
            // HID Information characteristic
            {
                .uuid = BLE_UUID16_DECLARE(CHR_UUID16_HID_INFORMATION),
                .access_cb = on_hid_information_access,
                .flags = BLE_GATT_CHR_F_READ
            },
            // HID Control Point characteristic
            {
                .uuid = BLE_UUID16_DECLARE(CHR_UUID16_HID_CONTROL_POINT),
                .access_cb = on_hid_control_point_access,
                .flags = BLE_GATT_CHR_F_WRITE_NO_RSP
            },
            // This indicates end of characteristic array
            {
                NULL
            }
        },
    },
    // Battery serice
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(SVC_UUID16_BATTERY),
        .characteristics = (struct ble_gatt_chr_def[]) {
            // Battery Level characteristice
            {
                .uuid = BLE_UUID16_DECLARE(CHR_UUID16_BATTERY_LEVEL),
                .access_cb = on_battery_level_access,
                .flags = BLE_GATT_CHR_F_READ
            },
            // This indicates end of characteristic array
            {
                NULL
            }
        }
    },
    // Device Information service
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(SVC_UUID16_DEVICE_INFO),
        .characteristics = (struct ble_gatt_chr_def[]) {
            // PnP ID characteristic
            {
                .uuid = BLE_UUID16_DECLARE(CHR_UUID16_DEVICE_INFO_PNP_ID),
                .access_cb = on_device_info_pnp_id_access,
                .flags = BLE_GATT_CHR_F_READ
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

static int on_hid_input_report_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    assert(ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR);    // Read-only characteristic

    int rc = os_mbuf_append(ctxt->om, input_report_data, sizeof(input_report_data));
    return (rc == 0) ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static int on_hid_input_report_reference_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    assert(ctxt->op == BLE_GATT_ACCESS_OP_READ_DSC);    // Read-only descriptor

    uint8_t data[] = { 
        0,  // Report ID = 0
        0x01    // Input Report
    };

    int rc = os_mbuf_append(ctxt->om, data, sizeof(data));
    return (rc == 0) ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static int on_hid_output_report_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    int rc;

    switch (ctxt->op) {
    case BLE_GATT_ACCESS_OP_WRITE_CHR:
        // See Apache Mynewt tutorial https://mynewt.apache.org/latest/tutorials/ble/bleprph/bleprph-sections/bleprph-chr-access.html#write-access
        rc = ble_hs_mbuf_to_flat(ctxt->om, output_report_data, sizeof(output_report_data), NULL);
        on_output_report_written();
        return (rc == 0) ? 0 : BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    
    case BLE_GATT_ACCESS_OP_READ_CHR:
        rc = os_mbuf_append(ctxt->om, output_report_data, sizeof(output_report_data));
        return (rc == 0) ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    
    default:
        assert(0);  // Should not happen
        return BLE_ATT_ERR_UNLIKELY;
    }
}

static int on_hid_output_report_reference_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    assert(ctxt->op == BLE_GATT_ACCESS_OP_READ_DSC);    // Read-only descriptor

    uint8_t data[] = { 
        0,  // Report ID = 0
        0x02    // Output Report
    };

    int rc = os_mbuf_append(ctxt->om, data, sizeof(data));
    return (rc == 0) ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static int on_hid_report_map_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    assert(ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR);    // Read-only characteristic

    int rc = os_mbuf_append(ctxt->om, REPORT_DESCRIPTOR, sizeof(REPORT_DESCRIPTOR));
    return (rc == 0) ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static int on_hid_information_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    assert(ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR);    // Read-only characteristic

    uint8_t data[] = { 
        LITTLE_ENDIAN_16BIT(0x111), // bcdHID = 0x111 (USB HID version 1.11)
        0x00,   // bCountryCode = 0x00 (not localized)
        0x02    // RemoteWake = FALSE, NormallyConnectable = TRUE (advertise when bonded but not connected)
    };

    int rc = os_mbuf_append(ctxt->om, data, sizeof(data));
    return (rc == 0) ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static int on_hid_control_point_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    assert(ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR);   // This characteristic only supoorts Write Without Response

    return 0;
}

static int on_battery_level_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    assert(ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR);    // Read-only characteristic

    uint8_t data[] = { 
        100 // TODO:
    };

    int rc = os_mbuf_append(ctxt->om, data, sizeof(data));
    return (rc == 0) ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static int on_device_info_pnp_id_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    assert(ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR);    // Read-only characteristic

    uint8_t data[] = { 
        0x02,   // VID assigned by USB Implementer's Forum
        LITTLE_ENDIAN_16BIT(0xDEAD),    // VID
        LITTLE_ENDIAN_16BIT(0xBEEF),    // PID
        LITTLE_ENDIAN_16BIT(0x0001),    // Product Version = 0.0.1
    };

    int rc = os_mbuf_append(ctxt->om, data, sizeof(data));
    return (rc == 0) ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static void on_output_report_written(void)
{
    char data_str[3 * sizeof(output_report_data)];
    for (int i = 0; i < sizeof(output_report_data); i++) {
        snprintf(&data_str[3 * i], 3, "%02x", output_report_data[i]);
        data_str[3 * i + 2] = ' ';  // Replace null character with whitespace
    }
    data_str[3 * sizeof(output_report_data)] = '\0';
    ESP_LOGI(TAG, "Output report written: %s", data_str);
}

static void on_timer(TimerHandle_t xTimer)
{
    input_report_data[0] = (input_report_data[0] + 1) % 256;

    if (input_report_notify_enable) {
        // Notify new input report data to the subscribed peer
        struct os_mbuf *om = ble_hs_mbuf_from_flat(input_report_data, sizeof(input_report_data));
        ble_gatts_notify_custom(conn_handle, input_report_handle, om);
    }
}

int hid_dap_init(void)
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

    TimerHandle_t timer = xTimerCreate("timer", pdMS_TO_TICKS(1000), pdTRUE, NULL, on_timer);
    xTimerStart(timer, 0);

    return 0;
}

int hid_dap_handle_subscribe_event(struct ble_gap_event *event)
{
    assert(event->type == BLE_GAP_EVENT_SUBSCRIBE);

    if (event->subscribe.attr_handle == input_report_handle) {
        input_report_notify_enable = event->subscribe.cur_notify;
        conn_handle = event->subscribe.conn_handle; // Remember connection handle for notification

        ESP_LOGI(TAG, "Input report notification %s", (event->subscribe.cur_notify) ? "enabled" : "disabled");
    }

    return 0;
}