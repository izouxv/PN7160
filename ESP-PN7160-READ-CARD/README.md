| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C5 | ESP32-C6 | ESP32-C61 | ESP32-H2 | ESP32-H21 | ESP32-P4 | ESP32-S2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | --------- | -------- | --------- | -------- | -------- | -------- |

# PN7160 NFC Card Reader Example

## Overview

This example shows how to use the ESP-IDF I2C master driver to communicate with an NXP PN7160 NFC controller and read NFC card UID (ISO14443 Type A).

The example includes PN7160 initialization, RF discovery, card detection via GPIO interrupt, and UID reading.

## Hardware Required

- ESP32 series development board
- PN7160 NFC module
- NFC cards (e.g. MIFARE Classic)

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
````

Exit monitor: `Ctrl-]`

## Example Output

```text
I (276) main_task: Calling app_main()
I (276) main: PN7160 NFC Reader Card Detection Example Start
I (276) main: I2C bus initialized
I (276) main: pn7160 device added
I (726) main: pn7160 exited download mode
I (726) main: pn7160 reset completed
I (756) main: pn7160 core reset response: 40 00 01 00
I (776) main: pn7160 core reset notification: 
I (776) main: 60 00 09 02 01 20 04 04 61 12 50 11
I (806) main: pn7160 core init response: 
I (806) main: 40 01 1e 00 1a 7e 06 02 01 d0 02 ff ff 01 ff 00
I (806) main: 08 00 00 01 00 02 00 03 00 80 00 82 00 83 00 84
I (816) main: 00
I (826) main: pn7160 NCI proprietary activation response: 
I (826) main: 4f 02 05 00 00 02 83 a3
I (836) main: pn7160 RF discover map response: 41 00 01 00
I (856) main: pn7160 core set config response: 40 02 02 00
I (876) main: pn7160 core reset response: 40 00 01 00
I (886) main: pn7160 core reset notification: 
I (886) main: 60 00 09 02 00 20 04 04 61 12 50 11
I (916) main: pn7160 core init response: 
I (916) main: 40 01 1e 00 1a 7e 06 02 01 d0 02 ff ff 01 ff 00
I (916) main: 08 00 00 01 00 02 00 03 00 80 00 82 00 83 00 84
I (926) main: 00
I (936) main: pn7160 RF discover response: 41 03 01 00
I (936) main: pn7160 initialization completed
I (936) main: pn7160 task started
I (936) main_task: Returned from app_main()
I (3046) main: Card detected
I (3046) main: 61 05 15 01 01 02 00 ff 01 0a 04 00 04 78 ec 86
I (3046) main: a2 01 08 00 00 00 00 00
I (3046) main: Card number: 1 ,Card ID: 0x78EC86A2
I (3066) main: RF deactivate response: 41 06 01 00
I (3066) main: RF deactivate notification:
I (3066) main: 61 06 02 03 00
```

## Notes

* Only card UID is read
* I2C address: `0x28`
* Based on PN7160 NCI protocol