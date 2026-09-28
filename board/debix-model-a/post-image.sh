#!/usr/bin/env bash
# Builds sdcard.img from genimage.cfg (imx8-boot-sd.bin is already produced
# by imx8-bootloader-prepare.sh at this point).
set -e

BOARD_DIR="$(dirname "$0")"
GENIMAGE_TMP="${BUILD_DIR}/genimage.tmp"


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
SWU_IMAGE="${BINARIES_DIR}/debix-model-a-${SWU_VERSION}.swu"

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
