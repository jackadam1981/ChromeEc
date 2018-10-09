Google Security Chip (GSC) CCD
------------------------------
### Background

The GSC CCD was designed to restrict CCD access to device owners. There are
**CCD privilege levels** that can be used to enable access to different CCD
capabilities: **Open, Unlocked, Locked**.

All CCD functionality has been assigned to different **CCD capabilities**.
Capability settings can be modified to require certain privilege levels to
access the each capability. Setting a capability requirement to **IfOpened**
will require a level of **Open** to access that capability. A requirement of
**UnlessLocked** will require the device to be **Open** or **Unlocked** to
access the capability. Setting the requirement to **Always** will make the
capability always accessible.

Owners can use these settings to customize the CCD so it is as open or
restricted as they want.


## GSC Capabilities

The GSC is locked by default. Here are all of the Capabilities and their default
settings.

| Capability   | Default   | Function |
|---|---|---|
| UartGscRxAPTx   | Always   | controls reading from the AP console |
| UartGscTxAPRx   | Always   | controls writing to the AP console |
| UartGscRxECTx   | Always   | controls reading from the EC console |
| UartGscTxECRx   | IfOpened | controls writing to the EC console |
| FlashAP         | IfOpened | controls flashing the AP |
| FlashEC         | IfOpened | controls flashing the EC |
| OverrideWP      | IfOpened | controls controlling write protect |
| RebootECAP      | IfOpened | controls rebooting the EC/AP from the cr50 console |
| GscFullConsole  | IfOpened | controls access to restricted Cr50 console commands |
| UnlockNoReboot  | Always   | controls unlocking Cr50 without rebooting the AP |
| UnlockNoShortPP | Always   | controls unlocking Cr50 without physical presence |
| OpenNoTPMWipe   | IfOpened | controls opening Cr50 without wiping the TPM |
| OpenNoLongPP    | IfOpened | controls opening Cr50 without physical presence |
| BatteryBypassPP | Always   | controls opening cr50 without physical presence and dev mode if the battery is removed |
| UpdateNoTPMWipe | Always   | controls updating cr50 without wiping the TPM |
| I2C             | IfOpened | controls access to the I2C master (used for measuring power) |
| FlashRead       | Always   | controls dumping a hash of the AP or EC flash |
| OpenNoDevMode   | IfOpened | controls opening cr50 without dev mode |
| OpenFromUSB     | IfOpened | controls opening cr50 from USB |

Opening the GSC
---------------

The first cr50 image with CCD support was 0.3.9. If you are not running 0.3.9,
you need to download the image and update cr50 from the AP or using Suzy-Q.
https://storage.googleapis.com/chromeos-localmirror/distfiles/cr50.r0.0.10.w0.3.9.tbz2

You can download the cr50 image and then flash cr50 using Suzy-Q from the chroot
	sudo gsctool cr50.r0.0.10.w0.3.9/cr50.bin.prod

If you are only briefly using ccd and aren’t doing anything that may brick the
device, you can probably just stick to opening cr50. The open state will be lost
after cr50 reboot, so if you don’t want to have to reopen cr50, you may want to
setup the ccd capabilities so that you can use them without needing cr50 to be
open.

## Standard Open Process (Requires Booting to Kernel)
If your device can boot, you can open the gsc by entering dev mode and then
sending the ccd open command from the kernel.
### Enter dev mode
Entering dev mode has to be done manually. Using the gbb flags to force dev mode
will not work.

1. First, on a root shell on the device, check the force dev mode flag isn’t set
GBB flags by running.  If you can’t access the shell, because you aren’t in dev
mode, then you’re fine.  You can skip steps 1, 2, and 3.
`/usr/share/vboot/bin/get_gbb_flags.sh`

2. Clear 0x8 from the GBB flags and set the new value by
`/usr/share/vboot/bin/set_gbb_flags.sh $OLD_FLAG_VALUE & ~0x8`.

3. Reboot the device

4. Put the device into recovery mode
 - Tablets/Detachables - hold power button vol up and vol down for 10 seconds.
                         Release and wait until the device boots into recovery
 - Clamshells/Convertibles - press power button escape refresh
 - Chromeboxes - Use a paperclip to press the recovery button while plugging in
                 AC.
 - Using servo - If cr50 is open or you are using a flex cable you can, you can
                 use dut-control power_state:rec

5. Enable developer mode
 - Tablets/Detachables - After the device boots into recovery, press volume up
                         and volume down at the same time to get to the enable
			 dev mode menu. Use volume up button to navigate to
			 “confirm disabling os verification” use the power
			 button to select it
 - Clamshells/Convertibles - press ctrl+d on keyboard or AP console to select
                             developer mode then enter to enable it.
 - Chromeboxes - Use a paper clip to press the dedicated recovery button

6. Verify Cr50 knows the device is in dev mode.  The TPM state will print
   `dev_mode` if cr50 knows the device is in dev mode.  If it doesn’t say
   `dev_mode`, ccd open will fail.
    If you see "`TPM: dev_mode`" you are okay to CCD open now.
    If you don’t see `dev_mode`, recheck the gbb flags to make sure they aren’t
    forcing dev mode. Retry the manual entry of dev mode
```
          > ccd
            State: Locked
            Password: none
            Flags: 0x000001
            Capabilities: 0000000000000000
            UartGscRxAPTx   Y 0=Default (Always)
            UartGscTxAPRx   Y 0=Default (Always)
            UartGscRxECTx   Y 0=Default (Always)
            UartGscTxECRx   - 0=Default (IfOpened)
            FlashAP         - 0=Default (IfOpened)
            FlashEC         - 0=Default (IfOpened)
            OverrideWP      - 0=Default (IfOpened)
            RebootECAP      - 0=Default (IfOpened)
            GscFullConsole  - 0=Default (IfOpened)
            UnlockNoReboot  Y 0=Default (Always)
            UnlockNoShortPP Y 0=Default (Always)
            OpenNoTPMWipe   - 0=Default (IfOpened)
            OpenNoLongPP    - 0=Default (IfOpened)
            BatteryBypassPP Y 0=Default (Always)
            UpdateNoTPMWipe Y 0=Default (Always)
            I2C             - 0=Default (IfOpened)
            FlashRead       Y 0=Default (Always)
            OpenNoDevMode   - 0=Default (IfOpened)
            OpenFromUSB     - 0=Default (IfOpened)
            TPM: dev_mode     <==== This is the important part
            Use 'ccd help' to print subcommands
```

### Run ccd open

You can start the open process from the AP. Once you start the process, you will
need to press  the power button when prompted open cr50.

1. Start the ccd open process. From the AP run `gsctool -a -o`

2. Over the next 5 minutes you will be prompted to tap the power button.

3. After the process is finished, use ‘ccd’ on the cr50 console to verify the
   state is open

The Open setting will be lost whenever cr50 reboots. Make sure to setup ccd
so you will be able to recover the device even if Open is lost. To open cr50 you
need access to the AP. If your debugging will make the AP inaccessible and you
want to ensure that you can recover the device, you either need to modify the
capability settings so you can access the capabilities necessary to recover the
device while cr50 is locked or you need to modify the capabilities so you don't
need the AP to open cr50.

If you need to reflash the AP or EC, you can set the FlashEC or FlashAP
capabilities to Always.

If you want to be able to open cr50 without the AP, set OpenNoDevMode and
OpenFromUSB to Always.
