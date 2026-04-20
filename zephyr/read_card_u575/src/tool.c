#include "tool.h"

#include <zephyr/kernel.h>

void Sleep(unsigned int ms)
{
	k_sleep(K_MSEC(ms));
}

void PrintBuf(unsigned char *label, unsigned char *data, unsigned int length)
{
	if (label == NULL || data == NULL || length == 0U) {
		return;
	}

	printk("%s", label);
	for (unsigned int index = 0; index < length; ++index) {
		printk("%02X%s", data[index], (index + 1U < length) ? " " : "");
	}
	printk("\n");
}
