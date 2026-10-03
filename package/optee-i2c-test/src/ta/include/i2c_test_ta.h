/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Test TA for the OP-TEE I2C device drivers.
 *
 * Command IDs and parameters are those of the i2c_devices PTA
 * (pta_i2c_devices.h): the TA forwards each request to the PTA. This TA
 * gives any normal world client access to the secure I2C devices and must
 * only be installed on development images.
 */
#ifndef I2C_TEST_TA_H
#define I2C_TEST_TA_H

#define TA_I2C_TEST_UUID { 0x9c73886c, 0xb10f, 0x4b83, \
	{ 0x9a, 0xd9, 0x4a, 0xff, 0x65, 0xdc, 0x09, 0xd1 } }

/* Largest memref forwarded to the PTA (EEPROM read/write) */
#define TA_I2C_TEST_MAX_BUF	4096

#endif /* I2C_TEST_TA_H */
