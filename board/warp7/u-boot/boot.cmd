setenv bootargs "console=${console},${baudrate} root=/dev/mmcblk2p${rootpart} rootwait rw oops=panic panic=5"

# Hold the Cortex-M4 in reset (SRC_M4RCR power-on value): a warm reset keeps
# it running the firmware of the previous session, whose startup would then
# reconfigure clocks and RDC under Linux. Linux remoteproc starts it.
mw.l 0x3039000c 0xab

if mmc dev ${mmcdev}; then
    load mmc ${mmcdev}:${rootpart} ${loadaddr} /boot/zImage;
    load mmc ${mmcdev}:${rootpart} ${fdt_addr} /boot/imx7s-warp.dtb;
    bootz ${loadaddr} - ${fdt_addr}
fi

run altbootcmd
