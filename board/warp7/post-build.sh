#!/usr/bin/env bash
# Copies the kernel zImage and board dtb into the rootfs /boot so each A/B
# rootfs partition carries the kernel that matches it (see boot.cmd).
set -e

mkdir -p "${TARGET_DIR}/boot"
cp -f "${BINARIES_DIR}/zImage" "${TARGET_DIR}/boot/zImage"
# Kept as /boot/imx7s-warp.dtb: boot.scr, shared by both slots, loads that name
cp -f "${BINARIES_DIR}/imx7s-warp-optee-m4.dtb" "${TARGET_DIR}/boot/imx7s-warp.dtb"
