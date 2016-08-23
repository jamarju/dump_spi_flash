# SPI NOR flash dumper and programmer

This is a SPI NOR flash dumper and programmer based on the ARM STM32F103C8T6 development board (AKA "Bluepill"), made specifically to dump and replace the Winbond 25Q64CVFIG chip used in Ubiquiti Unifi access points (UAP). Apparently these tend to fail over time with a very common problem in NOR flashes: bit flipping. Since there is no ECC at all in these chips, a bit flip will cause corrupted reads, which are rewarded by Linux with kernel panics, random lockups or the inability to boot the UAP at all.

I wrote this because I don't have a proper programmer and I wanted to fix my UAPs as cheaply as possible with parts I already owned. Probably a universal programmer (like the MiniPro / TL866) is a much easier and better solution.

## Parts

- 1 x STM32F103C8T6 "Bluepill" board (~1.50 €, aliexpress)
- 1 x ST-Link v2 dongle (~1.90 €, aliexpress)
- N x replacement 25Q64CVFIG chips (~0.90 € each, aliexpress)
- 3 x 4K7 resistors
- 1 x breadboard
- 1 x hot air tool to remove the chip
- 1 x FTDI or similar 3.3v serial to USB cable.
- Time and patience since the aliexpress orders can take 30-50 days.

## Diagnose the problem

A bit flipping NOR problem will show up as bootloader errors trying to decompress the Linux kernel, or later on as Linux panics trying to decompress corrupted data from the squashfs filesystem.

### Look for bootloader errors

Example of an OK boot:

```
U-Boot unifi-v1.5.2.206-g44e4c8bc (Aug 29 2014 - 18:01:57)

DRAM:  64 MB
Flash:  8 MB
PCIe WLAN Module found (tries: 1).
Net:   eth0, eth1
Board: Copyright Ubiquiti Networks Inc. 2014
Hit any key to stop autoboot:  0
Board: Ubiquiti Networks AR7241 board (e502.0101.002e)
UBNT application initialized
## Booting image at 9f050000 ...
   Image Name:   MIPS Ubiquiti Linux-2.6.32.33
   Created:      2015-05-31  23:49:15 UTC
   Image Type:   MIPS Linux Kernel Image (lzma compressed)
   Data Size:    918694 Bytes = 897.2 kB
   Load Address: 80002000
   Entry Point:  80002000
   Verifying Checksum at 0x9f050040 ...OK
   Uncompressing Kernel Image ... OK

Starting kernel ...

Booting...
```

Example of BAD boot (corrupted kernel partition):

```
U-Boot unifi-v1.5.2.206-g44e4c8bc (Aug 29 2014 - 18:01:57)

DRAM:  64 MB
Flash:  8 MB
PCIe WLAN Module found (tries: 1).
Net:   eth0, eth1
Board: Copyright Ubiquiti Networks Inc. 2014
Hit any key to stop autoboot:  0
Board: Ubiquiti Networks AR7241 board (e502.0101.002e)
UBNT application initialized
## Booting image at 9f050000 ...
   Image Name:   MIPS Ubiquiti Linux-2.6.32.33
   Created:      2015-05-31  23:49:15 UTC
   Image Type:   MIPS Linux Kernel Image (lzma compressed)
   Data Size:    918694 Bytes = 897.2 kB
   Load Address: 80002000
   Entry Point:  80002000
   Verifying Checksum at 0x9f050040 ...OK
   Uncompressing Kernel Image ... ERROR: LzmaDecode.c, 543

Decoding error = 1
LZMA ERROR 1 - must RESET board to recover

Resetting...
```

### Look for squashfs errors

To enable the console over the serial port (ttyS0), so you can see kernel messages and log in to a shell.

From the bootloader:

```
setenv bootargs root=31:03 rootfstype=squashfs init=/init console=ttyS0,115200 panic=3
saveenv
```

To print the bootloader env variables:

```
printenv
```

Here is the default environment:

