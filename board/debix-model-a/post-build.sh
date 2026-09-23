#!/usr/bin/env bash
# Copies the kernel Image and board dtb into the rootfs /boot so a single
# ext4 partition can hold both the rootfs and the files extlinux.conf points to.
set -e

mkdir -p "${TARGET_DIR}/boot"
cp -f "${BINARIES_DIR}/Image" "${TARGET_DIR}/boot/Image"
cp -f "${BINARIES_DIR}/imx8mp-debix-model-a.dtb" "${TARGET_DIR}/boot/imx8mp-debix-model-a.dtb"
