#!/usr/bin/env bash
# Builds sdcard.img from genimage.cfg (imx8-boot-sd.bin is already produced
# by imx8-bootloader-prepare.sh at this point).
set -e

BOARD_DIR="$(dirname "$0")"
GENIMAGE_TMP="${BUILD_DIR}/genimage.tmp"

"${HOST_DIR}/bin/mkimage" -A arm64 -O linux -T script -C none \
	-n "Debix Model A boot script" \
	-d "${BOARD_DIR}/boot.cmd" "${BINARIES_DIR}/boot.scr"

rm -rf "${GENIMAGE_TMP}"

genimage \
	--rootpath "${TARGET_DIR}" \
	--tmppath "${GENIMAGE_TMP}" \
	--inputpath "${BINARIES_DIR}" \
	--outputpath "${BINARIES_DIR}" \
	--config "${BOARD_DIR}/genimage.cfg"

SWU_VERSION="${SWU_VERSION:-$(date -u +%Y.%m.%d-%H%M)}"
SWU_VARS="${BUILD_DIR}/swu-vars.cfg"

printf 'variables = { SWU_VERSION = "%s"; };\n' "${SWU_VERSION}" > "${SWU_VARS}"

# -n: rootfs.ext4.zst is already compressed; keep compressed = "zstd" as-is.
echo "Generating ${BINARIES_DIR}/debix-model-a.swu (version ${SWU_VERSION})"
"${HOST_DIR}/bin/swugenerator" -n -l DEBUG \
	-s "${BOARD_DIR}/swupdate/sw-description" \
	-c "${SWU_VARS}" \
	-a "${BINARIES_DIR}" \
	-o "${BINARIES_DIR}/debix-model-a.swu" \
	create
echo "Generated ${BINARIES_DIR}/debix-model-a.swu"
