#include "tml.h"
#include "Nfc.h"

static const char *TAG = "main";

/* Discovery loop configuration according to the targeted modes of operation */
unsigned char DiscoveryTechnologies[] = {
    MODE_POLL | TECH_PASSIVE_NFCA,
    MODE_POLL | TECH_PASSIVE_NFCB,
    MODE_POLL | TECH_PASSIVE_15693};

/* Semaphore used to notify card detection interrupt */
SemaphoreHandle_t pn7160_semaphore = NULL;

/* I2C handles */
i2c_master_bus_handle_t bus_handle = NULL;    // I2C master bus handle
i2c_master_dev_handle_t pn7160_handle = NULL; // pn7160 I2C device handle

/* Card number */
uint8_t g_card_count = 0; // Number of stored cards

#if 0
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
#else
void PCD_MIFARE_scenario(void)
{
#define BLK_NB_MFC 4
#define KEY_MFC 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
#define DATA_WRITE_MFC 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff

    bool status;
    unsigned char Resp[256];
    unsigned char RespSize;
    /* Authenticate sector 1 with generic keys */
    unsigned char Auth[] = {0x40, BLK_NB_MFC / 4, 0x10, KEY_MFC};
    /* Read block 4 */
    unsigned char Read[] = {0x10, 0x30, BLK_NB_MFC};
    /* Write block 4 */
    unsigned char WritePart1[] = {0x10, 0xA0, BLK_NB_MFC};
    unsigned char WritePart2[] = {0x10, DATA_WRITE_MFC};

    /* Authenticate */
    status = NxpNci_ReaderTagCmd(Auth, sizeof(Auth), Resp, &RespSize);
    if ((status == NFC_ERROR) || (Resp[RespSize - 1] != 0))
    {
        ESP_LOGI(TAG, " Authenticate sector %d failed with error 0x%02x\n", Auth[1], Resp[RespSize - 1]);
        return;
    }
    ESP_LOGI(TAG, " Authenticate sector %d succeed\n", Auth[1]);

    /* Read block */
    status = NxpNci_ReaderTagCmd(Read, sizeof(Read), Resp, &RespSize);
    if ((status == NFC_ERROR) || (Resp[RespSize - 1] != 0))
    {
        ESP_LOGI(TAG, " Read block %d failed with error 0x%02x\n", Read[2], Resp[RespSize - 1]);
        return;
    }
    ESP_LOGI(TAG, " Read block %d:", Read[2]);
    PRINT_BUF(" ", (Resp + 1), RespSize - 2);

    /* Write block */
    status = NxpNci_ReaderTagCmd(WritePart1, sizeof(WritePart1), Resp, &RespSize);
    if ((status == NFC_ERROR) || (Resp[RespSize - 1] != 0x14))
    {
        ESP_LOGI(TAG, " Write block %d failed with error 0x%02x\n", WritePart1[2], Resp[RespSize - 1]);
        return;
    }
    status = NxpNci_ReaderTagCmd(WritePart2, sizeof(WritePart2), Resp, &RespSize);
    if ((status == NFC_ERROR) || (Resp[RespSize - 1] != 0x14))
    {
        ESP_LOGI(TAG, " Write block %d failed with error 0x%02x\n", WritePart1[2], Resp[RespSize - 1]);
        return;
    }
    ESP_LOGI(TAG, " Block %d written\n", WritePart1[2]);

    /* Read block */
    status = NxpNci_ReaderTagCmd(Read, sizeof(Read), Resp, &RespSize);
    if ((status == NFC_ERROR) || (Resp[RespSize - 1] != 0))
    {
        ESP_LOGI(TAG, " Read failed with error 0x%02x\n", Resp[RespSize - 1]);
        return;
    }
    ESP_LOGI(TAG, " Read block %d:", Read[2]);
    PRINT_BUF(" ", (Resp + 1), RespSize - 2);
}

