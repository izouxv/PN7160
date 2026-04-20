#include "tml.h"

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>

#define PN7160_DT_NODE DT_PATH(zephyr_user)
#define PN7160_IRQ_STABLE_SAMPLES 3U
#define PN7160_IRQ_STABLE_DELAY_US 100U

static bool isDwlMode = false;

#define HEADER_SZ (isDwlMode ? 1 : 2)
#define FOOTER_SZ (isDwlMode ? 2 : 0)

static const struct device *s_i2c_bus = DEVICE_DT_GET(DT_ALIAS(pn7160_i2c));
static const struct gpio_dt_spec s_irq_gpio =
	GPIO_DT_SPEC_GET(PN7160_DT_NODE, pn7160_irq_gpios);
static const struct gpio_dt_spec s_en_gpio =
	GPIO_DT_SPEC_GET(PN7160_DT_NODE, pn7160_en_gpios);
static const struct gpio_dt_spec s_dwl_gpio =
	GPIO_DT_SPEC_GET_OR(PN7160_DT_NODE, pn7160_dwl_gpios, ((struct gpio_dt_spec){0}));

static struct gpio_callback s_irq_callback;
static struct k_sem s_irq_sem;
static struct k_mutex s_i2c_mutex;
static bool s_initialized;

static bool pn7160_dwl_available(void)
{
	return s_dwl_gpio.port != NULL && device_is_ready(s_dwl_gpio.port);
}

static bool pn7160_irq_is_stably_high(void)
{
	for (uint32_t sample = 0; sample < PN7160_IRQ_STABLE_SAMPLES; ++sample) {
		if (gpio_pin_get_dt(&s_irq_gpio) <= 0) {
			return false;
		}
		if (sample + 1U < PN7160_IRQ_STABLE_SAMPLES) {
			k_busy_wait(PN7160_IRQ_STABLE_DELAY_US);
		}
	}

	return true;
}

static void pn7160_irq_callback_handler(
	const struct device *port,
	struct gpio_callback *cb,
	uint32_t pins)
{
	ARG_UNUSED(port);
	ARG_UNUSED(cb);
	ARG_UNUSED(pins);

	k_sem_give(&s_irq_sem);
}

static int pn7160_wait_for_irq(uint32_t timeout_ms)
{
	if (pn7160_irq_is_stably_high()) {
		return 0;
	}

	if (timeout_ms == TIMEOUT_INFINITE) {
		while (true) {
			k_sem_take(&s_irq_sem, K_FOREVER);
			if (pn7160_irq_is_stably_high()) {
				return 0;
			}
		}
	}

	int rc = k_sem_take(&s_irq_sem, K_MSEC(timeout_ms));
	if (rc != 0) {
		return rc;
	}

	return pn7160_irq_is_stably_high() ? 0 : -EAGAIN;
}

void INTF_INIT(void)
{
	int rc;

	if (s_initialized) {
		return;
	}

	if (!device_is_ready(s_i2c_bus) ||
	    !device_is_ready(s_irq_gpio.port) ||
	    !device_is_ready(s_en_gpio.port)) {
		printk("PN7160 Zephyr transport not ready\n");
		return;
	}

	k_sem_init(&s_irq_sem, 0, 1);
	k_mutex_init(&s_i2c_mutex);

	rc = gpio_pin_configure_dt(&s_en_gpio, GPIO_OUTPUT_ACTIVE);
	if (rc < 0) {
		printk("PN7160 EN gpio config failed: %d\n", rc);
		return;
	}

	if (pn7160_dwl_available()) {
		rc = gpio_pin_configure_dt(&s_dwl_gpio, GPIO_OUTPUT_INACTIVE);
		if (rc < 0) {
			printk("PN7160 DWL gpio config failed: %d\n", rc);
			return;
		}
	}

	rc = gpio_pin_configure_dt(&s_irq_gpio, GPIO_INPUT);
	if (rc < 0) {
		printk("PN7160 IRQ gpio config failed: %d\n", rc);
		return;
	}

	gpio_init_callback(&s_irq_callback, pn7160_irq_callback_handler, BIT(s_irq_gpio.pin));
	rc = gpio_add_callback(s_irq_gpio.port, &s_irq_callback);
	if (rc < 0) {
		printk("PN7160 IRQ callback add failed: %d\n", rc);
		return;
	}

	rc = gpio_pin_interrupt_configure_dt(&s_irq_gpio, GPIO_INT_EDGE_TO_ACTIVE);
	if (rc < 0) {
		printk("PN7160 IRQ interrupt config failed: %d\n", rc);
		return;
	}

	s_initialized = true;
}

