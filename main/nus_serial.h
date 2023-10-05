#pragma once

#include "sdkconfig.h"
#ifdef CONFIG_ENABLE_SERIAL

#include "host/ble_gap.h"

#define SVC_UUID128_NUS 0x6E, 0x40, 0x00, 0x01, 0xB5, 0xA3, 0xF3, 0x93, 0xE0, 0xA9, 0xE5, 0x0E, 0x24, 0xDC, 0xCA, 0x9E 

#define CHR_UUID128_NUS_RX 0x6E, 0x40, 0x00, 0x02, 0xB5, 0xA3, 0xF3, 0x93, 0xE0, 0xA9, 0xE5, 0x0E, 0x24, 0xDC, 0xCA, 0x9E
#define CHR_UUID128_NUS_TX 0x6E, 0x40, 0x00, 0x03, 0xB5, 0xA3, 0xF3, 0x93, 0xE0, 0xA9, 0xE5, 0x0E, 0x24, 0xDC, 0xCA, 0x9E

int nus_serial_init(void);
int nus_serial_handle_subscribe_event(struct ble_gap_event *event);

#endif  // CONFIG_ENABLE_SERIAL