```
bootargs=root=31:03 rootfstype=squashfs init=/init console=tty0 panic=3
bootcmd=run ubntappinit ubntboot
bootdelay=1
ipaddr=192.168.1.20
ubntappinit=go ${ubntaddr} uappinit;go ${ubntaddr} ureset_button;urescue;go ${ubntaddr} uwrite
ubntboot=bootm 0x9f050000
serverip=192.168.1.254
ethact=eth0
mtdids=nor0=ar7240-nor0
mtdparts=mtdparts=ar7240-nor0:256k(u-boot),64k(u-boot-env),1024k(kernel),6528k(rootfs),256k(cfg),64k(EEPROM)
partition=nor0,0
mtddevnum=0
mtddevname=u-boot
stdin=serial
stdout=serial
stderr=serial
ubntaddr=80200020
```

And here is an example of corrupted squashfs errors:

```
[    4.310000] SQUASHFS error: xz_dec_run error, data probably corrupt
[    4.310000] SQUASHFS error: squashfs_read_data failed to read block 0x14616e
[    4.320000] SQUASHFS error: Unable to read fragment cache entry [14616e]
[    4.320000] SQUASHFS error: Unable to read page, block 14616e, size 14160
```

## Hands-on: dump the flash

Linux device   | Start addr | Size     | Contents
-------------- | ---------- | -------- | --------
/dev/mtdblock0 |        0x0 |  0x40000 | u-boot (bootloader)
/dev/mtdblock1 |    0x40000 |  0x10000 | u-boot env
/dev/mtdblock2 |    0x50000 | 0x100000 | linux kernel
/dev/mtdblock3 |   0x150000 | 0x660000 | rootfs (squashfs)
/dev/mtdblock4 |   0x7b0000 |  0x40000 | cfg
/dev/mtdblock5 |   0x7f0000 |  0x10000 | EEPROM

The critical thing to back up is the 'EEPROM' partition, which contains device specific data like the MAC address and calibration. Everything else can be obtained from another UAP or the original firmware file. Ideally we want a full dump so that we can move it to a fresh chip and be done with it, but if the chip is bit flipping intensely, we'll try to focus on the EEPROM first.

### 1a: dumping via SSH

If we are lucky to boot the UAP and keep it up long enough we can dump the flash via ssh, just use the dump-via-ssh.sh script:

```
cd openocd
./dump-via-ssh.sh <ip address>
```

That will create a dump file named after the ip address of the UAP. It's a good idea to rename the file and repeat 2 or 3 times and compare them together until we get a sane dump.

### 1b: dumping via SPI

If we can't get Linux up in the UAP, then we have to remove the chip and wire it to the Bluepill like shown in the `schematic` dir (KiCad source, svg included).

Tools:

