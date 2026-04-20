#include <stdbool.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "Nfc.h"

#if DT_NODE_HAS_STATUS(DT_ALIAS(led0), okay)
static const struct gpio_dt_spec s_status_led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static bool s_status_led_ready;
#endif

static unsigned char DiscoveryTechnologies[] = {
	MODE_POLL | TECH_PASSIVE_NFCA,
	MODE_POLL | TECH_PASSIVE_NFCB,
	MODE_POLL | TECH_PASSIVE_15693,
};

static void status_led_init(void)
{
#if DT_NODE_HAS_STATUS(DT_ALIAS(led0), okay)
	if (device_is_ready(s_status_led.port) &&
	    gpio_pin_configure_dt(&s_status_led, GPIO_OUTPUT_INACTIVE) == 0) {
		s_status_led_ready = true;
	}
#endif
}

static void status_led_set(bool on)
{
#if DT_NODE_HAS_STATUS(DT_ALIAS(led0), okay)
	if (s_status_led_ready) {
		gpio_pin_set_dt(&s_status_led, on ? 1 : 0);
	}
#else
	ARG_UNUSED(on);
#endif
}

static void status_led_blink(uint32_t count, uint32_t delay_ms)
{
	for (uint32_t index = 0; index < count; ++index) {
		status_led_set(true);
		k_sleep(K_MSEC(delay_ms));
		status_led_set(false);
		k_sleep(K_MSEC(delay_ms));
	}
}

static void print_bytes(const char *label, const unsigned char *data, unsigned char len)
{
	printk("%s", label);
	for (unsigned char index = 0; index < len; ++index) {
		printk("%02X%s", data[index], (index + 1U < len) ? " " : "");
	}
	printk("\n");
}

static const char *protocol_to_string(unsigned char protocol)
{
	switch (protocol) {
	case PROT_T1T:
		return "T1T";
	case PROT_T2T:
		return "T2T";
	case PROT_T3T:
		return "T3T";
	case PROT_ISODEP:
		return "ISO-DEP";
	case PROT_NFCDEP:
		return "NFC-DEP";
	case PROT_T5T:
		return "T5T";
	case PROT_MIFARE:
		return "MIFARE";
	default:
		return "UNKNOWN";
	}
}

static bool looks_like_yubikey(const NxpNci_RfIntf_t *rf)
{
	return rf != NULL &&
	       rf->Protocol == PROT_ISODEP &&
	       rf->ModeTech == (MODE_POLL | TECH_PASSIVE_NFCA) &&
	       rf->Info.NFC_APP.SensRes[0] == 0x44 &&
	       rf->Info.NFC_APP.SensRes[1] == 0x00 &&
	       rf->Info.NFC_APP.NfcIdLen > 0U;
}

static void print_probe_summary(const NxpNci_RfIntf_t *rf)
{
	if (rf == NULL) {
		return;
	}

	printk("PN7160 target: protocol=%s mode_tech=0x%02X more=%u\n",
	       protocol_to_string(rf->Protocol),
	       rf->ModeTech,
	       rf->MoreTags ? 1U : 0U);

	if (rf->ModeTech == (MODE_POLL | TECH_PASSIVE_NFCA)) {
		print_bytes("SENS_RES: ", rf->Info.NFC_APP.SensRes, 2);
		print_bytes("UID: ", rf->Info.NFC_APP.NfcId, rf->Info.NFC_APP.NfcIdLen);
		if (rf->Info.NFC_APP.SelResLen != 0U) {
			print_bytes("SEL_RES: ", rf->Info.NFC_APP.SelRes, rf->Info.NFC_APP.SelResLen);
		}
		if (rf->Info.NFC_APP.RatsLen != 0U) {
			print_bytes("RATS: ", rf->Info.NFC_APP.Rats, rf->Info.NFC_APP.RatsLen);
		}
	}

	if (looks_like_yubikey(rf)) {
		printk("YubiKey candidate detected: uid_len=%u protocol=ISO-DEP atqa=0x%02X%02X\n",
		       rf->Info.NFC_APP.NfcIdLen,
		       rf->Info.NFC_APP.SensRes[0],
		       rf->Info.NFC_APP.SensRes[1]);
	} else {
		printk("Non-YubiKey target detected\n");
	}
}

int main(void)
{
	NxpNci_RfIntf_t rf_interface;

	status_led_init();
	status_led_blink(1, 100);
	printk("PN7160 izouxv Zephyr example on %s\n", CONFIG_BOARD);

	if (NxpNci_Connect() == NFC_ERROR) {
		status_led_blink(10, 50);
		printk("PN7160 connect failed\n");
		return 0;
	}
	status_led_blink(2, 100);

	/*
	 * The izouxv defaults bundle OM27160 demo-kit specific controller settings.
	 * For U575 bring-up we first validate the minimal transport/discovery path.
	 */
	printk("Skipping NxpNci_ConfigureSettings() for minimal U575 bring-up\n");
	status_led_blink(3, 100);

	if (NxpNci_ConfigureMode(NXPNCI_MODE_RW) == NFC_ERROR) {
		status_led_blink(10, 50);
		printk("PN7160 configure mode failed\n");
		return 0;
	}
	status_led_blink(4, 100);

	if (NxpNci_StartDiscovery(DiscoveryTechnologies, sizeof(DiscoveryTechnologies)) == NFC_ERROR) {
		status_led_blink(10, 50);
		printk("PN7160 start discovery failed\n");
		return 0;
	}
	status_led_blink(5, 100);
	status_led_set(true);

	printk("PN7160 initialized, RF discovery started\n");

	while (true) {
		printk("Waiting for PN7160 discovery notification\n");

		while (NxpNci_WaitForDiscoveryNotification(&rf_interface) != NFC_SUCCESS) {
		}

		if ((rf_interface.ModeTech & MODE_MASK) != MODE_POLL) {
			printk("Ignoring non-poll discovery: mode_tech=0x%02X\n", rf_interface.ModeTech);
			continue;
		}

		status_led_blink(6, 60);
		print_probe_summary(&rf_interface);

		NxpNci_ProcessReaderMode(rf_interface, PRESENCE_CHECK);
		printk("Target removed\n");
		status_led_set(true);

		NxpNci_StopDiscovery();
		while (NxpNci_StartDiscovery(DiscoveryTechnologies, sizeof(DiscoveryTechnologies)) != NFC_SUCCESS) {
		}
	}

	return 0;
}
