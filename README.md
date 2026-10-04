# bdx-iot-external

Buildroot external tree for two boards with OP-TEE, A/B updates (SWUpdate)
and secure peripherals.

| Board | SoC | Defconfig | Details |
|-------|-----|-----------|---------|
| Polyhex Debix Model A | i.MX8M Plus | `imx8mp_debix_model_a_defconfig` | [board/debix-model-a](board/debix-model-a/README.md) |
| Element14 WaRP7 | i.MX7S | `warp7_defconfig` | [board/warp7](board/warp7/README.md) |

## Build

```
git clone https://github.com/bdx-iot/bdx-iot-external.git
git clone https://git.busybox.net/buildroot
cd buildroot
make BR2_EXTERNAL=../bdx-iot-external O=../output/debix imx8mp_debix_model_a_defconfig
make O=../output/debix
```

Use `warp7_defconfig` for the WaRP7. The output's `images/` directory holds
`sdcard.img`, written to the board as described in its README, and the
`.swu` update.

Set a root password (`make menuconfig`, **System configuration**) before
building: it is not stored in the defconfigs, and SSH refuses empty
passwords.

Host dependencies (Ubuntu 22.04):

```
sudo apt-get install build-essential bison flex gettext libncurses5-dev \
    texinfo autoconf automake libtool git gperf gawk expat curl unzip bc \
    python3-dev wget cpio rsync xxd
```

## Shared files: `board/common`

| Path | Content |
|------|---------|
| `rootfs_overlay/` | applied before each board's overlay (a board file wins): SSH server configuration, systemd watchdog (`RuntimeWatchdogSec=120s`), `tee-supplicant` service, DLT configuration |
| `linux/`, `busybox.fragment` | Wi-Fi and Bluetooth kernel fragments, BusyBox fragment |
| `patches/` | package patches (`BR2_GLOBAL_PATCH_DIR`), e.g. OP-TEE's for the Debix |
| `swupdate/` | SWUpdate configuration; `source` is a git submodule, built through `local.mk` (`BR2_PACKAGE_OVERRIDE_FILE`) |

## Packages

| Package | Content |
|---------|---------|
| `dlt-daemon` | COVESA DLT logging daemon |
| `optee-i2c-test` | test TA and client for the WaRP7 OP-TEE I2C drivers (development only) |
