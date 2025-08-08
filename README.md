# MIDI over Bluetooth LE to DIN/TRS MIDI converter with ESP32

This is source code to make a device for connecting BLE MIDI devices to standard (wired) MIDI devices. This code is for ESP32 microcontroller, which has Bluetooth LE communication functionality.

## Target device

Currently this code only supports ESP32-S3. I tested this code on Seed studio's [XIAO ESP32-S3](https://www.seeedstudio.com/XIAO-ESP32S3-p-5627.html).

## Dependencies

This code uses [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/index.html) as development environment. And uses `esp-nimble-cpp` ([https://github.com/h2zero/esp-nimble-cpp]) Bluetooth LE library.

## How to build and flash

```
> idf.py set-target esp32s3
> idf.py build
> idf.py flash monitor
```

## Hardware

This software uses ESP32-S3's built-in UART (UART0)  to send/recieve MIDI message. UART0 uses D6/GPIO43 port as TX (MIDI output) and D7/GPIO44 port as RX (MIDI input). Therefore, you need to connect MIDI interface circuit to these port. For more information about this circuit, please check specification document: [https://midi.org/5-pin-din-electrical-specs].

To build this MIDI interface circuit on breadboard, please check the schematic image and layout image in `schematic` directory.

## License

GPLv3 - [https://www.gnu.org/licenses/gpl-3.0.en.html].

## Author

@mesotokyo ([https://b.meso.tokyo/])
