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
        ESP_LOGI(TAG, "Authenticate sector %d failed with error 0x%02x", Auth[1], Resp[RespSize - 1]);
        return;
    }
    ESP_LOGI(TAG, "Authenticate sector %d succeed", Auth[1]);

    /* Read block */
    status = NxpNci_ReaderTagCmd(Read, sizeof(Read), Resp, &RespSize);
    if ((status == NFC_ERROR) || (Resp[RespSize - 1] != 0))
    {
        ESP_LOGI(TAG, "Read block %d failed with error 0x%02x", Read[2], Resp[RespSize - 1]);
        return;
    }
    ESP_LOGI(TAG, "Read block %d:", Read[2]);
    PRINT_BUF(" ", (Resp + 1), RespSize - 2);

    /* Write block */
    status = NxpNci_ReaderTagCmd(WritePart1, sizeof(WritePart1), Resp, &RespSize);
    if ((status == NFC_ERROR) || (Resp[RespSize - 1] != 0x14))
    {
        ESP_LOGI(TAG, "Write block %d failed with error 0x%02x", WritePart1[2], Resp[RespSize - 1]);
        return;
    }
    status = NxpNci_ReaderTagCmd(WritePart2, sizeof(WritePart2), Resp, &RespSize);
    if ((status == NFC_ERROR) || (Resp[RespSize - 1] != 0x14))
    {
        ESP_LOGI(TAG, "Write block %d failed with error 0x%02x", WritePart1[2], Resp[RespSize - 1]);
        return;
    }
    ESP_LOGI(TAG, "Block %d written", WritePart1[2]);

    /* Read block */
    status = NxpNci_ReaderTagCmd(Read, sizeof(Read), Resp, &RespSize);
    if ((status == NFC_ERROR) || (Resp[RespSize - 1] != 0))
    {
        ESP_LOGI(TAG, "Read failed with error 0x%02x", Resp[RespSize - 1]);
        return;
    }
    ESP_LOGI(TAG, "Read block %d:", Read[2]);
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
        ESP_LOGI(TAG, "Read block %d failed with error 0x%02x", ReadBlock[2], Resp[RespSize - 1]);
        return;
    }
    ESP_LOGI(TAG, "Read block %d:", ReadBlock[2]);
    PRINT_BUF("", (Resp + 1), RespSize - 2);

    /* Write */
    status = NxpNci_ReaderTagCmd(WriteBlock, sizeof(WriteBlock), Resp, &RespSize);
    if ((status == NFC_ERROR) || (Resp[RespSize - 1] != 0))
    {
        ESP_LOGI(TAG, "Write block %d failed with error 0x%02x", WriteBlock[2], Resp[RespSize - 1]);
        return;
    }
    ESP_LOGI(TAG, "Block %d written", WriteBlock[2]);

    /* Read back */
    status = NxpNci_ReaderTagCmd(ReadBlock, sizeof(ReadBlock), Resp, &RespSize);
    if ((status == NFC_ERROR) || (Resp[RespSize - 1] != 0x00))
    {
        ESP_LOGI(TAG, "Read block %d failed with error 0x%02x", ReadBlock[2], Resp[RespSize - 1]);
        return;
    }
    ESP_LOGI(TAG, "Read block %d:", ReadBlock[2]);
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
        ESP_LOGI(TAG, "Read block %d failed with error 0x%02x", Read[1], Resp[RespSize - 1]);
        return;
    }
    ESP_LOGI(TAG, "Read block %d:", Read[1]);
    PRINT_BUF(" ", Resp, 4);
    /* Write */
    status = NxpNci_ReaderTagCmd(Write, sizeof(Write), Resp, &RespSize);
    if ((status == NFC_ERROR) || (Resp[RespSize - 1] != 0x14))
    {
        ESP_LOGI(TAG, "Write block %d failed with error 0x%02x", Write[1], Resp[RespSize - 1]);
        return;
    }
    ESP_LOGI(TAG, "Block %d written", Write[1]);

    /* Read back */
    status = NxpNci_ReaderTagCmd(Read, sizeof(Read), Resp, &RespSize);
    if ((status == NFC_ERROR) || (Resp[RespSize - 1] != 0))
    {
        ESP_LOGI(TAG, "Read block %d failed with error 0x%02x", Read[1], Resp[RespSize - 1]);
        return;
    }
    ESP_LOGI(TAG, "Read block %d:", Read[1]);
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
        ESP_LOGI(TAG, "Select PPSE failed with error %02x %02x", Resp[RespSize - 2], Resp[RespSize - 1]);
        return;
    }
    ESP_LOGI(TAG, "Select PPSE Application succeed");
}

