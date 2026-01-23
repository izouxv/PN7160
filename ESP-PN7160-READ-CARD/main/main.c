#include <string.h>
#include <stdbool.h>
#include <stdint.h>
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

/* Semaphore used to notify card detection interrupt */
SemaphoreHandle_t pn7160_semaphore = NULL;

/* I2C handles */
i2c_master_bus_handle_t bus_handle;    // I2C master bus handle
i2c_master_dev_handle_t pn7160_handle; // pn7160 I2C device handle

/* Card number */
uint8_t g_card_count = 0; // Number of stored cards

static const char *TAG = "main";

void pn7160_exit_download_mode(void)
{
    gpio_set_level(PN7160_DWL_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level(PN7160_RST_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(PN7160_RST_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(300));

    ESP_LOGI(TAG, "pn7160 exited download mode");
}
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

void pn7160_i2c_initialization(void)
{
    /* Create binary semaphore */
    pn7160_semaphore = xSemaphoreCreateBinary();
    if (pn7160_semaphore == NULL)
    {
        ESP_LOGE(TAG, "Semaphore creation failed");
    }

    /* Initialize I2C bus */
    i2c_master_bus_config_t i2c_cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_MASTER_NUM,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_cfg, &bus_handle));
    ESP_LOGI(TAG, "I2C bus initialized");

    /* Add pn7160 device */
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = PN7160_I2C_ADDRESS,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_cfg, &pn7160_handle));
    ESP_LOGI(TAG, "pn7160 device added");

    /* Configure reset pin */
    gpio_config_t pn7160_rst_cfg = {
        .pin_bit_mask = (1ULL << PN7160_RST_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE};
    gpio_config(&pn7160_rst_cfg);

    /* Configure download pin */
    gpio_config_t pn7160_dwl_cfg = {
        .pin_bit_mask = (1ULL << PN7160_DWL_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE};
    gpio_config(&pn7160_dwl_cfg);

    /* Configure interrupt pin*/
    gpio_config_t pn7160_irq_cfg = {
        .pin_bit_mask = (1ULL << PN7160_INT_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_POSEDGE};
    gpio_config(&pn7160_irq_cfg);

    gpio_install_isr_service(0);

    gpio_isr_handler_add(PN7160_INT_PIN, gpio_isr_handler, (void *)PN7160_INT_PIN);
}
/**
 * @brief Card recognition task
 * @param arg Task parameter (unused, pass NULL)
 * @return void
 */
void pn7160_task(void *arg)
{
    uint8_t RF_INTF_ACTIVATED_NTF[24] = {0};                 // Buffer for RF interface activated notification
    uint8_t RF_DEACTIVATE_CMD[4] = {0x21, 0x06, 0x01, 0x03}; // RF deactivate command
    uint8_t RF_DEACTIVATE_RSP[4] = {0};                      // Buffer for RF deactivate response
    uint8_t RF_DEACTIVATE_NTF[5] = {0};                      // Buffer for RF deactivate notification
    uint64_t card_id_value = 0;
    uint8_t g_card_uid[8] = {0};
    while (1)
    {
        if (xSemaphoreTake(pn7160_semaphore, portMAX_DELAY) == pdTRUE)
        {
            // failed frame:60 07 01 a1
            // successful frame:61 05 15 01 01 02 00 ff 01 0a 04 00 04 98 8c b3 a2 01 08 00 00 00 00 00
            if (i2c_master_receive(pn7160_handle, RF_INTF_ACTIVATED_NTF, sizeof(RF_INTF_ACTIVATED_NTF), portMAX_DELAY) == ESP_OK)
            {
                ESP_LOGI(TAG, "Card detected");
                ESP_LOG_BUFFER_HEX(TAG, RF_INTF_ACTIVATED_NTF, sizeof(RF_INTF_ACTIVATED_NTF));
                if (RF_INTF_ACTIVATED_NTF[0] == 0x60 && RF_INTF_ACTIVATED_NTF[1] == 0x07 && RF_INTF_ACTIVATED_NTF[2] == 0x01 && RF_INTF_ACTIVATED_NTF[3] == 0xa1)
                {
                    ESP_LOGW(TAG, "Card detection failed");
                    continue; // Skip failed detection
                }
                g_card_count++;
                card_id_value = 0;
                uint8_t card_id_len = RF_INTF_ACTIVATED_NTF[12]; // Card ID length

                if (card_id_len < 1 || card_id_len > 8)
                {
                    ESP_LOGE(TAG, "Invalid card ID length: %hhu", card_id_len);
                    continue; // Skip invalid card
                }

                memcpy(g_card_uid, &RF_INTF_ACTIVATED_NTF[13], card_id_len); // Copy card UID

                for (uint8_t i = 0; i < card_id_len; i++)
                {
                    card_id_value |= ((uint64_t)g_card_uid[i]) << ((card_id_len - i - 1) * 8);
                }
                ESP_LOGI(TAG, "Card number: %d ,Card ID: 0x%llX", g_card_count, card_id_value);

                i2c_master_transmit(pn7160_handle, RF_DEACTIVATE_CMD, sizeof(RF_DEACTIVATE_CMD), portMAX_DELAY);
                xSemaphoreTake(pn7160_semaphore, portMAX_DELAY); // Wait for mapping
                i2c_master_receive(pn7160_handle, RF_DEACTIVATE_RSP, sizeof(RF_DEACTIVATE_RSP), portMAX_DELAY);
                ESP_LOGI(TAG, "RF deactivate response: %02x %02x %02x %02x", RF_DEACTIVATE_RSP[0], RF_DEACTIVATE_RSP[1], RF_DEACTIVATE_RSP[2], RF_DEACTIVATE_RSP[3]);
                i2c_master_receive(pn7160_handle, RF_DEACTIVATE_NTF, sizeof(RF_DEACTIVATE_NTF), portMAX_DELAY);
                ESP_LOGI(TAG, "RF deactivate notification:");
                ESP_LOG_BUFFER_HEX(TAG, RF_DEACTIVATE_NTF, sizeof(RF_DEACTIVATE_NTF));
            }
        }
    }
}

void pn7160_read_card_test(void)
{
    ESP_LOGI(TAG, "pn7160 reset completed");

    /* pn7160 initialization sequence */
    uint8_t CORE_RESET_CMD[4] = {0x20, 0x00, 0x01, 0x01}; // Core reset command, reset configuration
    i2c_master_transmit(pn7160_handle, CORE_RESET_CMD, sizeof(CORE_RESET_CMD), portMAX_DELAY);
    xSemaphoreTake(pn7160_semaphore, portMAX_DELAY); // Wait for reset to complete
    uint8_t CORE_RESET_RSP[4] = {0};
    i2c_master_receive(pn7160_handle, CORE_RESET_RSP, sizeof(CORE_RESET_RSP), portMAX_DELAY);
    ESP_LOGI(TAG, "pn7160 core reset response: %02x %02x %02x %02x", CORE_RESET_RSP[0], CORE_RESET_RSP[1], CORE_RESET_RSP[2], CORE_RESET_RSP[3]);
    uint8_t CORE_RESET_NTF[12] = {0};
    xSemaphoreTake(pn7160_semaphore, portMAX_DELAY); // Wait for reset to complete
    i2c_master_receive(pn7160_handle, CORE_RESET_NTF, sizeof(CORE_RESET_NTF), portMAX_DELAY);
    ESP_LOGI(TAG, "pn7160 core reset notification: ");
    ESP_LOG_BUFFER_HEX(TAG, CORE_RESET_NTF, sizeof(CORE_RESET_NTF));
    uint8_t CORE_INIT_CMD[3] = {0x20, 0x01, 0x00}; // Core init command
    i2c_master_transmit(pn7160_handle, CORE_INIT_CMD, sizeof(CORE_INIT_CMD), portMAX_DELAY);
    xSemaphoreTake(pn7160_semaphore, portMAX_DELAY); // Wait for init to complete
    uint8_t CORE_INIT_RSP[33] = {0};
    i2c_master_receive(pn7160_handle, CORE_INIT_RSP, sizeof(CORE_INIT_RSP), portMAX_DELAY);
    ESP_LOGI(TAG, "pn7160 core init response: ");
    ESP_LOG_BUFFER_HEX(TAG, CORE_INIT_RSP, sizeof(CORE_INIT_RSP));
    uint8_t NCI_PROPRIETARY_ACT_CMD[3] = {0x2F, 0x02, 0x00}; // NCI proprietary activation command
    i2c_master_transmit(pn7160_handle, NCI_PROPRIETARY_ACT_CMD, sizeof(NCI_PROPRIETARY_ACT_CMD), portMAX_DELAY);
    xSemaphoreTake(pn7160_semaphore, portMAX_DELAY); // Wait for activation
    uint8_t NCI_PROPRIETARY_ACT_RSP[8] = {0};
    i2c_master_receive(pn7160_handle, NCI_PROPRIETARY_ACT_RSP, sizeof(NCI_PROPRIETARY_ACT_RSP), portMAX_DELAY);
    ESP_LOGI(TAG, "pn7160 NCI proprietary activation response: ");
    ESP_LOG_BUFFER_HEX(TAG, NCI_PROPRIETARY_ACT_RSP, sizeof(NCI_PROPRIETARY_ACT_RSP));
    uint8_t RF_DISCOVER_MAP_CMD[7] = {0x21, 0x00, 0x04, 0x01, 0x04, 0x01, 0x02}; // RF discover map command
    i2c_master_transmit(pn7160_handle, RF_DISCOVER_MAP_CMD, sizeof(RF_DISCOVER_MAP_CMD), portMAX_DELAY);
    xSemaphoreTake(pn7160_semaphore, portMAX_DELAY); // Wait for mapping
    uint8_t RF_DISCOVER_MAP_RSP[4] = {0};
    i2c_master_receive(pn7160_handle, RF_DISCOVER_MAP_RSP, sizeof(RF_DISCOVER_MAP_RSP), portMAX_DELAY);
    ESP_LOGI(TAG, "pn7160 RF discover map response: %02x %02x %02x %02x", RF_DISCOVER_MAP_RSP[0], RF_DISCOVER_MAP_RSP[1], RF_DISCOVER_MAP_RSP[2], RF_DISCOVER_MAP_RSP[3]);
    uint8_t CORE_SET_CONFIG_CMD[18] = {0x20, 0x02, 0x0F, 0x01, 0xA0, 0x0E, 0x0B, 0x11, 0x01, 0xC2, 0xB2, 0x00, 0xDA, 0x1E, 0x01, 0x00, 0xD0, 0x0C}; // Core set config command
    i2c_master_transmit(pn7160_handle, CORE_SET_CONFIG_CMD, sizeof(CORE_SET_CONFIG_CMD), portMAX_DELAY);
    xSemaphoreTake(pn7160_semaphore, portMAX_DELAY); // Wait for config
    uint8_t CORE_SET_CONFIG_RSP[4] = {0};
    i2c_master_receive(pn7160_handle, CORE_SET_CONFIG_RSP, sizeof(CORE_SET_CONFIG_RSP), portMAX_DELAY);
    ESP_LOGI(TAG, "pn7160 core set config response: %02x %02x %02x %02x", CORE_SET_CONFIG_RSP[0], CORE_SET_CONFIG_RSP[1], CORE_SET_CONFIG_RSP[2], CORE_SET_CONFIG_RSP[3]);
    uint8_t CORE_RESET_CMD_KEEP[4] = {0x20, 0x00, 0x01, 0x00}; // Core reset command
    i2c_master_transmit(pn7160_handle, CORE_RESET_CMD_KEEP, sizeof(CORE_RESET_CMD_KEEP), portMAX_DELAY);
    xSemaphoreTake(pn7160_semaphore, portMAX_DELAY); // Wait for reset to complete
    i2c_master_receive(pn7160_handle, CORE_RESET_RSP, sizeof(CORE_RESET_RSP), portMAX_DELAY);
    ESP_LOGI(TAG, "pn7160 core reset response: %02x %02x %02x %02x", CORE_RESET_RSP[0], CORE_RESET_RSP[1], CORE_RESET_RSP[2], CORE_RESET_RSP[3]);
    xSemaphoreTake(pn7160_semaphore, portMAX_DELAY); // Wait for reset to complete
    i2c_master_receive(pn7160_handle, CORE_RESET_NTF, sizeof(CORE_RESET_NTF), portMAX_DELAY);
    ESP_LOGI(TAG, "pn7160 core reset notification: ");
    ESP_LOG_BUFFER_HEX(TAG, CORE_RESET_NTF, sizeof(CORE_RESET_NTF));
    i2c_master_transmit(pn7160_handle, CORE_INIT_CMD, sizeof(CORE_INIT_CMD), portMAX_DELAY);
    xSemaphoreTake(pn7160_semaphore, portMAX_DELAY); // Wait for init to complete
    i2c_master_receive(pn7160_handle, CORE_INIT_RSP, sizeof(CORE_INIT_RSP), portMAX_DELAY);
    ESP_LOGI(TAG, "pn7160 core init response: ");
    ESP_LOG_BUFFER_HEX(TAG, CORE_INIT_RSP, sizeof(CORE_INIT_RSP));
    uint8_t RF_DISCOVER_CMD[6] = {0x21, 0x03, 0x03, 0x01, 0x00, 0x01}; // RF discover command
    i2c_master_transmit(pn7160_handle, RF_DISCOVER_CMD, sizeof(RF_DISCOVER_CMD), portMAX_DELAY);
    xSemaphoreTake(pn7160_semaphore, portMAX_DELAY); // Wait for discovery
    uint8_t RF_DISCOVER_RSP[4] = {0};
    i2c_master_receive(pn7160_handle, RF_DISCOVER_RSP, sizeof(RF_DISCOVER_RSP), portMAX_DELAY);
    ESP_LOGI(TAG, "pn7160 RF discover response: %02x %02x %02x %02x", RF_DISCOVER_RSP[0], RF_DISCOVER_RSP[1], RF_DISCOVER_RSP[2], RF_DISCOVER_RSP[3]);
    ESP_LOGI(TAG, "pn7160 initialization completed");
    /* Create pn7160 task */
    xTaskCreate(pn7160_task, "pn7160_task", 8192, NULL, 10, NULL);
    ESP_LOGI(TAG, "pn7160 task started");
}

void app_main(void)
{
    ESP_LOGI(TAG, "PN7160 NFC Reader Card Detection Example Start");
    pn7160_i2c_initialization();
    pn7160_exit_download_mode();
    pn7160_read_card_test();
}
