/*
 * Copyright (c) 2021 Panasonic Industrial Devices Europe GmbH
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <zephyr.h>
#include <bluetooth/services/nus.h>
#include <logging/log.h>
#include <drivers/i2c.h>
#include <stdio.h>

LOG_MODULE_REGISTER(main_app);

/* Bluetooth Handling */

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN	(sizeof(DEVICE_NAME) - 1)

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static const struct bt_data sd[] = {
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_NUS_VAL),
};

static struct bt_conn *current_conn = NULL;

static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		LOG_ERR("Connection failed (err %u)", err);
		return;
	}
	LOG_INF("Connected");
	current_conn = bt_conn_ref(conn);
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	LOG_INF("Disconnected (reason %u)", reason);
	bt_conn_unref(current_conn);
	current_conn = NULL;
}

static struct bt_conn_cb conn_callbacks = {
	.connected = connected,
	.disconnected = disconnected
};

static enum bt_nus_send_status send_status = BT_NUS_SEND_STATUS_DISABLED;

/* This callback is called whenever the send status changes and so it keeps 
track if sending is allowed or not. */
static void bt_send_enabled_cb(enum bt_nus_send_status s)
{
	LOG_INF("bt_nus_send_status %s", s == BT_NUS_SEND_STATUS_ENABLED ? "enabled" : "disabled");
	send_status = s;
}

static struct bt_nus_cb nus_cb = {
	.send_enabled = bt_send_enabled_cb,
};

/* Sensor Handling */

const struct device *i2c_dev;

/* Helper function that implements the i2c access protocol as required by the sensor.
It reads num_bytes bytes from the address addr and stores it to *data */
static int read_bytes(const struct device *dev, uint8_t addr, uint8_t *data, uint32_t num_bytes)
{
	struct i2c_msg msgs[2];

	msgs[0].buf = &addr;
	msgs[0].len = 1U;
	msgs[0].flags = I2C_MSG_WRITE;

	msgs[1].buf = data;
	msgs[1].len = num_bytes;
	msgs[1].flags = I2C_MSG_RESTART | I2C_MSG_READ | I2C_MSG_STOP;

	return i2c_transfer(dev, &msgs[0], 2, 0x33);
}

/* Helper function that reads 4 consecutive bytes from address addr and converts them
to an uint32_t value according to the sensor specification which is then passed back in *value */
static int read_register_4(const struct device *dev, uint8_t addr, uint32_t *value)
{
	uint8_t buf[4];
	int ret;

	ret = read_bytes(dev, addr, &buf[0], 4);
	if (ret)
	{
		LOG_ERR("read_bytes() @ i2c failed");
		return -1;
	}

	*value = (buf[3] << 24) | (buf[2] << 16) | (buf[1] << 8) | buf[0];

	return 0;
}

/* Helper function that reads 2 consecutive bytes from address addr and converts them
to an uint16_t value according to the sensor specification which is then passed back in *value */
static int read_register_2(const struct device *dev, uint8_t addr, uint16_t *value)
{
	uint8_t buf[2];
	int ret;

	ret = read_bytes(dev, addr, &buf[0], 2);
	if (ret)
	{
		LOG_ERR("read_bytes() @ i2c failed");
		return -1;
	}

	*value = (buf[1] << 8) | buf[0];

	return 0;
}

/* Init function that initializes the i2c subsystem according to the needs of the sensor. */
static int sensor_init(void)
{
	int ret;

	i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c0));
	if (i2c_dev == NULL)
	{
		LOG_ERR("device_get_binding() @ i2c failed");
		return -1;
	}

	ret = i2c_configure(i2c_dev, I2C_SPEED_SET(I2C_SPEED_STANDARD) | I2C_MODE_CONTROLLER);
	if (ret)
	{
		LOG_ERR("i2c_configure() failed");
		return -1;
	}

	return 0;
}

/* Helper macro that defines a variable and reads a 32-bit sensor 
variable according to the sensor specification to this variable */
#define DEFINE_AND_MEASURE_4(x, y)                 \
	uint32_t x = 0;                                \
	ret = read_register_4(i2c_dev, y, &x);         \
	if (ret)                                       \
	{                                              \
		LOG_ERR("read_register_4() @ i2c failed"); \
		return -1;                                 \
	}

/* Helper macro that defines a variable and reads a 16-bit sensor 
variable according to the sensor specification to this variable */
#define DEFINE_AND_MEASURE_2(x, y)                 \
	uint16_t x = 0;                                \
	ret = read_register_2(i2c_dev, y, &x);         \
	if (ret)                                       \
	{                                              \
		LOG_ERR("read_register_2() @ i2c failed"); \
		return -1;                                 \
	}
	
