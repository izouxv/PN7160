| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C5 | ESP32-C6 | ESP32-C61 | ESP32-H2 | ESP32-H21 | ESP32-P4 | ESP32-S2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | --------- | -------- | --------- | -------- | -------- | -------- |

# PN7160 Firmware Download Example

## Overview

This example demonstrates how to update firmware of the **NXP PN7160 NFC controller** using the ESP-IDF I2C master driver.

The example covers:

- PN7160 download mode control (DWL / RST)
- Firmware version query
- Firmware download via I2C
- CRC16 verification and chunked transfer

Firmware data is provided as a binary array and downloaded to PN7160 at runtime.

## Hardware Required

- ESP32 series development board
- PN7160 NFC module

## Pin Assignment

### I2C

| Signal | GPIO |
| ------ | ---- |
| SDA    | 21   |
| SCL    | 22   |

### PN7160 Control Pins

| Signal | GPIO |
| ------ | ---- |
| RST    | 14   |
| INT    | 4    |
| DWL    | 13   |

## Build and Flash

```bash
idf.py -p PORT flash monitor
```

Exit monitor: `Ctrl-]`

## Example Output

```text
I main: I2C bus initialized
I main: pn7160 entered download mode
I main: Current FW Version: ROM: 0x01, MAJ: 0x02, MIN: 0x03
I main: Downloading FW version 02.03
I main: Download succeed
```

## Notes

- I2C address: `0x28`
- Firmware is transferred in chunks
- FORCE_DWL controls forced firmware update