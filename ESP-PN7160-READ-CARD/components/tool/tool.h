#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <esp_err.h>
#include <esp_log.h>
#include <driver/i2c_master.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#define PN7160_I2C_ADDRESS 0x28
#define PN7160_RST_PIN 14
#define PN7160_INT_PIN 4
#define PN7160_DWL_PIN 13
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_SCL_IO 22
#define I2C_MASTER_SDA_IO 21
#define I2C_MASTER_FREQ_HZ 10000

extern SemaphoreHandle_t pn7160_semaphore;
extern i2c_master_bus_handle_t bus_handle;
extern i2c_master_dev_handle_t pn7160_handle;

#define PRINT_BUF(x, y, z) PrintBuf((unsigned char *)x, (unsigned char *)y, (unsigned int)z);

extern void Sleep(unsigned int ms);

extern void PrintBuf(unsigned char *x, unsigned char *y, unsigned int z);
