# bdx-iot-external

Buildroot external tree for the Polyhex Debix Model A (i.MX8M Plus) board.

## Board support: `debix-model-a`

Defconfig: `configs/imx8mp_debix_model_a_defconfig`

- **Boot chain**: SPL → ATF (BL31) → OP-TEE (BL32) → U-Boot (BL33) → Linux, via a
  single FIT image (`imx8-boot-sd.bin`) built by NXP's `imx-mkimage`.
- **Boot media**: single bootable ext4 partition (rootfs + `/boot/Image` +
  `/boot/*.dtb` + `/boot/extlinux/extlinux.conf`); `imx-boot` lives outside the
  partition table at offset 32K. Custom `genimage.cfg`/`post-image.sh` replace
  NXP's default two-partition (FAT+ext2) layout.
- **Kernel devicetree**: custom board dts at
  `board/debix-model-a/linux/dts/freescale/imx8mp-debix-model-a.dts`
  (`BR2_LINUX_KERNEL_CUSTOM_DTS_DIR`), covering ethernet, HDMI, USB hubs,
  CAN (`flexcan1`/`flexcan2`), and the onboard WiFi/BT combo chip
  (BCM4345/6 on `usdhc1` SDIO + Bluetooth on `uart1`).
- **OP-TEE**: enabled via `BR2_TARGET_OPTEE_OS` + `SPD=opteed` on ATF. See
  `patches/u-boot/0003-*` and `0004-*`, and `external.mk` (patches
  `host-imx-mkimage`'s `mkimage_fit_atf.sh` to add the missing `os = "tee";`
  FIT property, since that's what SPL needs to locate BL32).
- **WiFi/BT firmware**: `board/debix-model-a/rootfs_overlay/usr/lib/firmware/brcm/`
  provides the missing default nvram calibration file for the BCM4345/6 chip
  (reused from a Raspberry Pi 4B/CM4 nvram — same chip family, but **not**
  RF-tuned for this board's antenna; replace with Polyhex's file if available).
- `brcmfmac` is built as a kernel **module** (not built-in) — see
  `board/debix-model-a/linux/wifi.fragment`. This is required: a built-in
  driver's SDIO probe (and firmware request) can run before the real rootfs
  is mounted, causing spurious firmware-not-found errors.

### U-Boot patches (`patches/u-boot/`)

Applied via `BR2_TARGET_UBOOT_PATCH`, applied in order:

1. `0001-*-disable-usdhc2-uhs-voltage-swi.patch` — disables UHS voltage
   switching on the SD card slot (`no-1-8-v;`), fixing intermittent
   `Card did not respond to voltage select!` boot failures.
2. `0002-*-add-dummy-mcu_rdc-node.patch` — adds a dummy `imx8m,mcu_rdc`
   devicetree node so SPL's RDC config code doesn't print (harmless) errors.
3. `0003-imx8mp-load-optee-as-fit-loadable.patch` — adds a `tee`/`os = "tee";`
   FIT entry to U-Boot's own binman template. Kept for consistency, but note
   the **actual** production image is built by `mkimage_fit_atf.sh` (patched
   separately via `external.mk`), not this template.
4. `0004-*-enable-optee-tee-uclass.patch` — enables `CONFIG_TEE`/`CONFIG_OPTEE`
   in U-Boot proper, so it can probe OP-TEE and perform the devicetree fixup
   (`firmware/optee` node) that the Linux `optee` driver needs.

## Install System Dependencies

The external is tested on Ubuntu 22.04 LTS.  The following system build
dependencies are required.
```
$ sudo apt-get install subversion build-essential bison flex gettext \
    libncurses5-dev texinfo autoconf automake libtool mercurial git-core \
    gperf gawk expat curl cvs libexpat-dev bzr unzip bc python-dev \
    wget cpio rsync xxd
```

In some cases, buildroot will notify that additional host dependencies are
required.  It will let you know what those are.

## Build

Clone, configure, and build against Buildroot.

```
$ git clone https://github.com/bdx-iot/bdx-iot-external.git
$ git clone https://git.busybox.net/buildroot
$ cd buildroot
$ make BR2_EXTERNAL=../bdx-iot-external imx8mp_debix_model_a_defconfig
$ make
```

The resulting bootloader, kernel, and root filesystem will be put in the
`output/images` directory. There is also a complete `sdcard.img`.

#### Create an eMMC/SD Card

A SD card image is generated in the file `sdcard.img`.
This image can be written directly to an eMMC/SD card.

```
$ cd output/images
$ sudo dd if=sdcard.img of=/dev/sdX bs=1M
```
Another method, which is cross platform, to write the SD card image is to use
[Etcher][1].

[1]: https://etcher.io/

