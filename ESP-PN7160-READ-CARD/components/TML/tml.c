#include "tml.h"

static bool isDwlMode = false;

#define HEADER_SZ (isDwlMode == true ? 1 : 2)
#define FOOTER_SZ (isDwlMode == true ? 2 : 0)

static const char *TAG = "tml";

/**
 * @brief GPIO interrupt service routine for pn7160 INT pin
 *
 * Triggered when pn7160 detects a card.
 */
static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t)arg;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (gpio_num == PN7160_INT_PIN)
    {
        xSemaphoreGiveFromISR(pn7160_semaphore, &xHigherPriorityTaskWoken);
    }
    if (xHigherPriorityTaskWoken)
    {
        portYIELD_FROM_ISR();
    }
}

void INTF_INIT(void)
{
    /* Create semaphore if not exists */
    if (pn7160_semaphore == NULL)
    {
        pn7160_semaphore = xSemaphoreCreateBinary();
        if (pn7160_semaphore == NULL)
        {
            ESP_LOGE(TAG, "Semaphore creation failed");
            return;
        }
    }

    /* I2C bus configuration */
    i2c_master_bus_config_t i2c_cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_MASTER_NUM,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_cfg, &bus_handle));

    /* PN7160 device config */
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = PN7160_I2C_ADDRESS,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };

    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_cfg, &pn7160_handle));

    ESP_LOGI(TAG, "I2C interface initialized");
}

int32_t INTF_WRITE(uint8_t *pBuff, uint16_t buffLen)
{
    if (pn7160_handle == NULL)
    {
        return ESP_FAIL;
    }

    esp_err_t ret = i2c_master_transmit(pn7160_handle, pBuff, buffLen, portMAX_DELAY);
    return ret;
}

int32_t INTF_READ(uint8_t *pBuff, uint16_t buffLen)
{
    if (pn7160_handle == NULL)
    {
        return ESP_FAIL;
    }

    esp_err_t ret = i2c_master_receive(pn7160_handle, pBuff, buffLen, portMAX_DELAY);
    return ret;
}

Status tml_Init(void)
{
    /* Reset pin */
    gpio_config_t rst_cfg = {
        .pin_bit_mask = 1ULL << PN7160_RST_PIN,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&rst_cfg);

    /* Download pin */
    gpio_config_t dwl_cfg = {
        .pin_bit_mask = 1ULL << PN7160_DWL_PIN,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&dwl_cfg);

    /* Interrupt pin */
    gpio_config_t irq_cfg = {
        .pin_bit_mask = 1ULL << PN7160_INT_PIN,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_POSEDGE,
    };
    gpio_config(&irq_cfg);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(PN7160_INT_PIN, gpio_isr_handler, (void *)PN7160_INT_PIN);

    ESP_LOGI(TAG, "GPIO interface initialized");

    /* Initialize I2C */
    INTF_INIT();

    return SUCCESS;
}

Status tml_DeInit(void)
{
    /* Set VEN (reset) low */
    gpio_set_level(PN7160_RST_PIN, 0);
    return SUCCESS;
}

Status tml_Reset(void)
{
    gpio_set_level(PN7160_RST_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(PN7160_RST_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    return SUCCESS;
}

Status tml_Tx(uint8_t *pBuff, uint16_t buffLen)
{
    if (INTF_WRITE(pBuff, buffLen) != ESP_OK)
    {
        vTaskDelay(pdMS_TO_TICKS(10));
        if (INTF_WRITE(pBuff, buffLen) != ESP_OK)
        {
            return ERROR;
        }
    }
    return SUCCESS;
}

Status tml_Rx(uint8_t *pBuff, uint16_t buffLen, uint16_t *pBytesRead)
{
    if (INTF_READ(pBuff, HEADER_SZ + 1) == ESP_OK)
    {
        if ((pBuff[HEADER_SZ] + HEADER_SZ + 1) <= buffLen)
        {
            if (pBuff[HEADER_SZ] > 0)
            {
                if (INTF_READ(&pBuff[HEADER_SZ + 1], pBuff[HEADER_SZ] + FOOTER_SZ) == ESP_OK)
                {
                    *pBytesRead = pBuff[HEADER_SZ] + HEADER_SZ + 1;
                }
                else
                    return ERROR;
            }
            else
            {
                *pBytesRead = HEADER_SZ + 1;
            }
        }
        else
            return ERROR;
    }
    else
        return ERROR;

    return SUCCESS;
}

Status tml_WaitForRx(uint32_t timeout)
{
    TickType_t xTicksToWait;
    if (timeout == 0)
    {
        xTicksToWait = portMAX_DELAY;
    }
    else
    {
        xTicksToWait = pdMS_TO_TICKS(timeout);
    }
    BaseType_t ret = xSemaphoreTake(pn7160_semaphore, xTicksToWait);

    if (ret == pdTRUE)
    {
        return SUCCESS;
    }
    else
    {
        ESP_LOGW(TAG, "tml_WaitForRx timeout (timeout: %lu ms)", timeout);
        return ERROR;
    }
    return SUCCESS;
}

/* Public API */

void tml_Connect(void)
{
    tml_Init();
    tml_Reset();
}

void tml_Disconnect(void)
{
    tml_DeInit();
}

void tml_EnterDwlMode(void)
{
    isDwlMode = true;
    gpio_set_level(PN7160_DWL_PIN, 1);
    tml_Reset();
}

void tml_LeaveDwlMode(void)
{
    isDwlMode = false;
    gpio_set_level(PN7160_DWL_PIN, 0);
    tml_Reset();
}

void tml_Send(uint8_t *pBuffer, uint16_t BufferLen, uint16_t *pBytesSent)
{
    if (tml_Tx(pBuffer, BufferLen) == ERROR)
        *pBytesSent = 0;
    else
        *pBytesSent = BufferLen;
}

void tml_Receive(uint8_t *pBuffer, uint16_t BufferLen, uint16_t *pBytes, uint16_t timeout)
{
    if (tml_WaitForRx(timeout) == ERROR)
        *pBytes = 0;
    else
        tml_Rx(pBuffer, BufferLen, pBytes);
}
