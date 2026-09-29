# Upstream mkimage_fit_atf.sh (imx-mkimage) never sets the FIT "os" property
# on the tee-1 image node, so U-Boot SPL's spl_fit_images_find(IH_OS_TEE)
# never finds it and OP-TEE (BL32) is silently skipped at boot.
define BDX_IOT_EXTERNAL_IMX_MKIMAGE_FIX_TEE_OS
	find $(@D) -name mkimage_fit_atf.sh -exec \
		$(SED) '/description = "TEE firmware";/a\                        os = "tee";' {} +
endef
HOST_IMX_MKIMAGE_POST_PATCH_HOOKS += BDX_IOT_EXTERNAL_IMX_MKIMAGE_FIX_TEE_OS

include $(sort $(wildcard $(BR2_EXTERNAL_BDXIOT_EXTERNAL_PATH)/package/*/*.mk))
