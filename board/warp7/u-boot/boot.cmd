setenv bootargs "console=${console},${baudrate} root=/dev/mmcblk2p${rootpart} rootwait rw oops=panic panic=5"

if mmc dev ${mmcdev}; then
    load mmc ${mmcdev}:${rootpart} ${loadaddr} /boot/zImage;
    load mmc ${mmcdev}:${rootpart} ${fdt_addr} /boot/imx7s-warp.dtb;
    bootz ${loadaddr} - ${fdt_addr}
fi

run altbootcmd
