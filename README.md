# bluedap
Wireless SWD debug probe firmware for ESP32 series.

## Features
- **Wireless flashing and debugging over Bluetooth Low Energy (BLE)**
    - Supports Arm Cortex-M microcontrollers with Serial Wire Debug (SWD) ports
    - Suitable for development of moving devices such as robots
- **Compatibility with existing software** such as [probe-rs](https://probe.rs/) and [OpenOCD](https://openocd.org/)
    - This is because bluedap implements CMSIS-DAP v1 (HID-based) protocol on BLE HID-over-GATT Profile (HOGP)

## Usage
1. Compile and flash bluedap firmware on an ESP32-series microcontroller (tested on ESP32C3), using [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/) environment.
2. Connect the ESP and target device 
    | ESP | Target | Remarks |
    | --- | ------ | ------- |
    | GPIO4 | SWCLK | |
    | GPIO5 | SWDIO | |
    | GPIO6 | RESET | (optional) |
3. Plug the ESP into USB port of a PC and open [serial console](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-reference/kconfig.html#config-esp-console-uart).
3. On a PC, pair a Bluetooth LE device named `bluedap CMSIS-DAP` or `bluedap`. **The required PIN code is displayed on the serial console.**
4. Now you can use your favorite CMSIS-DAP-compatible software! **Pairing using serial console is no longer needed for subsequent uses.**

## TODO
- [ ] Faster communication using LE 2M PHY
- [ ] JTAG support for non-Arm targets
- [ ] Virtual serial port

## Similar projects
- [yswallow/nRF52_BLE_DAP](https://github.com/yswallow/nRF52_BLE_DAP): Preceding BLE CMSIS-DAP probe using Nordic nRF chip.
- [windowsair/wireless-esp8266-dap](https://github.com/windowsair/wireless-esp8266-dap): ESP-based wireless CMSIS-DAP using Wi-Fi and USBIP.