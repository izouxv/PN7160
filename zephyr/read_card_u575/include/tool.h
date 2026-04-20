#ifndef PN7160_ZEPHYR_TOOL_H_
#define PN7160_ZEPHYR_TOOL_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <zephyr/sys/printk.h>

#define PN7160_I2C_ADDRESS 0x28
#define PRINT_BUF(x, y, z) PrintBuf((unsigned char *)(x), (unsigned char *)(y), (unsigned int)(z))
#define PRINTF(...) printk(__VA_ARGS__)

void Sleep(unsigned int ms);
void PrintBuf(unsigned char *label, unsigned char *data, unsigned int length);

#endif