/* This buffer is used to buffer the message that is being sent to the remote device.
It must be big enough to hold our string, so CONFIG_BT_NUS_UART_BUFFER_SIZE must be big enough. */
static uint8_t buf[128];

/* This function implements the application logic for the sensor handling. */
static int sensor_process(void)
{
	int ret;

	/* Otherwise the status register is read and checked first. */
	uint8_t status;
	ret = read_bytes(i2c_dev, 0x26, &status, 1);
	if (ret)
	{
		LOG_ERR("read_bytes() @ i2c failed");
		return -1;
	}

	/* Afterwards all relevant sensor values are retrieved. */
	DEFINE_AND_MEASURE_4(pm1_0, 0x00);
	DEFINE_AND_MEASURE_4(pm2_5, 0x04);
	DEFINE_AND_MEASURE_4(pm10, 0x08);
	DEFINE_AND_MEASURE_2(pc0_5, 0x0c);
	DEFINE_AND_MEASURE_2(pc1_0, 0x0e);
	DEFINE_AND_MEASURE_2(pc2_5, 0x10);
	DEFINE_AND_MEASURE_2(pc5_0, 0x14);
	DEFINE_AND_MEASURE_2(pc7_5, 0x16);
	DEFINE_AND_MEASURE_2(pc10_0, 0x18);

	/* All sensor data is written nicely to our output buffer. */
	int pos = snprintf(buf, sizeof(buf), "ST:%x;PM1.0:%d;PM2.5:%d;PM10:%d;PC0.5:%d;PC1.0:%d;PC2.5:%d;PC5.0:%d;PC7.5:%d;P10.0:%d\n", status, pm1_0, pm2_5, pm10, pc0_5, pc1_0, pc2_5, pc5_0, pc7_5, pc10_0);
	if ((pos < 0) || (pos >= sizeof(buf))) {
		LOG_ERR("snprintf returned %d", pos);
		return -ENOMEM;
	}

	LOG_INF("ST:%x;PM1.0:%d;PM2.5:%d;PM10:%d;PC0.5:%d;PC1.0:%d;PC2.5:%d;PC5.0:%d;PC7.5:%d;P10.0:%d", status, pm1_0, pm2_5, pm10, pc0_5, pc1_0, pc2_5, pc5_0, pc7_5, pc10_0);

	/* The message in the output buffer may be larger than the currently configured MTU
	size. In that case the output buffer cannot be send in one go, but must be packetized correctly. */

	/* Retrive the maximum allowed message size from the UART service. */
	uint16_t mtu = bt_nus_get_mtu(current_conn);
	LOG_INF("string length is %d, mtu %d, buffer size %d", pos, mtu, sizeof(buf));

	/* Later on the mtu variable is used to packetize the data, if necessary. The 
	buffer has a certain size, but of course the mtu can actually be bigger. If so, 
	then packetizing needs to be done in chunks of the buffer size maximum. */
	if (mtu > sizeof(buf)) {
		mtu = sizeof(buf);
	}

	/* Loop over our message and packetize and send out data via Bluetooth. */
	int offset = 0;
	int remaining = pos;
	while(offset < pos) {
		int chunk = mtu < remaining ? mtu : remaining;
		LOG_INF("offset %d, pos %d, chunk %d", offset, pos, chunk);	
		ret = bt_nus_send(current_conn, &buf[offset], chunk);
		if(ret) {
			LOG_ERR("Failed to send data (err: %d)", ret);
			return -1;
		}
		offset += chunk;
		remaining -= chunk;
	}
 
	return 0;
}

void main(void)
{
	int err = 0;

	bt_conn_cb_register(&conn_callbacks);

	err = sensor_init();
	if (err)
	{
		LOG_ERR("Failed to enable sensor (err: %d)", err);
		return;
	}

	err = bt_enable(NULL);
	if (err)
	{
		LOG_ERR("Failed to enable Bluetooth (err: %d)", err);
		return;
	}

	err = bt_nus_init(&nus_cb);
	if (err)
	{
		LOG_ERR("Failed to initialize UART service (err: %d)", err);
		return;
	}

	err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
	if (err)
	{
		LOG_ERR("Advertising failed to start (err %d)", err);
		return;
	}

	while(1)
	{
		k_sleep(K_MSEC(1000));
		if (current_conn != NULL && send_status == BT_NUS_SEND_STATUS_ENABLED) {
			sensor_process();
		}
	}
}
