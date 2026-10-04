# Debix Model A (i.MX8M Plus) board support

Defconfig: `configs/imx8mp_debix_model_a_defconfig`

## Boot chain

SPL → TF-A BL31 (`SPD=opteed`) → OP-TEE (BL32) → U-Boot (BL33) → Linux, all
in one FIT image, `imx8-boot-sd.bin`, built by NXP's `imx-mkimage`.

SPL finds OP-TEE through the `os = "tee"` property of its FIT node, which
upstream `mkimage_fit_atf.sh` never sets: `external.mk` adds it to
`host-imx-mkimage`. U-Boot enables `CONFIG_TEE`/`CONFIG_OPTEE` to probe OP-TEE
and add the `firmware/optee` node Linux needs.

OP-TEE is NXP's `lf-6.12.3-1.0.0` (`imx-mx8mpevk` platform), with the patches
of `board/common/patches/optee-os`:

| Patch | Purpose |
|-------|---------|
| `0001-libmbedtls-enable-PKCS1-v2.1-for-user-TAs` | RSA-PSS in TAs |
| `0002-libmbedtls-add-TLS-1.2-client-for-user-TAs` | TLS 1.2 client in the TA dev kit |

## Storage layout

U-Boot: `mmc 1`. Linux: `/dev/mmcblk1`. Built by `genimage.cfg` as
`output/images/sdcard.img`:

| Offset / partition | Content |
|--------------------|---------|
| 32 KiB | `imx8-boot-sd.bin` (outside the partition table) |
| 4 MiB | U-Boot environment (redundant) |
| p1 @ 16 MiB, 32 MiB | VFAT: `boot.scr` |
| p2, 1 GiB | rootfs A (`/boot/Image`, `/boot/imx8mp-debix-model-a.dtb`) |
| p3, 1 GiB | rootfs B |
| p4 | `/data`, grown to the end of the disk on first boot |

## A/B boot and SWUpdate

`boot.scr` (`u-boot/boot.cmd`) boots the kernel and dtb from `/boot` of the
rootfs selected by `rootpart`, falling back to the other one through
`altbootcmd` (bootcount). U-Boot patch 0004 adds the A/B variables to the
default environment. `post-image.sh` builds `debix-model-a-<version>.swu`
from `swupdate/sw-description`.

## Files

| File | Purpose |
|------|---------|
| `genimage.cfg` | storage layout above |
| `post-build.sh` | copies `Image` and the dtb into `/boot` of the rootfs |
| `post-image.sh` | `sdcard.img` and the `.swu` update |
| `linux/dts/freescale/imx8mp-debix-model-a.dts` | board dts (`BR2_LINUX_KERNEL_CUSTOM_DTS_DIR`): Ethernet, HDMI, USB hubs, CAN (`flexcan1`/`flexcan2`), Wi-Fi/BT (BCM4345/6 on `usdhc1` SDIO, Bluetooth on `uart1`) |
| `u-boot/uboot.fragment` | merged into the U-Boot defconfig: OP-TEE, redundant environment, bootcount, USB gadget (UMS, ACM, Ethernet) |
| `u-boot/boot.cmd` | A/B boot script |
| `u-boot/patches/` | U-Boot patches, below |
| `rootfs_overlay/usr/lib/firmware/brcm/` | BCM4345/6 NVRAM, see Wi-Fi |

## U-Boot patches

| Patch | Purpose |
|-------|---------|
| `0001` | no UHS voltage switching on the SD slot (`no-1-8-v`): fixes intermittent `Card did not respond to voltage select!` |
| `0002` | dummy `imx8m,mcu_rdc` node: silences SPL's `Failed to find node!` errors |
| `0003` | OP-TEE as a FIT loadable in U-Boot's binman template (the image actually used comes from `mkimage_fit_atf.sh`, see Boot chain) |
| `0004` | SWUpdate A/B variables in the default environment |
| `0005` | USB PHY and controller configuration |
| `0006` | keeps the FEC PHY reset GPIO in the Linux dtb, so the PHY is out of reset when Linux probes it |

## Wi-Fi and Bluetooth

- The NVRAM file for the BCM4345/6 comes from a Raspberry Pi 4B/CM4 (same
  chip family) and is **not** RF-tuned for this board's antenna: replace it
  with Polyhex's file if available.
- `brcmfmac` is a module (`board/common/linux/wifi.fragment`): built in, its
  SDIO probe can request the firmware before the rootfs is mounted.

## Flashing

At the U-Boot prompt, `mmc list` shows the devices; `ums 0 mmc <devnum>`
(e.g. `ums 0 mmc 1`) exports one to a USB host through a device-capable USB
port, until Ctrl-C. On the host:

```
sudo dd if=output/images/sdcard.img of=/dev/sdX bs=1M
```

Do not mount or modify the device on the host and the board at the same time.
