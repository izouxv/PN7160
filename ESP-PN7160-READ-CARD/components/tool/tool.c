#include "tool.h"

static const char *TAG = "tool";

void Sleep(unsigned int ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

void PrintBuf(unsigned char *x, unsigned char *y, unsigned int z)
{
    if (x == NULL || y == NULL || z == 0)
    {
        ESP_LOGE(TAG, "PrintBuf: Invalid parameters");
    }

    const char *TAG = (const char *)x;

    ESP_LOG_BUFFER_HEX(TAG, y, z);
}
