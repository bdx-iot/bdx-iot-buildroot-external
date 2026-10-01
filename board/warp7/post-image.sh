#!/usr/bin/env bash
# Wraps TF-A BL2 with the i.MX BootROM header, then builds sdcard.img from
# genimage.cfg and the SWUpdate image.
# $2 is ${UBOOT_DIR} (BR2_ROOTFS_POST_SCRIPT_ARGS): u-boot.cfgout there holds
# the DCD (DDR init) generated from board/warp7/imximage.cfg.
set -e

BOARD_DIR="$(dirname "$0")"
UBOOT_DIR="$2"
GENIMAGE_TMP="${BUILD_DIR}/genimage.tmp"

# BL2 is linked at BL2_RAM_BASE (0x9df00000), just below OP-TEE.
"${HOST_DIR}/bin/mkimage" \
	-n "${UBOOT_DIR}/u-boot.cfgout" \
	-T imximage \
	-e 0x9df00000 \
	-d "${BINARIES_DIR}/bl2.bin" \
	"${BINARIES_DIR}/bl2.bin.imx"

rm -rf "${GENIMAGE_TMP}"
rm -rf "${BINARIES_DIR}"/*.swu

genimage \
	--rootpath "${TARGET_DIR}" \
	--tmppath "${GENIMAGE_TMP}" \
	--inputpath "${BINARIES_DIR}" \
	--outputpath "${BINARIES_DIR}" \
	--config "${BOARD_DIR}/genimage.cfg"

SWU_VERSION="${SWU_VERSION:-$(date -u +%Y.%m.%d-%H%M)}"
SWU_VARS="${BUILD_DIR}/swu-vars.cfg"
SWU_IMAGE="${BINARIES_DIR}/warp7-${SWU_VERSION}.swu"

printf 'variables = { SWU_VERSION = "%s"; };\n' "${SWU_VERSION}" > "${SWU_VARS}"

# -n: rootfs.ext4.zst is already compressed; keep compressed = "zstd" as-is.
echo "Generating ${SWU_IMAGE} (version ${SWU_VERSION})"
"${HOST_DIR}/bin/swugenerator" -n -l DEBUG \
	-s "${BOARD_DIR}/swupdate/sw-description" \
	-c "${SWU_VARS}" \
	-a "${BINARIES_DIR}" \
	-o "${SWU_IMAGE}" \
	create
echo "Generated ${SWU_IMAGE}"
