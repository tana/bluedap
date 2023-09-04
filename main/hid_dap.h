#pragma once

#include "host/ble_uuid.h"

#define LITTLE_ENDIAN_16BIT(value) (uint8_t)(value & 0x00FF), (uint8_t)((value & 0xFF00) >> 8)

extern const ble_uuid16_t SVC_UUID_HID;
extern const ble_uuid16_t SVC_UUID_BATTERY;
extern const ble_uuid16_t SVC_UUID_DEVICE_INFO;

extern const ble_uuid16_t CHR_UUID_HID_REPORT;
extern const ble_uuid16_t CHR_UUID_HID_REPORT_MAP;
extern const ble_uuid16_t CHR_UUID_HID_INFORMATION;
extern const ble_uuid16_t CHR_UUID_HID_CONTROL_POINT;
extern const ble_uuid16_t CHR_UUID_BATTERY_LEVEL;
extern const ble_uuid16_t CHR_UUID_DEVICE_INFO_PNP_ID;

extern const ble_uuid16_t DSC_UUID_REPORT_REFERENCE;

extern const uint8_t *REPORT_DESCRIPTOR;
extern const size_t REPORT_DESCRIPTOR_LEN;

int hid_dap_init(void);