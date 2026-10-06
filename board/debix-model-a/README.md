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
of `board/common/patches/optee-os` then `board/debix-model-a/patches/optee-os`
(`BR2_GLOBAL_PATCH_DIR`):

| Patch | Purpose |
|-------|---------|
| common `0001-libmbedtls-enable-PKCS1-v2.1-for-user-TAs` | RSA-PSS in TAs |
| common `0002-libmbedtls-add-TLS-1.2-client-for-user-TAs` | TLS 1.2 client in the TA dev kit |
| board `0001-pta-stm32mp-build-the-remoteproc-PTA-only-with-CFG_S` | build the STM32MP remoteproc PTA only on STM32MP |
| board `0002-plat-imx-add-i.MX8MP-Cortex-M7-remoteproc-support` | M7 remoteproc driver and PTA (`CFG_IMX_REMOTEPROC=y`), see Cortex-M7 |

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
| `linux/dts/freescale/imx8mp-debix-model-a.dts` | board dts (`BR2_LINUX_KERNEL_CUSTOM_DTS_DIR`): Ethernet, HDMI, USB hubs, CAN (`flexcan1`/`flexcan2`), Wi-Fi/BT (BCM4345/6 on `usdhc1` SDIO, Bluetooth on `uart1`), Cortex-M7 (remoteproc, RPMsg); UART3 left to the M7 |
| `patches/` | OP-TEE and Linux patches (`BR2_GLOBAL_PATCH_DIR`), see Cortex-M7 |
| `linux/rpmsg.fragment` | `CONFIG_IMX_REMOTEPROC`, `CONFIG_IMX_MBOX`, RPMsg virtio, `rpmsg_tty` (module) |
| `u-boot/uboot.fragment` | merged into the U-Boot defconfig: OP-TEE, redundant environment, bootcount, USB gadget (UMS, ACM, Ethernet) |
| `u-boot/boot.cmd` | A/B boot script |
| `u-boot/patches/` | U-Boot patches, below |
| `rootfs_overlay/usr/lib/firmware/brcm/` | BCM4345/6 NVRAM, see Wi-Fi |

## Cortex-M7 (remoteproc)

OP-TEE loads, authenticates, starts and stops the M7 (`CFG_IMX_REMOTEPROC=y`,
`fsl,imx8mp-cm7-tee` node, `patches/` below): Linux `imx_rproc` hands the
signed firmware to the OP-TEE remoteproc TA, which checks its signature,
loads it into the TCM and releases the M7 from `CPUWAIT`. Linux then talks to
the M7 over RPMsg through the MU, on the resource table of the loaded ELF.

Sign the Zephyr ELF with the OP-TEE script and the key whose public part is
built into OP-TEE (`RPROC_SIGN_KEY`, default `keys/default.pem`: the public
OP-TEE development key, replace it for a product), RSA only (the PTA does not
verify ECDSA). The script needs `pycryptodomex` and `pyelftools`:

```
O=output/build/optee-os-custom
python3 $O/scripts/sign_rproc_fw.py --in zephyr.elf --out m7-firmware.elf \
	--key $O/keys/default.pem
```

```
cp m7-firmware.elf /lib/firmware/
echo m7-firmware.elf > /sys/class/remoteproc/remoteproc0/firmware
echo start > /sys/class/remoteproc/remoteproc0/state
echo stop  > /sys/class/remoteproc/remoteproc0/state
```

An unsigned ELF is refused ("Not a signed firmware image"). A stop asks the
TA to release the firmware, which clears the TCM. The firmware is only
authenticated at load: the M7 is a non-secure bus master and the TCM stays
writable from Linux. With `fsl,imx8mn-cm7` instead, Linux loads an unsigned
ELF itself and starts the M7 through TF-A (`IMX_SIP_SRC`).

Tested on the Debix Model A: signed Zephyr firmware (TCM) loaded, started
and stopped through OP-TEE.

Buildroot applies patches only when it extracts a package: after changing
them, rebuild OP-TEE and Linux from scratch
(`make optee-os-dirclean linux-dirclean all`).

| Linux patch (`patches/linux`) | |
|-------|-|
| `0001-dt-bindings-remoteproc-imx_rproc-add-fsl-imx8mp-cm7-` | dt-bindings: `fsl,imx8mp-cm7-tee` |
| `0002-remoteproc-imx_rproc-add-OP-TEE-mode-for-i.MX8MP` | `imx_rproc` OP-TEE mode: TA load, start, stop, release |

| Region | Address |
|--------|---------|
| ITCM / DTCM (M7 code / data) | M7 0x0 / 0x20000000, Linux 0x007E0000 / 0x00800000 |
| RPMsg vrings (`vdev0vring0`/`1`) | 0x55000000 / 0x55008000 |
| RPMsg buffers (`vdevbuffer`) | 0x55400000, 1 MiB |
| OP-TEE | 0x56000000 - 0x58000000 |

- Clock: `IMX8MP_CLK_M7_CORE`. The NXP EVK DT uses `IMX8MP_CLK_M7_DIV`, which
  this kernel does not register: `imx_rproc` then gets no clock, `m7_core` is
  disabled as unused, and any access to the TCM hangs the bus (and remoteproc
  with it, until the watchdog resets the board).
- Resource table: the one in the ELF. `<&rsc_table>` (0x550ff000, NXP SDK
  firmware) is left out of `memory-region`: with it, `imx_rproc` writes the
  vring addresses and the "driver ready" status there, and firmware keeping
  its table in the ELF (Zephyr) waits forever.
- MU channel 1 (`mboxes = <&mu 0 1>, <&mu 1 1>, <&mu 3 1>`): Linux listens on
  MU register 1 and then processes every vring, so the M7 must kick on
  register 1 (Zephyr `rpmsg_echo`: `CONFIG_RPMSG_ECHO_MU_KICK_ID=1`).
- The M7 has a data cache: firmware must maintain it on the shared DDR
  (Zephyr: `CONFIG_CACHE_MANAGEMENT=y`, `CONFIG_OPENAMP_WITH_DCACHE=y`).
- Console: UART3 (`/dev/ttymxc2` when Linux owns it, disabled in the Linux DT
  for the M7). On the DEBIX I/O board, DIP switch SW1-1 ON (up) routes it to
  the 40-pin header J2: pin 8 TX, pin 10 RX, ground on pin 6, 3.3 V TTL. With
  SW1-1 OFF it goes to the RS232 transceiver (J12 pins 6/5/7).
- An `rpmsg-tty` channel from the M7 shows up as `/dev/ttyRPMSG*` once the
  `rpmsg_tty` module is loaded (`modprobe rpmsg_tty` if not automatic).
- No DDR is reserved for M7 code: firmware runs from the TCM (Zephyr board
  `imx8mp_debix_model_a/mimx8ml8/m7`, not the `ddr` variant). For the `ddr`
  variant, reserve its DDR in Linux and give it to OP-TEE with
  `CFG_IMX_REMOTEPROC_DDR_START`/`CFG_IMX_REMOTEPROC_DDR_SIZE`.

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
