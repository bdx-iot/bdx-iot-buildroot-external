/* SPDX-License-Identifier: BSD-2-Clause */
#ifndef USER_TA_HEADER_DEFINES_H
#define USER_TA_HEADER_DEFINES_H

#include <i2c_test_ta.h>

#define TA_UUID			TA_I2C_TEST_UUID
#define TA_FLAGS		TA_FLAG_SINGLE_INSTANCE
#define TA_STACK_SIZE		(2 * 1024)
#define TA_DATA_SIZE		(16 * 1024)
#define TA_VERSION		"1.0"
#define TA_DESCRIPTION		"Test TA for the OP-TEE I2C device drivers"

#endif /* USER_TA_HEADER_DEFINES_H */