void PCD_ISO15693_scenario(void)
{
#define BLK_NB_ISO15693 8
#define DATA_WRITE_ISO15693 0x11, 0x22, 0x33, 0x44

    bool status;
    unsigned char Resp[256];
    unsigned char RespSize;
    unsigned char ReadBlock[] = {0x02, 0x20, BLK_NB_ISO15693};
    unsigned char WriteBlock[] = {0x02, 0x21, BLK_NB_ISO15693, DATA_WRITE_ISO15693};

    status = NxpNci_ReaderTagCmd(ReadBlock, sizeof(ReadBlock), Resp, &RespSize);
    if ((status == NFC_ERROR) || (Resp[RespSize - 1] != 0x00))
    {
        ESP_LOGI(TAG, " Read block %d failed with error 0x%02x\n", ReadBlock[2], Resp[RespSize - 1]);
        return;
    }
    ESP_LOGI(TAG, " Read block %d:", ReadBlock[2]);
    PRINT_BUF(" ", (Resp + 1), RespSize - 2);

    /* Write */
    status = NxpNci_ReaderTagCmd(WriteBlock, sizeof(WriteBlock), Resp, &RespSize);
    if ((status == NFC_ERROR) || (Resp[RespSize - 1] != 0))
    {
        ESP_LOGI(TAG, " Write block %d failed with error 0x%02x\n", WriteBlock[2], Resp[RespSize - 1]);
        return;
    }
    ESP_LOGI(TAG, " Block %d written\n", WriteBlock[2]);

    /* Read back */
    status = NxpNci_ReaderTagCmd(ReadBlock, sizeof(ReadBlock), Resp, &RespSize);
    if ((status == NFC_ERROR) || (Resp[RespSize - 1] != 0x00))
    {
        ESP_LOGI(TAG, " Read block %d failed with error 0x%02x\n", ReadBlock[2], Resp[RespSize - 1]);
        return;
    }
    ESP_LOGI(TAG, " Read block %d:", ReadBlock[2]);
    PRINT_BUF(" ", (Resp + 1), RespSize - 2);
}

void PCD_ISO14443_3A_scenario(void)
{
#define BLK_NB_ISO14443_3A 5
#define DATA_WRITE_ISO14443_3A 0x11, 0x22, 0x33, 0x44

    bool status;
    unsigned char Resp[256];
    unsigned char RespSize;
    /* Read block */
    unsigned char Read[] = {0x30, BLK_NB_ISO14443_3A};
    /* Write block */
    unsigned char Write[] = {0xA2, BLK_NB_ISO14443_3A, DATA_WRITE_ISO14443_3A};

    /* Read */
    status = NxpNci_ReaderTagCmd(Read, sizeof(Read), Resp, &RespSize);
    if ((status == NFC_ERROR) || (Resp[RespSize - 1] != 0))
    {
        ESP_LOGI(TAG, " Read block %d failed with error 0x%02x\n", Read[1], Resp[RespSize - 1]);
        return;
    }
    ESP_LOGI(TAG, " Read block %d:", Read[1]);
    PRINT_BUF(" ", Resp, 4);
    /* Write */
    status = NxpNci_ReaderTagCmd(Write, sizeof(Write), Resp, &RespSize);
    if ((status == NFC_ERROR) || (Resp[RespSize - 1] != 0x14))
    {
        ESP_LOGI(TAG, " Write block %d failed with error 0x%02x\n", Write[1], Resp[RespSize - 1]);
        return;
    }
    ESP_LOGI(TAG, " Block %d written\n", Write[1]);

    /* Read back */
    status = NxpNci_ReaderTagCmd(Read, sizeof(Read), Resp, &RespSize);
    if ((status == NFC_ERROR) || (Resp[RespSize - 1] != 0))
    {
        ESP_LOGI(TAG, " Read block %d failed with error 0x%02x\n", Read[1], Resp[RespSize - 1]);
        return;
    }
    ESP_LOGI(TAG, " Read block %d:", Read[1]);
    PRINT_BUF(" ", Resp, 4);
}

void PCD_ISO14443_4_scenario(void)
{
    bool status;
    unsigned char Resp[256];
    unsigned char RespSize;
    unsigned char SelectPPSE[] = {0x00, 0xA4, 0x04, 0x00, 0x0E, 0x32, 0x50, 0x41, 0x59, 0x2E, 0x53, 0x59, 0x53, 0x2E, 0x44, 0x44, 0x46, 0x30, 0x31, 0x00};

    status = NxpNci_ReaderTagCmd(SelectPPSE, sizeof(SelectPPSE), Resp, &RespSize);
    if ((status == NFC_ERROR) || (Resp[RespSize - 2] != 0x90) || (Resp[RespSize - 1] != 0x00))
    {
        ESP_LOGI(TAG, " Select PPSE failed with error %02x %02x\n", Resp[RespSize - 2], Resp[RespSize - 1]);
        return;
    }
    ESP_LOGI(TAG, " Select PPSE Application succeed\n");
}

