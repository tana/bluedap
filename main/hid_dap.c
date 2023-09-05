// Roughly based on esp-idf bluehr example: https://github.com/espressif/esp-idf/tree/master/examples/bluetooth/nimble/blehr

#include "hid_dap.h"
#include "host/ble_hs.h"
#include "services/gatt/ble_svc_gatt.h"

const ble_uuid16_t SVC_UUID_HID = BLE_UUID16_INIT(0x1812);
const ble_uuid16_t SVC_UUID_BATTERY = BLE_UUID16_INIT(0x180F);
const ble_uuid16_t SVC_UUID_DEVICE_INFO = BLE_UUID16_INIT(0x180A);

const ble_uuid16_t CHR_UUID_HID_REPORT = BLE_UUID16_INIT(0x2A4D);
const ble_uuid16_t CHR_UUID_HID_REPORT_MAP = BLE_UUID16_INIT(0x2A4B);
const ble_uuid16_t CHR_UUID_HID_INFORMATION = BLE_UUID16_INIT(0x2A4A);
const ble_uuid16_t CHR_UUID_HID_CONTROL_POINT = BLE_UUID16_INIT(0x2A4C);
const ble_uuid16_t CHR_UUID_BATTERY_LEVEL = BLE_UUID16_INIT(0x2A19);
const ble_uuid16_t CHR_UUID_DEVICE_INFO_PNP_ID = BLE_UUID16_INIT(0x2A50);

const ble_uuid16_t DSC_UUID_REPORT_REFERENCE = BLE_UUID16_INIT(0x2908);

static const char REPORT_DESCRIPTOR[] = {
    0x06, 0x00, 0xff,              // USAGE_PAGE (Vendor Defined Page 1)
    0x09, 0x01,                    // USAGE (Vendor Usage 1)
    0xa1, 0x01,                    // COLLECTION (Application)
    0x09, 0x01,                    //   USAGE (Vendor Usage 1)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x26, 0xff, 0x00,              //   LOGICAL_MAXIMUM (255)
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x95, 0x40,                    //   REPORT_COUNT (64)
    0x81, 0x02,                    //   INPUT (Data,Var,Abs)
    0x09, 0x02,                    //   USAGE (Vendor Usage 2)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x26, 0xff, 0x00,              //   LOGICAL_MAXIMUM (255)
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x95, 0x40,                    //   REPORT_COUNT (64)
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

static const struct ble_gatt_svc_def gatt_services[] = {
    // HID service
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &SVC_UUID_HID.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            // Input Report (device to PC)
            {
                .uuid = &CHR_UUID_HID_REPORT.u,
                .access_cb = on_hid_input_report_access,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                .descriptors = (struct ble_gatt_dsc_def[]) {
                    // Client Characteristic Configuration descriptor (CCCD) is not included because it is automatically added by stack.
                    // Report Reference descriptor
                    {
                        .uuid = &DSC_UUID_REPORT_REFERENCE.u,
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
                .uuid = &CHR_UUID_HID_REPORT.u,
                .access_cb = on_hid_output_report_access,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
                .descriptors = (struct ble_gatt_dsc_def[]) {
                    // Report Reference descriptor
                    {
                        .uuid = &DSC_UUID_REPORT_REFERENCE.u,
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
                .uuid = &CHR_UUID_HID_REPORT_MAP.u,
                .access_cb = on_hid_report_map_access,
                .flags = BLE_GATT_CHR_F_READ
            },
            // HID Information characteristic
            {
                .uuid = &CHR_UUID_HID_INFORMATION.u,
                .access_cb = on_hid_information_access,
                .flags = BLE_GATT_CHR_F_READ
            },
            // HID Control Point characteristic
            {
                .uuid = &CHR_UUID_HID_CONTROL_POINT.u,
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
        .uuid = &SVC_UUID_BATTERY.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            // Battery Level characteristice
            {
                .uuid = &CHR_UUID_BATTERY_LEVEL.u,
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
        .uuid = &SVC_UUID_DEVICE_INFO.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            // PnP ID characteristic
            {
                .uuid = &CHR_UUID_DEVICE_INFO_PNP_ID.u,
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
        NULL
    }
};

static int on_hid_input_report_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    return 0;
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
    return 0;
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

    return 0;
}