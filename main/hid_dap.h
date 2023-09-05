#pragma once

#include "host/ble_uuid.h"
#include "host/ble_gap.h"

#define LITTLE_ENDIAN_16BIT(value) (uint8_t)(value & 0x00FF), (uint8_t)((value & 0xFF00) >> 8)

#define SVC_UUID16_HID 0x1812
#define SVC_UUID16_BATTERY 0x180F
#define SVC_UUID16_DEVICE_INFO 0x180A

#define CHR_UUID16_HID_REPORT 0x2A4D
#define CHR_UUID16_HID_REPORT_MAP 0x2A4B
#define CHR_UUID16_HID_INFORMATION 0x2A4A
#define CHR_UUID16_HID_CONTROL_POINT 0x2A4C
#define CHR_UUID16_BATTERY_LEVEL 0x2A19
#define CHR_UUID16_DEVICE_INFO_PNP_ID 0x2A50

#define DSC_UUID16_REPORT_REFERENCE 0x2908

int hid_dap_init(void);

int hid_dap_handle_subscribe_event(struct ble_gap_event *event);