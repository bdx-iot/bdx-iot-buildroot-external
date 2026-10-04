# WaRP7 (i.MX7S) board support

Defconfig: `configs/warp7_defconfig` (Buildroot 2026.05.3).

```
make BR2_EXTERNAL=../bdx-iot-external warp7_defconfig
make
```

## Boot chain

```
BootROM -> TF-A BL2 (bl2.bin.imx) -> OP-TEE (BL32) -> U-Boot (BL33) -> Linux
           eMMC boot0/boot1 @ 1K    \- fip.bin, user area @ 1M -/      rootfs A/B
```

| Stage  | Version / config                                    | Runs in    |
|--------|-----------------------------------------------------|------------|
| TF-A   | v2.12, `PLAT=warp7`, BL2 at EL3 (`RESET_TO_BL2`)    | secure     |
| OP-TEE | 4.9.0, `PLATFORM=imx-mx7swarp7_mbl`                 | secure     |
| U-Boot | 2025.04, `warp7_bl33_defconfig` + `u-boot/uboot.fragment` | non-secure |
| Linux  | 6.12.24, `imx_v6_v7_defconfig` + fragments          | non-secure |

Reference: <https://trustedfirmware-a.readthedocs.io/en/stable/plat/warp7.html>.
Differences from that page: U-Boot is upstream (no Linaro MBL branch), OP-TEE
uses the `mx7swarp7_mbl` flavour (TF-A v2.12 BL2 no longer loads a DTB into the
FIP), and Trusted Board Boot is not enabled.

BL2 is wrapped with the i.MX BootROM header by `post-image.sh`, using the DCD
(DDR init) from U-Boot's `u-boot.cfgout`:

```
mkimage -n ${UBOOT_DIR}/u-boot.cfgout -T imximage -e 0x9df00000 -d bl2.bin bl2.bin.imx
```

## Memory map (512 MiB DDR)

| Address                 | Content                                         |
|-------------------------|-------------------------------------------------|
| 0x80800000              | `zImage` (`loadaddr`)                           |
| 0x83000000              | Linux dtb (`fdt_addr`)                          |
| 0x83100000              | OP-TEE DT overlay (`CFG_DT_ADDR`, `_mbl` flavour) |
| 0x87800000              | U-Boot (BL33 entry, `CONFIG_TEXT_BASE`)         |
| 0x9df00000 - 0x9e000000 | TF-A BL2 (only during boot)                     |
| 0x9e000000 - 0xa0000000 | OP-TEE TZDRAM + shared memory (secure)          |

U-Boot is told about the OP-TEE region with `CONFIG_OPTEE_TZDRAM_SIZE`, and
Linux with a `reserved-memory` node in `linux/dts/nxp/imx/imx7s-warp-optee-m4.dts`.

## eMMC layout

U-Boot: `mmc 0`. Linux: `/dev/mmcblk2` (usdhc3, `mmc2` alias).

BL2 (`bl2.bin.imx`, TF-A BL2 + i.MX header) is A/B in the eMMC **boot
partitions**, at 1 KiB of boot0 and boot1 (`/dev/mmcblk2boot0`/`boot1`). The
BootROM boots from the one selected by `mmc partconf 0 1 <1|2> 0` (EXT_CSD
PARTITION_CONFIG). BL2 then always reads the FIP from the user area:

| Offset / partition | Content (user area)                            |
|--------------------|------------------------------------------------|
| 512 KiB / 640 KiB  | U-Boot environment (redundant, 8 KiB each)     |
| 1 MiB (max 1 MiB)  | `fip.bin` (OP-TEE + U-Boot), read by BL2       |
| p1 @ 8 MiB, 32 MiB | VFAT: `boot.scr`                               |
| p2, 1 GiB          | rootfs A (`/boot/zImage`, `/boot/imx7s-warp.dtb`) |
| p3, 1 GiB          | rootfs B                                       |
| p4                 | `/data`, grown to the end of the disk on first boot |

`genimage.cfg` builds this as `output/images/sdcard.img`. The FIP partition is
limited to 1 MiB (`IMX_FIP_SIZE` in TF-A): genimage fails if `fip.bin` is
bigger. Current size: ~0.9 MiB (U-Boot ~510 KiB + OP-TEE ~400 KiB).

## Files