static int32_t INTF_WRITE(uint8_t *pBuff, uint16_t buffLen)
{
	if (!s_initialized || pBuff == NULL || buffLen == 0U) {
		return -EINVAL;
	}

	return i2c_write(s_i2c_bus, pBuff, buffLen, PN7160_I2C_ADDRESS);
}

static int32_t INTF_READ(uint8_t *pBuff, uint16_t buffLen)
{
	if (!s_initialized || pBuff == NULL || buffLen == 0U) {
		return -EINVAL;
	}

	return i2c_read(s_i2c_bus, pBuff, buffLen, PN7160_I2C_ADDRESS);
}

Status tml_Init(void)
{
	INTF_INIT();
	return s_initialized ? TML_SUCCESS : TML_ERROR;
}

Status tml_DeInit(void)
{
	if (!s_initialized) {
		return TML_SUCCESS;
	}

	gpio_pin_set_dt(&s_en_gpio, 0);
	return TML_SUCCESS;
}

Status tml_Reset(void)
{
	if (!s_initialized) {
		return TML_ERROR;
	}

	gpio_pin_set_dt(&s_en_gpio, 0);
	k_sleep(K_MSEC(10));
	gpio_pin_set_dt(&s_en_gpio, 1);
	k_sleep(K_MSEC(10));
	return TML_SUCCESS;
}

Status tml_Tx(uint8_t *pBuff, uint16_t buffLen)
{
	int32_t rc;

	k_mutex_lock(&s_i2c_mutex, K_FOREVER);
	rc = INTF_WRITE(pBuff, buffLen);
	if (rc != 0) {
		k_sleep(K_MSEC(10));
		rc = INTF_WRITE(pBuff, buffLen);
	}
	k_mutex_unlock(&s_i2c_mutex);

	return (rc == 0) ? TML_SUCCESS : TML_ERROR;
}

Status tml_Rx(uint8_t *pBuff, uint16_t buffLen, uint16_t *pBytesRead)
{
	int32_t rc;
	uint16_t frameLength;

	if (pBytesRead == NULL) {
		return TML_ERROR;
	}

	*pBytesRead = 0;

	k_mutex_lock(&s_i2c_mutex, K_FOREVER);

	if (!pn7160_irq_is_stably_high()) {
		k_mutex_unlock(&s_i2c_mutex);
		return TML_ERROR;
	}

	rc = INTF_READ(pBuff, HEADER_SZ + 1U);
	if (rc != 0) {
		k_mutex_unlock(&s_i2c_mutex);
		return TML_ERROR;
	}

	frameLength = (uint16_t)(pBuff[HEADER_SZ] + HEADER_SZ + 1U);
	if (frameLength > buffLen) {
		k_mutex_unlock(&s_i2c_mutex);
		return TML_ERROR;
	}

	if (pBuff[HEADER_SZ] > 0U) {
		rc = INTF_READ(&pBuff[HEADER_SZ + 1U], pBuff[HEADER_SZ] + FOOTER_SZ);
		if (rc != 0) {
			k_mutex_unlock(&s_i2c_mutex);
			return TML_ERROR;
		}
	}

	k_mutex_unlock(&s_i2c_mutex);
	*pBytesRead = frameLength;
	return TML_SUCCESS;
}

Status tml_WaitForRx(uint32_t timeout)
{
	return (pn7160_wait_for_irq(timeout) == 0) ? TML_SUCCESS : TML_ERROR;
}

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
	if (pn7160_dwl_available()) {
		gpio_pin_set_dt(&s_dwl_gpio, 1);
	}
	tml_Reset();
}

void tml_LeaveDwlMode(void)
{
	isDwlMode = false;
	if (pn7160_dwl_available()) {
		gpio_pin_set_dt(&s_dwl_gpio, 0);
	}
	tml_Reset();
}

void tml_Send(uint8_t *pBuffer, uint16_t BufferLen, uint16_t *pBytesSent)
{
	if (pBytesSent == NULL) {
		return;
	}

	*pBytesSent = (tml_Tx(pBuffer, BufferLen) == TML_SUCCESS) ? BufferLen : 0U;
}

void tml_Receive(uint8_t *pBuffer, uint16_t BufferLen, uint16_t *pBytes, uint16_t timeout)
{
	if (pBytes == NULL) {
		return;
	}

	if (tml_WaitForRx(timeout) == TML_ERROR) {
		*pBytes = 0;
		return;
	}

	if (tml_Rx(pBuffer, BufferLen, pBytes) == TML_ERROR) {
		*pBytes = 0;
	}
}
