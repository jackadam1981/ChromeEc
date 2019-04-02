# Reflashing an ITE EC

This doc: [http://go/cros-ite-ec-reflash][https://goto.google.com/cros-ite-ec-reflash]
First written: 2019-04-02
Last updated: 2019-04-02

Familiarity with [Chromium OS][https://www.chromium.org/chromium-os] [Embedded Controller (EC) development][https://www.chromium.org/chromium-os/ec-development] is assumed.

[TOC]

## Background

### Terminology

**ITE EC** refers to the [ITE][http://www.ite.com.tw/] [IT8320][http://www.ite.com.tw/en/product/view?mid=96] [Embedded Controller (EC)][https://en.wikipedia.org/wiki/Embedded_controller] microcontroller when used as a Chromium OS / Chrome OS EC.

**CrOS** refers to Chromium OS, Chrome OS, or both, depending on the context.  The distinction between Chromium OS and Chrome OS is largely immaterial to this document.

**Servo** refers to a debug board providing direct debug access to various circuits on a Chrome OS device motherboard.  As of this writing, the most common [servos][https://www.chromium.org/chromium-os/servo] used by CrOS developers are [CR50 (CCD)][https://www.chromium.org/chromium-os/ccd], [Servo Micro][https://www.chromium.org/chromium-os/servo/servomicro], and [Servo v2][https://www.chromium.org/chromium-os/servo/servo-v2].  (Note that [Servo v4][https://www.chromium.org/chromium-os/servo/servov4] is **not** a Servo in this sense.  It is a USB hub with a microcontroller that proxies Servo functionality from either CR50 CCD or Servo Micro.)  See also [Case-Closed Debug in Chromebooks and Servo Micro][https://chromium.googlesource.com/chromiumos/platform/ec/+/master/board/servo_micro/ccd.md].

### How ITE EC reflashing works

An ITE EC is reflashed using a Servo by:

1. Sending special non-I2C waveforms over its I2C clock and data lines, to enable a debug mode / direct firmware update (DFU) mode.

1. Communicating with it using I2C, including transferring the actual EC image over I2C.  (The ITE EC will only respond over I2C after receiving the special waveforms.)

See [IT8320_eflash_SMBus_Programming_Guide.pdf][https://drive.google.com/file/d/0B5qRYWtgJKXnSkZsbm9FeUxVaXM2T0dfQWR3SkRNVmRtUmlV/view] for details on the special waveforms and I2C messages involved.

The need for special waveforms and use of I2C for transferring images are unique among CrOS EC microcontrollers to date.

## How to reflash

### Prerequisites for CR50 CCD or Servo Micro

This section applies whether using CR50 CCD via [Servo v4][https://www.chromium.org/chromium-os/servo/servov4] or [SuzyQ / SuzyQable][https://www.sparkfun.com/products/14746].

This section applies whether the [Servo Micro][https://www.chromium.org/chromium-os/servo/servomicro] is connected directly to your development host, or through a [Servo v4][https://www.chromium.org/chromium-os/servo/servov4].

1. Install the `i2c-pseudo` Linux kernel module.  (Do this **outside** of the CrOS development chroot!)
    * `$ cd src/platform/ec/extra/i2c_pseudo`
    * `$ ./install`

If the above fails, your system may be missing packages necessary for building kernel modules.  Consult your Linux distribution's documentation and support forums.  After installing any packages that might be missing, simply try the install script again.

You will need to reinstall `i2c-pseudo` after each kernel upgrade.

There is an intention to [upstream i2c-pseudo][https://issuetracker.google.com/129565355], though even if accepted upstream, it may or may not become included with common Linux distribution kernels.

## Reflashing with Servo Micro

This section applies whether the [Servo Micro][https://www.chromium.org/chromium-os/servo/servomicro] is connected directly to your development host, or through a [Servo v4][https://www.chromium.org/chromium-os/servo/servov4].

## Reflashing with Servo v2

This section applies when using a [Servo v2][https://www.chromium.org/chromium-os/servo/servo-v2] with its Yoshi Flex cable connected to the DUT.

### Common reflash instructions

These instructions apply when using any kind of Servo, including those with no special prerequisites.

1. Enter the CrOS development chroot (for servod).
    * `$ cros_sdk --no-ns-pid`
1. Start servod.
    * `$ servod --board=<board_name>`
1. Enter the CrOS development chroot (for flash_ec).
    * `$ cros_sdk`
1. Build the EC image for your board.
    * `$ cd ~/trunk/src/platform/ec`
    * `$ board=<board_name>`
    * `$ make -j BOARD="$board"`
1. Run flash_ec from the util directory.
    * `$ util/flash_ec --board="$board" --image=build/"$board"/ec.bin`