| File | Purpose |
|------|---------|
| `genimage.cfg` | eMMC image layout above |
| `post-build.sh` | copies `zImage` and the dtb (`imx7s-warp-optee-m4.dtb`, installed as `/boot/imx7s-warp.dtb`) into `/boot` of the rootfs |
| `post-image.sh` | `bl2.bin.imx`, `sdcard.img`, `warp7-<version>.swu` |
| `linux/dts/nxp/imx/imx7s-warp-optee-m4.dts` | `#include`s the in-tree `imx7s-warp.dts` and adds the OP-TEE nodes and the IO board devices (MCP23008, RTC, EEPROM, LM75A, PCF8591 on i2c3, owned by OP-TEE; MCP2515 CAN); disables Wi-Fi (usdhc1) |
| `linux/optee.fragment` | `CONFIG_TEE`, `CONFIG_OPTEE` |
| `linux/sensors.fragment` | sensors, MCP23S08, MCP251x, DS1307 |
| `u-boot/uboot.fragment` | merged into `warp7_bl33_defconfig`, see below |
| `u-boot/imx7s-warp-extra.dtsi` | U-Boot DT nodes, appended with `CONFIG_DEVICE_TREE_INCLUDES` |
| `u-boot/patches/0001-*` | adds the SWUpdate A/B and bootcount variables to `warp7.h` |
| `u-boot/boot.cmd` | `boot.scr`: loads kernel + dtb from `/boot` of the active rootfs |
| `swupdate/sw-description` | A/B update: rootfs, `boot.scr`, `fip.bin`, A/B BL2 in the boot partitions |
| `rootfs_overlay/` | `fstab`, `fw_env.config`, `hwrevision`, growpart, SWUpdate args, `can0` |
| `rootfs_overlay/etc/modules-load.d/g_ether.conf`, `etc/modprobe.d/g_ether.conf` | load the USB Ethernet gadget at boot with fixed MACs (WaRP7 `usb0` = 02:00:00:00:77:01, host interface `enx020000007702`) |
| `rootfs_overlay/etc/systemd/network/10-usb0.network` | `usb0` = 10.0.0.1/24 with a DHCP server for the host |
| `rootfs_overlay/usr/lib/firmware/brcm/brcmfmac43430-sdio.element14,imx7s-warp.txt` | NVRAM (RF calibration) for the BCM43430 WiFi; brcmfmac looks for `<chip>.<dt compatible>.txt` first. The firmware `.bin`/`.clm_blob` come from linux-firmware |

Shared with the Debix in `board/common/`: rootfs overlay (applied first),
busybox/WiFi/Bluetooth fragments, SWUpdate config and source, tee-supplicant,
and `BR2_GLOBAL_PATCH_DIR` (`patches/optee-os/0001-*`: enables
`MBEDTLS_PKCS1_V21` (RSA OAEP/PSS) in the mbedTLS library linked into user
TAs; mbedTLS for TAs is on by default, `CFG_TA_MBEDTLS=y`).

## U-Boot as BL33: required settings

`warp7_bl33_defconfig` does not boot as-is behind TF-A v2.12 + OP-TEE 4.9.
`u-boot/uboot.fragment` fixes it:

