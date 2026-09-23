# Ynov Buildroot External

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

Clone, configure, and build.  When building, use the appropriate defconfig in
the `buildroot-ynov-external/configs` directory for your board.
Here, as an example, we use `warp7_ynov_defconfig or stm32mp157a_dk1_ynov_defconfig`.

```
$ git clone https://github.com/Ynov-Linux/buildroot-ynov-external.git
$ git clone https://git.busybox.net/buildroot
$ cd buildroot
$ make BR2_EXTERNAL=../buildroot-ynov-external/ warp7_ynov_defconfig
$ make
```

The resulting bootloader, kernel, and root filesystem will be put in the
'output/images' directory.  There is also a complete `sdcard.img`.

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
