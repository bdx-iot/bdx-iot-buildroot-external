env set  bootargs 'console=${console} root=/dev/mmcblk1p'${rootpart}' rootwait rw oops=panic panic=5 vt.global_cursor_default=0';

run swu_setup;
if mmc dev 1; then
    load mmc 1:${rootpart} ${fdt_addr_r} /boot/${fdtfile};
    load mmc 1:${rootpart} ${kernel_addr_r} /boot/Image;
    booti ${kernel_addr_r} - ${fdt_addr_r}
fi

run altbootcmd