| Option | Why |
|--------|-----|
| `CONFIG_SKIP_LOWLEVEL_INIT=y` | otherwise `arch_cpu_init()` writes AIPS-TZ/CSU registers, which aborts in the normal world before the console (seen as a silent reset loop) |
| `# CONFIG_ARMV7_NONSEC is not set` | U-Boot is already non-secure; OP-TEE provides PSCI |
| `CONFIG_CONS_INDEX=0` | `arch-mx7/clock.h` picks the UART clock root from this 0-based index; the default `1` reads UART2, gated off by TF-A, giving 0 Hz and a garbled console |
| `CONFIG_OPTEE_LIB=y`, `CONFIG_OPTEE_TZDRAM_SIZE=0x2000000` | `dram_init()` subtracts the OP-TEE region so U-Boot does not relocate into secure memory |
| `CONFIG_SPI=y`, `CONFIG_DM_SPI=y` | `mcp230xx_gpio.c` calls `dm_spi_*()` unconditionally (link error otherwise, still the case upstream) |
| `CONFIG_WDT`, `CONFIG_IMX_WATCHDOG`, `CONFIG_SYSRESET`, `CONFIG_SYSRESET_WATCHDOG` | `reset` through WDOG1 (`wdt-reboot` node of `imx7s-warp-u-boot.dtsi`); without them `reset_cpu()` is an empty stub and `reset` returns to the prompt. U-Boot also starts WDOG1 (128 s timeout): Linux must keep it fed, which systemd does (`board/common/.../10-watchdog.conf`) |
| `CONFIG_LEGACY_IMAGE_FORMAT=y` | `boot.scr` is a legacy `mkimage -T script` image; `CONFIG_FIT_SIGNATURE` (in `warp7_bl33_defconfig`) disables legacy images by default, and `source` then fails with `Wrong image format` |
| `CONFIG_SYS_REDUNDAND_ENVIRONMENT`, `CONFIG_ENV_OFFSET_REDUND=0xA0000` | redundant environment at 512K/640K |
| `CONFIG_BOOTCOUNT_LIMIT`, `CONFIG_BOOTCOUNT_ENV` | A/B fallback for SWUpdate |
| `CONFIG_DEVICE_TREE_INCLUDES`, GPIO hog, MCP230xx, LED, RTC, `CMD_I2C`/`CMD_RTC` | board nodes from `imx7s-warp-extra.dtsi` |
| `CONFIG_DEBUG_UART*`, `CONFIG_PANIC_HANG` | **bring-up only**: early prints (`<debug_uart>`) and halt on panic; remove once U-Boot boots |

## Rebuilding after a change

Buildroot does not rebuild TF-A when U-Boot or OP-TEE changes, and the FIP
embeds both:

```
make uboot-dirclean arm-trusted-firmware-rebuild all          # U-Boot change
make optee-os-rebuild arm-trusted-firmware-rebuild all        # OP-TEE change
make arm-trusted-firmware-rebuild all                         # TF-A change
```

`uboot-dirclean` is needed after editing `uboot.fragment`, a patch or the
dtsi. `all` reruns `post-image.sh` (new `bl2.bin.imx`, `sdcard.img`, `.swu`).

## Flashing

`sdcard.img` goes to the eMMC user area; BL2 goes to a boot partition.

1. On the WaRP7 U-Boot console, export the eMMC user area over USB:
   ```
   => mmc dev 0 0
   => ums 0 mmc 0
   ```
2. On the host (`lsblk` to find the device, here `/dev/sdX`):
   ```
   sudo dd if=output/images/sdcard.img of=/dev/sdX bs=1M conv=fsync status=progress
   sync
   ```
3. Back on the console, Ctrl-C to stop UMS, write BL2 (copied on the VFAT
   partition) into boot0, check it, then boot from boot0:
   ```
   => load mmc 0:1 ${loadaddr} bl2.bin.imx
   => setexpr cnt ${filesize} + 1ff; setexpr cnt ${cnt} / 200
   => mmc dev 0 1
   => mmc write ${loadaddr} 2 ${cnt}
   => mmc read 0x84000000 2 ${cnt}
   => cmp.b ${loadaddr} 0x84000000 ${filesize}
   => mmc dev 0 0
   => mmc partconf 0 1 1 0
   => reset
   ```
   `mmc dev 0 <hwpart>`: `0` = user area, `1` = boot0, `2` = boot1.
   `mmc partconf <dev> <boot_ack> <boot_partition> <access>`: boot partition
   `1` = boot0, `2` = boot1. Always run it **last**, after `mmc dev 0 0`: its
   `access` argument switches the partition the eMMC reads and writes
   without U-Boot knowing, so a `mmc write` after it lands in the user area.
4. On the new U-Boot, load the default environment once, then **reset**:
   ```
   => env default -a
   => saveenv
   => reset
   ```
   Do not `run bootcmd` right after `env default -a`: it also deletes
   `hab_enabled`, which `board_late_init()` only sets at boot. With it empty,
   `test ${hab_enabled} -eq 1` sends the bootcmd into the HAB path, which
   renames `script` to `boot.scr.imx-signed` and fails to find it.

   The environment is redundant (0x80000 and 0xA0000). Starting from an
   invalid environment, the first two `saveenv` both print
   `Writing to MMC(0)`; the copies then alternate and every other save prints
   `Writing to redundant MMC(0)`. Check both copies from Linux with
   `fw_printenv`.

