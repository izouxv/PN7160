#include "tool.h"

static const char *TAG = "tool";

void Sleep(unsigned int ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

void PrintBuf(unsigned char *x, unsigned char *y, unsigned int z)
{
    size_t print_len;

    if (x != NULL)
    {
        ESP_LOGI(TAG, "%s", (char *)x);
    }

    if (y == NULL || z == 0)
    {
        return;
    }

    print_len = (z > 30) ? 30 : z;

    ESP_LOG_BUFFER_HEX(TAG, y, print_len);

    if (z > 30)
    {
        ESP_LOGI(TAG, "...");
    }
}
