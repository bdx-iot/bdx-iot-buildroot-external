#!/usr/bin/env bash
# imx8-bootloader-prepare.sh embeds ${BINARIES_DIR}/tee.bin as BL32 and BL31's
# opteed dispatcher jumps straight to its first byte. optee-os's "tee.bin"
# target has a 28-byte non-executable "OPTE" header prefix; "tee-raw.bin" is
# the actual raw code the loader needs. Must run before imx8-bootloader-prepare.sh.
set -e
cp -f "${BINARIES_DIR}/tee-raw.bin" "${BINARIES_DIR}/tee.bin"