Expected console: `NOTICE: BL2: v2.12...`, the OP-TEE banner, then U-Boot.

## Updating U-Boot / OP-TEE (fip.bin) or TF-A (bl2.bin.imx)

After the rebuild above, write only what changed. U-Boot `mmc write` takes
hexadecimal block numbers (512 bytes): `2` = 1 KiB, `800` = 1 MiB.

**With SWUpdate** (preferred): `warp7-<version>.swu` writes the inactive
rootfs, `boot.scr`, `fip.bin`, and BL2 into the boot partition the BootROM
does not use, then switches to it (`emmc_boot_toggle`) once everything is
installed. A power loss while BL2 is written leaves the previous BL2 booting.
`fip.bin` has a single copy in the user area: a power loss while it is
written leaves the board unbootable (see Recovery).

**From the U-Boot console**, with the file copied to the VFAT partition (p1):
```
=> load mmc 0:1 ${loadaddr} fip.bin
=> setexpr cnt ${filesize} + 1ff; setexpr cnt ${cnt} / 200
=> mmc dev 0 0
=> mmc write ${loadaddr} 800 ${cnt}
```
For BL2, write `bl2.bin.imx` at block `2` of the boot partition not in use,
as in Flashing step 3 (`mmc dev 0 2` and `mmc partconf 0 1 2 0` for boot1).

**From Linux on the board** (`swupdate -E /dev/mmcblk2` prints the boot
partition in use: `0` = boot0, `1` = boot1):
```
dd if=fip.bin of=/dev/mmcblk2 bs=1k seek=1024 conv=fsync
echo 0 > /sys/block/mmcblk2boot1/force_ro      # boot1 if boot0 is in use
dd if=bl2.bin.imx of=/dev/mmcblk2boot1 bs=1k seek=1 conv=fsync
echo 1 > /sys/block/mmcblk2boot1/force_ro
```
then switch with `mmc partconf 0 1 2 0` in U-Boot (or `mmc bootpart enable 2
1 /dev/mmcblk2` with mmc-utils).

If the default environment changed (new variables in the U-Boot patch), run
`env default -a; saveenv; reset` after the update: a saved environment wins over the
built-in defaults.

## A/B boot and SWUpdate

- `rootpart` (2 or 3) selects the rootfs; `boot.scr` loads
  `/boot/zImage` and `/boot/imx7s-warp.dtb` from it and boots with
  `root=/dev/mmcblk2p${rootpart}`.
- SWUpdate sets `rootpart`, `upgrade_available=1`, `bootcount=0`. If Linux
  does not reach `01-commit-upgrade` within `bootlimit` (1) reboot,
  `altbootcmd` switches back to the other rootfs.
- `09-swupdate-args` selects `stable,rootfsA` or `stable,rootfsB` from the
  running root, and links `/dev/mmcblk2boot-standby` to the boot partition
  not in use (`swupdate -E`). `sw-description` writes BL2 there, then
  `emmc_boot_toggle` switches to it after a successful install. Both are
  decided when SWUpdate starts: reboot between two updates. Hardware
  compatibility is `imx7s-warp:1.0` (`/etc/hwrevision`).
- `fw_env.config`: `/dev/mmcblk2` at 0x80000 and 0xA0000, 0x2000 bytes each.

## Recovery

- **Broken BL2** (new BL2 does not start): switch back to the other boot
  partition from a U-Boot (below) with `mmc partconf 0 1 <1|2> 0`.
- **Broken FIP**: BL2 always reads it from the user area, so it cannot be
  fixed from the board itself.

Keep a U-Boot built from upstream `warp7_defconfig` (without TF-A) and `uuu`
ready:

1. Set the boot switches to USB serial download, then
   `uuu SDP: boot -f u-boot.imx`.
2. On that U-Boot: rewrite the images as in Flashing, or select the other
   boot partition.
3. Set the boot switches back to eMMC boot.

## Known messages

- `E/TC:0 0 __plat_rng_init:192 Warning: seeding RNG with zeroes`: OP-TEE has
  no hardware entropy source in this configuration. Fine for bring-up, not for
  production keys.
- `<debug_uart>`: printed by the bring-up `CONFIG_DEBUG_UART_ANNOUNCE`.