void displayCardInfo(NxpNci_RfIntf_t RfIntf)
{
    switch (RfIntf.Protocol)
    {
    case PROT_T1T:
    case PROT_T2T:
    case PROT_T3T:
    case PROT_ISODEP:
        ESP_LOGI(TAG, "- POLL MODE: Remote T%dT activated", RfIntf.Protocol);
        break;
    case PROT_T5T:
        ESP_LOGI(TAG, "- POLL MODE: Remote ISO15693 card activated");
        break;
    case PROT_MIFARE:
        ESP_LOGI(TAG, "- POLL MODE: Remote MIFARE card activated");
        break;
    default:
        ESP_LOGI(TAG, "- POLL MODE: Undetermined target");
        return;
    }

    switch (RfIntf.ModeTech)
    {
    case (MODE_POLL | TECH_PASSIVE_NFCA):
        ESP_LOGI(TAG, "SENS_RES = 0x%.2x 0x%.2x", RfIntf.Info.NFC_APP.SensRes[0], RfIntf.Info.NFC_APP.SensRes[1]);
        PRINT_BUF("NFCID = ", RfIntf.Info.NFC_APP.NfcId, RfIntf.Info.NFC_APP.NfcIdLen);
        if (RfIntf.Info.NFC_APP.SelResLen != 0)
            ESP_LOGI(TAG, "SEL_RES = 0x%.2x", RfIntf.Info.NFC_APP.SelRes[0]);
        break;

    case (MODE_POLL | TECH_PASSIVE_NFCB):
        if (RfIntf.Info.NFC_BPP.SensResLen != 0)
            PRINT_BUF("SENS_RES = ", RfIntf.Info.NFC_BPP.SensRes, RfIntf.Info.NFC_BPP.SensResLen);
        break;

    case (MODE_POLL | TECH_PASSIVE_NFCF):
        ESP_LOGI(TAG, "Bitrate = %s", (RfIntf.Info.NFC_FPP.BitRate == 1) ? "212" : "424");
        if (RfIntf.Info.NFC_FPP.SensResLen != 0)
            PRINT_BUF("SENS_RES = ", RfIntf.Info.NFC_FPP.SensRes, RfIntf.Info.NFC_FPP.SensResLen);
        break;

    case (MODE_POLL | TECH_PASSIVE_15693):
        PRINT_BUF("ID = ", RfIntf.Info.NFC_VPP.ID, sizeof(RfIntf.Info.NFC_VPP.ID));
        ESP_LOGI(TAG, "AFI = 0x%.2x", RfIntf.Info.NFC_VPP.AFI);
        ESP_LOGI(TAG, "DSFID = 0x%.2x", RfIntf.Info.NFC_VPP.DSFID);
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
        ESP_LOGI(TAG, "Error: cannot connect to NXPNCI device");
        return;
    }
    if (NxpNci_ConfigureSettings() == NFC_ERROR)
    {
        ESP_LOGI(TAG, "Error: cannot configure NXPNCI settings");
        return;
    }

    if (NxpNci_ConfigureMode(NXPNCI_MODE_RW) == NFC_ERROR)
    {
        ESP_LOGI(TAG, "Error: cannot configure NXPNCI");
        return;
    }

    /* Start Discovery */
    if (NxpNci_StartDiscovery(DiscoveryTechnologies, sizeof(DiscoveryTechnologies)) != NFC_SUCCESS)
    {
        ESP_LOGI(TAG, "Error: cannot start discovery");
        return;
    }

    while (1)
    {
        ESP_LOGI(TAG, "WAITING FOR DEVICE DISCOVERY");

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

            ESP_LOGI(TAG, "CARD REMOVED");

            /* Restart discovery loop */
            NxpNci_StopDiscovery();
            while (NxpNci_StartDiscovery(DiscoveryTechnologies, sizeof(DiscoveryTechnologies)))
                ;
        }
        else
        {
            ESP_LOGI(TAG, "WRONG DISCOVERY");
        }
    }
}
