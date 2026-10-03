################################################################################
#
# optee-i2c-test
#
################################################################################

OPTEE_I2C_TEST_VERSION = 1.0
OPTEE_I2C_TEST_SITE = $(BR2_EXTERNAL_BDXIOT_EXTERNAL_PATH)/package/optee-i2c-test/src
OPTEE_I2C_TEST_SITE_METHOD = local
OPTEE_I2C_TEST_LICENSE = BSD-2-Clause
OPTEE_I2C_TEST_DEPENDENCIES = optee-client optee-os

OPTEE_I2C_TEST_TA_UUID = 9c73886c-b10f-4b83-9ad9-4aff65dc09d1

define OPTEE_I2C_TEST_BUILD_CMDS
	$(TARGET_CONFIGURE_OPTS) $(MAKE) CROSS_COMPILE=$(TARGET_CROSS) \
		TA_DEV_KIT_DIR=$(OPTEE_OS_SDK) O=out -C $(@D)/ta all
	$(TARGET_CC) $(TARGET_CFLAGS) -Wall -Wextra \
		-I$(@D)/ta/include -I$(OPTEE_OS_SDK)/host_include \
		-o $(@D)/optee-i2c-test $(@D)/host/main.c \
		$(TARGET_LDFLAGS) -lteec
endef

define OPTEE_I2C_TEST_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/optee-i2c-test \
		$(TARGET_DIR)/usr/bin/optee-i2c-test
	$(INSTALL) -D -m 0444 $(@D)/ta/out/$(OPTEE_I2C_TEST_TA_UUID).ta \
		$(TARGET_DIR)/lib/optee_armtz/$(OPTEE_I2C_TEST_TA_UUID).ta
endef

$(eval $(generic-package))
