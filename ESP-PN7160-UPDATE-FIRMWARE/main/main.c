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

#define DL_CMD 0x00        // Download command
#define DL_RESET 0xF0      // Reset command
#define DL_GETVERSION 0xF1 // Get version command

#define MAX_FRAME_SIZE 1000             // Maximum frame size for PN7160
#define CHUNK_SIZE (MAX_FRAME_SIZE - 4) // Chunk size for data transfer

#define FORCE_DWL true

// firmware binary data ,34KB
extern uint8_t gphDnldNfc_DlSequence[];

// size of firmware binary data
extern uint32_t gphDnldNfc_DlSeqSz;

// Firmware version structure
typedef struct
{
    uint8_t ROM;
    uint8_t MAJ;
    uint8_t MIN;
} sFWu_version_t;

/* I2C handles */
i2c_master_bus_handle_t bus_handle;    // I2C master bus handle
i2c_master_dev_handle_t pn7160_handle; // pn7160 I2C device handle

/* Card number */
uint8_t g_card_count = 0; // Number of stored cards

static const char *TAG = "main";

uint16_t BytesRead;           // records number of bytes read from PN7160
uint8_t pResponseBuffer[512]; // storage for response from PN7160
void pn7160_enter_download_mode(void)
{
    gpio_set_level(PN7160_DWL_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level(PN7160_RST_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(PN7160_RST_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(300));

    ESP_LOGI(TAG, "pn7160 entered download mode");
}

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
 * @brief This function calculates the CRC16 checksum for the given data.
 * @param[in] p - pointer to the data buffer
 * @param[in] dwLength - length of the data buffer
 * @return calculated CRC16 checksum
 **/
static uint16_t sFWu_CalcCrc16(uint8_t *p, uint32_t dwLength)
{
    uint32_t i;
    uint16_t crc_new;
    uint16_t crc = 0xffffU;

    for (i = 0; i < dwLength; i++)
    {
        crc_new = (uint8_t)(crc >> 8) | (crc << 8);
        crc_new ^= p[i];
        crc_new ^= (uint8_t)(crc_new & 0xff) >> 4;
        crc_new ^= crc_new << 12;
        crc_new ^= (crc_new & 0xff) << 5;
        crc = crc_new;
    }
    return crc;
}

/**
 * @brief This function handles the transceive operation with the PN7160.
 * @param[in] pTBuff - pointer to the transmit buffer
 * @param[in] TbuffLen - length of the transmit buffer
 * @param[out] pRBuff - pointer to the receive buffer
 * @param[in] RBuffSize - size of the receive buffer
 * @param[out] pBytesread - pointer to store the number of bytes read
 * @return void
 **/
static void sFWu_Transceive(uint8_t *pTBuff, uint16_t TbuffLen, uint8_t *pRBuff, uint16_t RBuffSize, uint16_t *pBytesread)
{
    uint8_t header[2];
    uint16_t payload_len;

    /* 1. Write command */
    i2c_master_transmit(pn7160_handle, pTBuff, TbuffLen, portMAX_DELAY);
    // ESP_LOG_BUFFER_HEX("PN7160 TX", pTBuff, TbuffLen);
    vTaskDelay(pdMS_TO_TICKS(50));

    /* 2. Read length header (2 bytes) */
    i2c_master_receive(pn7160_handle, header, 2, portMAX_DELAY);
    // ESP_LOG_BUFFER_HEX("PN7160 RX Header", header, 2);
    payload_len = ((header[0] & 0x03) << 8) | header[1];

    if (payload_len > RBuffSize - 4)
        payload_len = RBuffSize - 4;

    /* 3. Read payload */
    i2c_master_receive(pn7160_handle, pRBuff + 2, payload_len + 2, portMAX_DELAY);

    pRBuff[0] = header[0];
    pRBuff[1] = header[1];

    *pBytesread = payload_len + 4;

    // ESP_LOGI(TAG, "PN7160 RX total len=%d", *pBytesread);
    // ESP_LOG_BUFFER_HEX("PN7160 RX", pRBuff, *pBytesread);
}

/**
 * @brief This function resets the PN7160 firmware.
 * @return status of the reset operation
 **/
uint8_t sFWu_Reset(void)
{
    uint16_t crc16 = 0x00;
    uint8_t Cmd[8] = {0};

    Cmd[0] = DL_CMD;
    Cmd[1] = 0x04;
    Cmd[2] = DL_RESET;
    crc16 = sFWu_CalcCrc16(Cmd, 0x06);
    Cmd[6] = (uint8_t)((crc16 & 0xff00) >> 0x08);
    Cmd[7] = (uint8_t)crc16;

    sFWu_Transceive(Cmd, sizeof(Cmd), pResponseBuffer, sizeof(pResponseBuffer), &BytesRead);
    if ((BytesRead > 0) || (pResponseBuffer[2] != 0))
        return pResponseBuffer[2];

    return 0;
}
/**
 * @brief This function retrieves the firmware version from the PN7160.
 * @param[out] pVer - pointer to store the firmware version
 * @return status of the get version operation
 **/
uint8_t sFWu_GetVersion(sFWu_version_t *pVer)
{
    uint16_t crc16 = 0x00;
    uint8_t Cmd[8] = {0};

    Cmd[0] = DL_CMD;
    Cmd[1] = 0x04;
    Cmd[2] = DL_GETVERSION;
    crc16 = sFWu_CalcCrc16(Cmd, 0x06);
    Cmd[6] = (uint8_t)((crc16 & 0xff00) >> 0x08);
    Cmd[7] = (uint8_t)crc16;

    sFWu_Transceive(Cmd, sizeof(Cmd), pResponseBuffer, sizeof(pResponseBuffer), &BytesRead);
    if ((BytesRead < 12) || (pResponseBuffer[2] != 0))
        return pResponseBuffer[2];

    pVer->ROM = pResponseBuffer[4];
    pVer->MAJ = pResponseBuffer[11];
    pVer->MIN = pResponseBuffer[10];

    return 0;
}
/**
 * @brief This function downloads the firmware to the PN7160.
 * @param[in] pFile - pointer to the firmware file buffer
 * @param[in] fileSize - size of the firmware file
 * @return status of the download operation
 **/
uint8_t sFWu_Download(uint8_t *pFile, uint16_t fileSize)
{
    uint16_t index = 0;
    uint16_t frame_size;
    uint8_t pTempBuffer[MAX_FRAME_SIZE];
    uint16_t crc16 = 0x00;

    while (index < fileSize)
    {
        frame_size = (pFile[index] << 8) + pFile[index + 1];

        /* Is chunking required ? */
        if (frame_size > CHUNK_SIZE)
        {
            /* first chunck */
            index += 2; /* Skip frame size */
            pTempBuffer[0] = (uint8_t)(((MAX_FRAME_SIZE - 4) & 0xff00) >> 0x08) | 0x04;
            pTempBuffer[1] = (uint8_t)(MAX_FRAME_SIZE - 4);
            memcpy(pTempBuffer + 2, pFile + index, CHUNK_SIZE);
            crc16 = sFWu_CalcCrc16(pTempBuffer, CHUNK_SIZE + 2);
            pTempBuffer[CHUNK_SIZE + 2] = (uint8_t)((crc16 & 0xff00) >> 0x08);
            pTempBuffer[CHUNK_SIZE + 3] = (uint8_t)crc16;
            sFWu_Transceive(pTempBuffer, MAX_FRAME_SIZE, pResponseBuffer, sizeof(pResponseBuffer), &BytesRead);
            if ((BytesRead != 8) || (pResponseBuffer[2] != 0x2D))
            {
                return pResponseBuffer[2];
            }
            index += CHUNK_SIZE;
            frame_size -= CHUNK_SIZE;

            /* intermediate chunks */
            while (frame_size > CHUNK_SIZE)
            {
                pTempBuffer[0] = (uint8_t)(((MAX_FRAME_SIZE - 4) & 0xff00) >> 0x08) | 0x04;
                pTempBuffer[1] = (uint8_t)(MAX_FRAME_SIZE - 4);
                memcpy(pTempBuffer + 2, pFile + index, CHUNK_SIZE);
                crc16 = sFWu_CalcCrc16(pTempBuffer, CHUNK_SIZE + 2);
                pTempBuffer[CHUNK_SIZE + 2] = (uint8_t)((crc16 & 0xff00) >> 0x08);
                pTempBuffer[CHUNK_SIZE + 3] = (uint8_t)crc16;
                sFWu_Transceive(pTempBuffer, MAX_FRAME_SIZE, pResponseBuffer, sizeof(pResponseBuffer), &BytesRead);
                if ((BytesRead != 8) || (pResponseBuffer[2] != 0x2E))
                {
                    return pResponseBuffer[2];
                }
                index += CHUNK_SIZE;
                frame_size -= CHUNK_SIZE;
            }

            /* last chunk */
            pTempBuffer[0] = (uint8_t)((frame_size & 0xff00) >> 0x08);
            pTempBuffer[1] = (uint8_t)frame_size;
            memcpy(pTempBuffer + 2, pFile + index, frame_size);
            crc16 = sFWu_CalcCrc16(pTempBuffer, frame_size + 2);
            pTempBuffer[frame_size + 2] = (uint8_t)((crc16 & 0xff00) >> 0x08);
            pTempBuffer[frame_size + 3] = (uint8_t)crc16;
            sFWu_Transceive(pTempBuffer, frame_size + 4, pResponseBuffer, sizeof(pResponseBuffer), &BytesRead);
            if ((BytesRead != 8) || (pResponseBuffer[2] != 0))
            {
                return pResponseBuffer[2];
            }
            index += frame_size;
        }
        else
        {
            memcpy(pTempBuffer, pFile + index, frame_size + 2);
            crc16 = sFWu_CalcCrc16(pTempBuffer, frame_size + 2);
            pTempBuffer[frame_size + 2] = (uint8_t)((crc16 & 0xff00) >> 0x08);
            pTempBuffer[frame_size + 3] = (uint8_t)crc16;
            sFWu_Transceive(pTempBuffer, frame_size + 4, pResponseBuffer, sizeof(pResponseBuffer), &BytesRead);
            index += frame_size + 2;
            if ((BytesRead < 6) || (pResponseBuffer[2] != 0))
            {
                return pResponseBuffer[2];
            }
        }
    }
    return 0;
}

void pn7160_i2c_initialization(void)
{

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
}

void app_main(void)
{
    uint8_t error;
    sFWu_version_t Ver = {0};

    // Initialize I2C and PN7160
    pn7160_i2c_initialization();

    // Enter download mode
    pn7160_enter_download_mode();

    /* Retrieve current FW version in IC */
    sFWu_GetVersion(&Ver);

    ESP_LOGI(TAG, "Current FW Version: ROM: 0x%.2X, MAJ: 0x%.2X, MIN: 0x%.2X", Ver.ROM, Ver.MAJ, Ver.MIN);

    // /* Check current FW version from binary */
    if ((Ver.MAJ == gphDnldNfc_DlSequence[5]) && (Ver.MIN == gphDnldNfc_DlSequence[4]) && (FORCE_DWL == false))
    {
        ESP_LOGI(TAG, "FW already up to date\n");
    }
    else
    {
        /* Downloading FW according sequence from FW c file */
        ESP_LOGI(TAG, "Downloading FW version %.2X.%.2X\n", gphDnldNfc_DlSequence[5], gphDnldNfc_DlSequence[4]);
        error = sFWu_Download(gphDnldNfc_DlSequence, gphDnldNfc_DlSeqSz);
        if (error)
        {
            ESP_LOGE(TAG, "Error downloading: 0x%.2X\n", error);
            pn7160_exit_download_mode();
        }
        else
        {
            ESP_LOGI(TAG, "Download succeed\n");
            /* Retrieve current FW version in IC */
            sFWu_GetVersion(&Ver);
        }
    }
}