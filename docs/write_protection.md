# Firmware Write Protection

[TOC]

This is a somewhat tricky topic since there's a lot of historical baggage in the
existing documentation (as well as many different terms with essentially the
same meaning). I'll do my best to decode it here, but please edit or open a bug
ifCl something is not clear.

## Terminology

### EC

EC (aka Embedded Controller) can refer to many things in the Chrome OS
documentation due to historical reasons. If you just see the term "EC", it
probably refers to "the" EC (i.e. the first one that existed). Most Chrome OS
devices have an MCU, known as "the EC" that controls lots of things (key
presses, turning the AP on/off). The OS that was written for "the" EC is now
running on several different MCUs on Chrome OS devices with various tweaks
(e.g. the FPMCU, the touchpad one that can do palm rejection, etc.). It's quite
confusing, so try to be specific and use terms like FPMCU to distinguish the
fingerprint MCU from "the EC".

## Hardware Write Protect

The Fingerprint MCU for Chrome OS only exists on nocturne devices and newer. On
these modern devices, the Cr50 (aka GSC / TPM) provides a "hardware
write protect" GPIO that is connected to all of the ECs in the system
(fingerprint,"the EC", etc) via a
[GPIO](https://chromium.googlesource.com/chromiumos/platform/ec/+/aaba1d5efd51082d143ce2ac64e6caf9cb14d5e5/include/ec_commands.h#1599).
This "hardware write protect" can only be disabled with servo or suzyq
(["CCD open"](http://go/ccd-open)) and corresponds to
[`OverrideWP`](../case_closed_debugging_cr50.md)
in ccd. Disabling this write protect disables it for every EC on the device.

In the case of the FPMCU, the hardware write protect GPIO is tied to the STM32
`BOOT0` pin, which is what tells the MCU to enter the STM32 bootloader mode.

You may see various references to a
[write protect screw in documentation](https://www.chromium.org/chromium-os/firmware-porting-guide/firmware-ec-write-protection).
Older Chromebooks had a write protect screw that had to be physically removed.
More details on this history can be found here: http://go/cros-wp-status.

Additional reference:
https://www.google.com/chromeos/partner/fe/docs/cpfe/firmwaretestmanual.html#hardware-write-protect

### Toggling HW Write Protect

*** note
`servod` *must* be running for `dut-control` to work
***

#### Enable

```bash
(chroot)$ dut-control fw_wp_state:force_on
```

#### Disable

```bash
(chroot)$ dut-control fw_wp_state:force_off
```

#### Enable/Disable via Cr50 Console

You can use the following commands from the Cr50 console:

```bash
wp disable
```

```bash
wp enable
```

```bash
wp follow_batt_pres
```

## Software Write Protect

In addition to HW write protect, each EC has its own RW/RO flash block
protection. This can be toggled with `ectool --name=cros_fp flashprotect
enable/disable`, which sends the `EC_CMD_FLASH_PROTECT` command toggling
`EC_FLASH_PROTECT_RO_AT_BOOT` (changing `--name` to target different ECs).

*** note
NOTE: You cannot disable software write protect if hardware write protect is
enabled.
***

WARNING: If you disable HW write protect *and* then reboot the FPMCU, it will do
a mass erase of the chip, due to [RDP1](#rdp1).

Additional reference:
https://www.google.com/chromeos/partner/fe/docs/cpfe/firmwaretestmanual.html#software-write-protect

## `system_is_locked()`

The
[`system_is_locked()`](https://chromium.googlesource.com/chromiumos/platform/ec/+/aaba1d5efd51082d143ce2ac64e6caf9cb14d5e5/common/system.c#195)
function in the EC code returns false if the HW write protect GPIO is disabled
or the read-only firmware is not protected. We mainly use it to guard test/debug
functionality in the firmware, so that we can run tests against the exact
firmware we ship by disabling the hardware write protection.

## RDP1 {#rdp1}

Stands for Readout Protection Level 1.

Protects user flash memory against a debugger (JTAG/SWD) or potential malicious
code stored in RAM by disabling access (a bus error is generated when read
access is requested). Otherwise (no debugger connected and no boot in RAM set),
all read/program/erase operations from/to flash are allowed.

When switching to a lower level of RDP (i.e., setting to 0), the user flash
memory is mass erased (set to all `0xFF`).

Note that this completely destroys *all* of the firmware, including the RO
section.

### Additional References

https://chromium-review.googlesource.com/c/chromiumos/platform/ec/+/1222094

## EC Flash Read/Write Command Write Protection Checks

The EC code command handlers (`command_flash_erase`, `command_flash_write`,
etc.) return an error if `EC_FLASH_PROTECT_ALL_NOW` is set.