void displayCardInfo(NxpNci_RfIntf_t RfIntf)
{
    switch (RfIntf.Protocol)
    {
    case PROT_T1T:
    case PROT_T2T:
    case PROT_T3T:
    case PROT_ISODEP:
        ESP_LOGI(TAG, " - POLL MODE: Remote T%dT activated\n", RfIntf.Protocol);
        break;
    case PROT_T5T:
        ESP_LOGI(TAG, " - POLL MODE: Remote ISO15693 card activated\n");
        break;
    case PROT_MIFARE:
        ESP_LOGI(TAG, " - POLL MODE: Remote MIFARE card activated\n");
        break;
    default:
        ESP_LOGI(TAG, " - POLL MODE: Undetermined target\n");
        return;
    }

    switch (RfIntf.ModeTech)
    {
    case (MODE_POLL | TECH_PASSIVE_NFCA):
        ESP_LOGI(TAG, "\tSENS_RES = 0x%.2x 0x%.2x\n", RfIntf.Info.NFC_APP.SensRes[0], RfIntf.Info.NFC_APP.SensRes[1]);
        PRINT_BUF("\tNFCID = ", RfIntf.Info.NFC_APP.NfcId, RfIntf.Info.NFC_APP.NfcIdLen);
        if (RfIntf.Info.NFC_APP.SelResLen != 0)
            ESP_LOGI(TAG, "\tSEL_RES = 0x%.2x\n", RfIntf.Info.NFC_APP.SelRes[0]);
        break;

    case (MODE_POLL | TECH_PASSIVE_NFCB):
        if (RfIntf.Info.NFC_BPP.SensResLen != 0)
            PRINT_BUF("\tSENS_RES = ", RfIntf.Info.NFC_BPP.SensRes, RfIntf.Info.NFC_BPP.SensResLen);
        break;

    case (MODE_POLL | TECH_PASSIVE_NFCF):
        ESP_LOGI(TAG, "\tBitrate = %s\n", (RfIntf.Info.NFC_FPP.BitRate == 1) ? "212" : "424");
        if (RfIntf.Info.NFC_FPP.SensResLen != 0)
            PRINT_BUF("\tSENS_RES = ", RfIntf.Info.NFC_FPP.SensRes, RfIntf.Info.NFC_FPP.SensResLen);
        break;

    case (MODE_POLL | TECH_PASSIVE_15693):
        PRINT_BUF("\tID = ", RfIntf.Info.NFC_VPP.ID, sizeof(RfIntf.Info.NFC_VPP.ID));
        ESP_LOGI(TAG, "\tAFI = 0x%.2x\n", RfIntf.Info.NFC_VPP.AFI);
        ESP_LOGI(TAG, "\tDSFID = 0x%.2x\n", RfIntf.Info.NFC_VPP.DSFID);
        break;

    default:
        break;
    }
}
void app_main(void)
{
    NxpNci_RfIntf_t RfInterface;

    if (NxpNci_Connect() == NFC_ERROR)
    {
        ESP_LOGI(TAG, "Error: cannot connect to NXPNCI device\n");
        return;
    }
    if (NxpNci_ConfigureSettings() == NFC_ERROR)
    {
        ESP_LOGI(TAG, "Error: cannot configure NXPNCI settings\n");
        return;
    }

    if (NxpNci_ConfigureMode(NXPNCI_MODE_RW) == NFC_ERROR)
    {
        ESP_LOGI(TAG, "Error: cannot configure NXPNCI\n");
        return;
    }

    /* Start Discovery */
    if (NxpNci_StartDiscovery(DiscoveryTechnologies, sizeof(DiscoveryTechnologies)) != NFC_SUCCESS)
    {
        ESP_LOGI(TAG, "Error: cannot start discovery\n");
        return;
    }

    while (1)
    {
        ESP_LOGI(TAG, "\nWAITING FOR DEVICE DISCOVERY\n");

        /* Wait until a peer is discovered */
        while (NxpNci_WaitForDiscoveryNotification(&RfInterface) != NFC_SUCCESS)
            ;

        if ((RfInterface.ModeTech & MODE_MASK) == MODE_POLL)
        {
            /* For each discovered cards */
            while (1)
            {
                /* Display detected card information */
                displayCardInfo(RfInterface);

                /* What's the detected card type ? */
                switch (RfInterface.Protocol)
                {
                case PROT_T2T:
                    PCD_ISO14443_3A_scenario();
                    break;
                case PROT_ISODEP:
                    PCD_ISO14443_4_scenario();
                    break;
                case PROT_T5T:
                    PCD_ISO15693_scenario();
                    break;
                case PROT_MIFARE:
                    PCD_MIFARE_scenario();
                    break;
                default:
                    break;
                }

                /* If more cards (or multi-protocol card) were discovered (only same technology are supported) select next one */
                if (RfInterface.MoreTags)
                {
                    if (NxpNci_ReaderActivateNext(&RfInterface) == NFC_ERROR)
                        break;
                }
                /* Otherwise leave */
                else
                    break;
            }

            /* Wait for card removal */
            NxpNci_ProcessReaderMode(RfInterface, PRESENCE_CHECK);

            ESP_LOGI(TAG, "CARD REMOVED\n");

            /* Restart discovery loop */
            NxpNci_StopDiscovery();
            while (NxpNci_StartDiscovery(DiscoveryTechnologies, sizeof(DiscoveryTechnologies)))
                ;
        }
        else
        {
            ESP_LOGI(TAG, "WRONG DISCOVERY\n");
        }
    }
}

#endif