- Eclipse (Mars.2)
- The [GNU ARM Eclipse plugin](http://gnuarmeclipse.github.io)
- The [GCC for ARM toolchain](https://launchpad.net/gcc-arm-embedded) (gcc-arm-none-eabi-4_9-2015q3).
- [OpenOCD](http://openocd.org) (0.90).

Import the project (`dump_spi_flash` folder). My launch configuration is in the `launch_configurations` folder inside the project. It requires OpenOCD to be launched externally, so do that first:

```
cd openocd
./launch.sh
```

Openocd will spit some lines about ST-Link being plugged in, and the debugging capabilities of the STM32 chip, etc.

The project uses an ARM feature called semihosting, which means that the code runs mainly on the ARM chip but some POSIX I/O calls run on the host PC to which the ST-Link dongle is connected. So we can use the STM32's SPI peripheral to talk to the NOR chip while POSIX calls (open, read, write, ...) run on the PC via semihosting black magic.

In Eclipse use the `Debug` configuration: Project -> Build configurations -> Set active -> Debug

Run the project from the little triangle by the bug or the play buttons -> `dump_spi_flash Debug`.

On the OpenOCD screen, the program will try to identify the chip and read the status register, then ask what to do:

```
Hello ARM World!
System clock: 72000000 Hz
* Read id

-> 9f
<- ef 40 17
* Read status reg 1

-> 05
<- 00
* Read status reg 2

-> 35
<- 00
(d)ump, (p)rogram + verify or (v)erify?
```

* `d` will read the flash and write it to `dump.bin` (in OpenOCD's current directory).
* `p` will erase the chip and program `dump.bin` into it, then re-read the flash and verify it against the same file.
* `v` will read the flash and compare it to the contents of `dump.bin`.

A successful result of the `p` routine would be:

```
p
* Write enable

* Chip erase

-> c7
<-
* Write flash

* Verify flash

100% verify OK!!

Ok bye
```

Note: it takes a long time because NOR chips are slow to write to, but mostly because of the semihosting overhead.

## Flashing from bootloader via TFTP

Use tftpd-hpa (linux) or built-in server (OS X).

It might be useful to program NOR chunks via TFTP. We need a TFTP server in 192.168.1.254. The UAP address is 192.168.1.20, both of which can be changed via u-boot variables `serverip` and `ipaddr` with `setenv` and `saveenv` (see useful u-boot commands below).

This is to flash mtdblock2 (linux kernel). What we do here is:

1. Download mtdblock2 from the tftp root into temp RAM (0x83000000).
2. `protect off` the chip
3. Erase 0x100000 starting from 0x9f050000 (where mtdblock2 is mapped).
4. Program 0x100000 bytes from 0x83000000 (temp RAM) to 0x9f050000 (NOR).
5. re-read NOR block and compare to temp RAM block.

```
ar7240> tftp 83000000 mtdblock2
Using eth0 device
TFTP from server 192.168.1.254; our IP address is 192.168.1.20
Filename 'mtdblock2'.
Load address: 0x83000000
Loading: #################################################################
         #################################################################
         #################################################################
         ##########
done
Bytes transferred = 1048576 (100000 hex)
ar7240> protect off all
Un-Protect Flash Bank # 1
ar7240> erase 0x9f050000 +0x100000
................ done
Erased 16 sectors
ar7240> cp.b 0x83000000 0x9f050000 0x100000
Copy to Flash... write addr: 9f050000
done
ar7240> cmp.b 0x83000000 0x9f050000 0x100000
Total of 1048576 bytes were the same
```

For mtdblock3 (rootfs):

```
ar7240> tftp 83000000 mtdblock3
Using eth0 device
TFTP from server 192.168.1.254; our IP address is 192.168.1.20
Filename 'mtdblock3'.
Load address: 0x83000000
Loading: #################################################################
         #################################################################
         #################################################################
         #################################################################
         #################################################################
         #################################################################
         #################################################################
         #################################################################
         #################################################################
         #################################################################
         #################################################################
         #################################################################
         #################################################################
         #################################################################
         #################################################################
         #################################################################
         #################################################################
         #################################################################
         #################################################################
         #################################################################
         ######
done
Bytes transferred = 6684672 (660000 hex)
ar7240> erase 0x9f150000 +0x660000
...................................................................................................... done
Erased 102 sectors
ar7240> cp.b 0x83000000 0x9f150000 0x660000
Copy to Flash... write addr: 9f150000
done
ar7240> cmp.b 0x83000000 0x9f150000 0x660000
Total of 6684672 bytes were the same
```

Run the last command multiple times to check for bit-flips. Example: only one of the following commands went through, the others failed to compare the whole block at some random point.

```
ar7240> cmp.b 0x83000000 0x9f150000 0x660000
byte at 0x833e4e14 (0x00) != byte at 0x9f534e14 (0x40)
Total of 4083220 bytes were the same
ar7240> cmp.b 0x83000000 0x9f150000 0x660000
byte at 0x831384ab (0x80) != byte at 0x9f2884ab (0x90)
Total of 1279147 bytes were the same
ar7240> cmp.b 0x83000000 0x9f150000 0x660000
byte at 0x833e4e14 (0x00) != byte at 0x9f534e14 (0x40)
Total of 4083220 bytes were the same
ar7240> cmp.b 0x83000000 0x9f150000 0x660000
byte at 0x833b8bd0 (0x0e) != byte at 0x9f508bd0 (0x4e)
Total of 3902416 bytes were the same
ar7240> cmp.b 0x83000000 0x9f150000 0x660000
byte at 0x832c59de (0x4a) != byte at 0x9f4159de (0x4e)
Total of 2906590 bytes were the same
ar7240> cmp.b 0x83000000 0x9f150000 0x660000
byte at 0x831384ab (0x80) != byte at 0x9f2884ab (0x90)
Total of 1279147 bytes were the same
ar7240> cmp.b 0x83000000 0x9f150000 0x660000
Total of 6684672 bytes were the same
ar7240> cmp.b 0x83000000 0x9f150000 0x660000
byte at 0x8303a472 (0xa2) != byte at 0x9f18a472 (0xa6)
Total of 238706 bytes were the same
```

## Other useful u-boot commands:

### Change UAP's self and tftp server address:

```
ar7240>setenv ipaddr 192.168.1.x
ar7240>setenv serverip 192.168.1.y
ar7240>saveenv
```

### YMODEM

You can use YMODEM over serial to flash the firmware from u-boot:

This will load the image into RAM starting at 0x81000000.

```
loady   <-- YMODEM kernel (1024k)
protect off all
erase 0x9f050000 +0x00100000    (16 sectors)
cp.b 0x81000000 0x9f050000 0x00100000

loady   <-- YMODEM root_fs (6528k)
erase 0x9f150000 +0x00660000   (102 sectors)
cp.b 0x81000000 0x9f150000 0x00660000

loady   <-- YMODEM cfg (256k)
erase 0x9f7b0000 +0x00040000   (4 sectors)
cp.b 0x81000000 0x9f7b0000 0x00040000
```

Example out:

```
ar7240> loady
## Ready for binary (ymodem) download to 0x81000000 at 115200 bps...
CCCxyzModem - CRC mode, 2(SOH)/1024(STX)/0(CAN) packets, 5 retries
## Total Size      = 0x00100000 = 1048576 Bytes
ar7240> protect off all
Un-Protect Flash Bank # 1
ar7240> erase 0x9f050000 +0x00100000
................ done
Erased 16 sectors
ar7240> cp.b 0x81000000 0x9f050000 0x00100000


cmp.b 0x81000000 0x9f050000 0x00100000

Copy to Flash... write addr: 9f050000
done
```

### Show partitions

```
ar7240> mtdparts

device nor0 <ar7240-nor0>, # parts = 6
 #: name                        size            offset          mask_flags
 0: u-boot                      0x00040000      0x00000000      0
 1: u-boot-env                  0x00010000      0x00040000      0
 2: kernel                      0x00100000      0x00050000      0
 3: rootfs                      0x00660000      0x00150000      0
 4: cfg                         0x00040000      0x007b0000      0
 5: EEPROM                      0x00010000      0x007f0000      0

 active partition: nor0,0 - (u-boot) 0x00040000 @ 0x00000000

 defaults:
 mtdids  : nor0=ar7240-nor0
 mtdparts: mtdparts=ar7240-nor0:256k(u-boot),64k(u-boot-env),1024k(kernel),6528k(rootfs),256k(cfg),64k(EEPROM)
```

### Reset partitions

```
ar7240> mtdparts default
ar7240> saveenv
Saving Environment to Flash...
Un-Protected 1 sectors
Erasing Flash.... done
Erased 1 sectors
Writing to Flash... write addr: 9f040000
done
Protected 1 sectors
```

### Reboot

```
reset
```

## Acknowledgments

* [The guy who posted this](https://community.ubnt.com/t5/airMAX-General-Discussion/ns-loco-m2-lost-his-WLAN-MAC-and-how-to-repair-a-bad-flash/td-p/545712).
* [Everyone involved in this post](http://community.ubnt.com/t5/UniFi-Wireless/Again-on-Unifi-BAD-DATA-CRC/m-p/746202)
* This guy's scratch notes: ftp://powerfast.net/pub/windows/routers/ubiquiti/unifis/ubiquit-boot-flash.txt
