# Reducing the EC image size

The EC ToT code base continues grows as new features are added and for bug
fixes. This puts pressure on older boards that have limited flash space
remaining. This document provides some tips for reducing the EC image size.

## Checking the EC image footprint

The EC codebase supports two build types:

1. `cros-ec` builds are the legacy EC images built using Make (e.g. `make
   BOARD=volteer`)
1. `zephyr-ec` builds are the EC images built using the Zephyr RTOS kerne using
   zmake/Cmake (e.g. `zmake configure -b zephyr/projects/volteer/volteer`)

### Checking a single cros-ec build

Building a single cros-ec board using `make BOARD=<board> -j` reports the the
number of bytes free in flash and RAM for both the RO and RW images.  An example
from building the juniper board is shown below.

```bash
$ make BOARD=juniper -j
    ...
  *** 668 bytes in flash and 10308 bytes in RAM still available on juniper RO ****
  *** 3224 bytes in flash and 7460 bytes in RAM still available on juniper RW ****
```

### Checking all cros-ec builds

Running `make buildall -j` shows a summary of the three boards with the smallest
RO flash footprint, FW flash footprint, and RW RAM footprint.

```bash
$ make buildall -j
    ...
buildall completed successfully!
Smallest free spaces in RO flash (bytes):
servo_v4  :    104
scarlet   :    108
mushu     :    160
Smallest free spaces in RW flash (bytes):
mushu     :     96
bobba     :    232
trondo    :    376
Tightest boards' RW RAM images, bytes free:
whiskers  :    244
minimuffin:    284
zinger    :    284
```

### Comparing cros-ec image sizes

The cros-ec makefile provides two make targets for helping track the impact of
code changes.

`make savesizes` saves the EC footprint information for all boards, providing
the baseline for comparison.  `make newsizes` compares the sizes of the current
build against the EC footprint information saved by most recent invocation of
`make savesizes`.

General workflow:
1. Run `make buildall -j`.
1. Run `make savesizes`.
1. Apply your code change (e.g. change the local branch, cherry-pick your
   changes, or directly edit source files).
1. Run `make buildall -j` again.
1. Run `make newsizes` to generate report of size changes.

Example report from `make newsizes` shown below:

```bash
$ make newsizes
build/burnet/RO/space_free_flash grew by 576 bytes: (488 to 1064)
build/burnet/RW/space_free_flash grew by 552 bytes: (1324 to 1876)
build/cerise/RO/space_free_flash grew by 512 bytes: (276 to 788)
build/cerise/RW/space_free_flash grew by 548 bytes: (7076 to 7624)
    ...
```

### Checking a single zephyr-ec build

TBD

## Disable console commands

The lowest hanging fruit for reducing the EC image size is by disabling console
commands that provide debug information only and don't impact the user or the
automated testing.  Any console command that is not used by the FAFT tests and
suites is safe to disable in the EC images.

For cros-ec builds, add `#undef CONFIG_CMD_<name>` to the board.h or baseboard.h
file to disable the console command.

For zephyr-ec builds, add `CONFIG_PLATFORM_EC_CONSOLE_CMD_<name>=n` to the board
prj.conf file to disable the console command.

| Used by FAFT | config.h option | Console commands | Notes |
|:---|:---|:---|:---|
|  | CONFIG_CMD_ACCELS | `accelrange`<br>`accelres`<br>`accelrate`<br>`accelread`<br>`accelinit`<br>`accelinfo` | |
|  | CONFIG_CMD_ACCELSPOOF | `accelspoof` | |
| | CONFIG_CMD_ACCEL_FIFO | `fiforead` | |
| | CONFIG_CMD_ACCEL_INFO | `accelinfo` | |
| | CONFIG_CMD_ADC | `adc` | Note firmware_ECAdc uses the `temps` command. |
| | CONFIG_CMD_ALS | `als` | |
| | CONFIG_CMD_APTHROTTLE | `apthrottle` | |
| | CONFIG_CMD_AP_RESET_LOG |??? | |
| | CONFIG_CMD_BATDEBUG | `fgunseal`<br>`fgseal`<br>`fginit`<br>`fgprobe`<br>`fgrd`<br>`fgcmd`<br>`fcmdrd` | |
| | CONFIG_CMD_BATTFAKE | `battfake` | |
| | CONFIG_CMD_BATT_MFG_ACCESS | `battmfgacc` | |
| | CONFIG_CMD_CBI | `cbi` | firmwareECCbiEeprom uses `ectool` on AP to test CBI |
| x | CONFIG_CMD_CHARGEN | `chargen` | Used by firmware_Cr50CCDUartStress, included in faft_ccd, faft_cr50_prepvt, and faft_cr50_pvt suites |
| | CONFIG_CMD_CHARGER | `bd9995x`<br>`sy21612` | |
| | CONFIG_CMD_CHARGER_ADC_AMON_BMON | `amonbmon` | |
| | CONFIG_CMD_CHARGER_DUMP | `charger_dump` | |
| | CONFIG_CMD_CHARGER_PROFILE_OVERRIDE | `fastcharge` | |
| | CONFIG_CMD_CHARGER_PROFILE_OVERRIDE_TEST | `fastchgtest` | |
| | CONFIG_CMD_CHARGE_SUPPLIER_INFO | `chgsup` | |
| | CONFIG_CMD_CHGRAMP | `chgramp` | |
| | CONFIG_CMD_CLOCKGATES | `clockgates` | |
| | CONFIG_CMD_COMXTEST | `comxtest` | |
| | CONFIG_CMD_CRASH | `crash` | |
| | CONFIG_CMD_DEVICE_EVENT | `deviceevent` | |
| | CONFIG_CMD_DLOG | `dlog` | |
| | CONFIG_CMD_ECTEMP | `ectemp` | |
| | CONFIG_CMD_FASTCHARGE | `fastcharge` | Obsolete? use CONFIG_CMD_CHARGER_PROFILE_OVERRIDE? |
| | CONFIG_CMD_FLASH | `flasherase`<br>`flashwrite`<br>`flashread` | |
| | CONFIG_CMD_FLASHINFO | `flashinfo` | |
| | CONFIG_CMD_FLASH_LOG | no longer used | |
| | CONFIG_CMD_FLASH_TRISTATE | `fpcapture`<br>`flash_tristate` | |
| | CONFIG_CMD_FLASH_WP | `flashwp` | |
| | CONFIG_CMD_FORCETIME | `forcetime` | |
| | CONFIG_CMD_FPSENSOR_DEBUG | `fpcapture`<br>`fpenroll`<br>`fpmatch`<br>`fpclear`<br>`fpmaintenance` | |
| | CONFIG_CMD_GETTIME | `gettime` | Used by Cr50 tests, not by FAFT EC |
| | CONFIG_CMD_GL3590 | `gl3590` | |
| | CONFIG_CMD_GPIO_EXTENDED | Adds options to `gpioget` and `gpioset`. | Should be renamed to CONFIG_GPOI_EXTENDED |
| | CONFIG_CMD_GPIO_POWER_DOWN | Not a valid config. | Should be CONFIG_GPIO_POWER_DOWN |
| | CONFIG_CMD_GSV | no longer used | |
| | CONFIG_CMD_GT7288 | `gt7288_desc`<br>`gt7288_repdesc`<br>`gt7288_ver`<br>`gt7288_report` | |
| | CONFIG_CMD_HASH | `hash` | firmware_ECHash uses `ectool echash` |
| x | CONFIG_CMD_HCDEBUG | `hcdebug` | firmware_ECBootTime.py |
| | CONFIG_CMD_HOSTCMD | `hostcmd` | |
| | CONFIG_CMD_I2CWEDGE | `i2cwedge`<br>`i2cunwedge` | |
| | CONFIG_CMD_I2C_PROTECT | `i2cprotect` | |
| | CONFIG_CMD_I2C_SCAN | `i2cscan` | |
| | CONFIG_CMD_I2C_STRESS_TEST | `i2ctest` | |
| | CONFIG_CMD_I2C_STRESS_TEST_ACCEL | Not a console command | |
| | CONFIG_CMD_I2C_STRESS_TEST_ALS | Not a console command | |
| | CONFIG_CMD_I2C_STRESS_TEST_BATTERY | Not a console command | |
| | CONFIG_CMD_I2C_STRESS_TEST_CHARGER | Not a console command | |
| | CONFIG_CMD_I2C_STRESS_TEST_TCPC | `Not a console command | |
| | CONFIG_CMD_I2C_XFER | `i2cxfer` | firmware_ECCbiEeprom uses `ectool i2cxfer` which is not guarded by CONFIG_CMD_I2C_XFER |
| | CONFIG_CMD_I2C_XFER_RAW |  | Adds options to `i2cxfer` |
| | CONFIG_CMD_IDLE_STATS | `idlestats` | |
| | CONFIG_CMD_ILIM | No longer used | |
| | CONFIG_CMD_INA | `ina` | |
| | CONFIG_CMD_JUMPTAGS | `jumptags` | |
| | CONFIG_CMD_KEYBOARD | `8042`<br>`ksstate`<br>`kbpress` | |
| | CONFIG_CMD_LEDTEST | `ledtest` | |
| | CONFIG_CMD_LID_ANGLE | No longer used | |
| | CONFIG_CMD_MCDP | `mcdp` | |
| | CONFIG_CMD_MD | `md` | |
| | CONFIG_CMD_MEM | | Not a console command - gates `md` and `rw` |
| | CONFIG_CMD_MFALLOW | `mfallow` | |
| | CONFIG_CMD_MMAPINFO | `mmapinfo` | |
| x | CONFIG_CMD_PD | `pd` | Used by FAFT PD |
| | CONFIG_CMD_PD_DEV_DUMP_INFO | | Not a console command |
| | CONFIG_CMD_PD_FLASH | `pd flash` | Not supported by TCPMv2 |
| | CONFIG_CMD_PECI | `peci` | firmware_ECThermal uses `ectool tempsinfo` |
| | CONFIG_CMD_PLL | `pll` | only used by lm4 chip |
| | CONFIG_CMD_PMU | no longer used | |
| | CONFIG_CMD_POWERINDEBUG | `powerindebug` | |
| | CONFIG_CMD_POWERLED | `powerled` | |
| x | CONFIG_CMD_POWER_AP | `apreset`<br>`apshutdown` | Used by power_Monitoring.py |
| | CONFIG_CMD_PPC_DUMP | `ppc_dump` | |
| | CONFIG_CMD_PS2 | `ps2ench`<br>`ps2write` | Used only on NPCX |
| | CONFIG_CMD_PWR_AVG | `pwr_avg` | |
| | CONFIG_CMD_RAND | `rand` | Used only on STM32 |
| | CONFIG_CMD_REGULATOR | `ir357x` | |
| | CONFIG_CMD_RESET_FLAGS | `rflags` | |
| | CONFIG_CMD_RETIMER | `bb`<br>`kbxfer` | |
| | CONFIG_CMD_RTC | `rtc` | |
| | CONFIG_CMD_RTC_ALARM | `rtc_alarm` | |
| | CONFIG_CMD_RW | `rw` | |
| | CONFIG_CMD_SCRATCHPAD | `scratchpad` | |
| | CONFIG_CMD_SEVEN_SEG_DISPLAY | `seg` | |
| | CONFIG_CMD_SHA256_TEST | `???` | |
| x | CONFIG_CMD_SHMEM | `shmem` | Used by firmware_ECSharedMem |
| | CONFIG_CMD_SLEEP | `sleep` | used only lm4 |
| | CONFIG_CMD_SLEEPMASK | `sleepmask` | Only used for Cr50 tests |
| | CONFIG_CMD_SLEEPMASK_SET | | Adds options to `sleepmask` |
| | CONFIG_CMD_SPI_FLASH | `spi_flasherase`<br>`spi_flashwrite`<br>`spi_flashread`<br>`spi_flash_rsr`<br>`spi_flash_wsr`<br>`spi_flash_wsr` | |
| | CONFIG_CMD_SPI_NOR | `spinorinfo`<br>`spinorerase`<br>`spinorwrite`<br>`spinorread` | |
| | CONFIG_CMD_SPI_XFER | `spixfer` | |
| | CONFIG_CMD_STACKOVERFLOW | `crash stack` | Adds option to `crash` command. |
| x | CONFIG_CMD_SYSINFO | `sysinfo` | Used by firmware_ECSystemLocked |
| x | CONFIG_CMD_SYSJUMP | `sysjump` | Used by firmware_ECSharedMem |
| | CONFIG_CMD_SYSLOCK | `syslock` | |
| | CONFIG_CMD_TASKREADY | `taskready` | |
| | CONFIG_CMD_TASK_RESET | `taskreset` | |
| | CONFIG_CMD_TCPC_DUMP | `tcpci_dump` | |
| x | CONFIG_CMD_TEMP_SENSOR | `temps` | |
| | CONFIG_CMD_TIMERINFO | `timerinfo` | |
| | CONFIG_CMD_TYPEC | `typec` | |
| | CONFIG_CMD_USART_INFO | `usart_info` | |
| | CONFIG_CMD_USBMUX | No longer used | |
| | CONFIG_CMD_USB_PD_CABLE | `pdcable` | |
|?| CONFIG_CMD_USB_PD_PE | `pe` | Doesn't appear to be used by might be by FAFT PD |
| x | CONFIG_CMD_WAITMS | `waitms` | firmware_ECWatchdog | |

## Reduce or eliminate USB-C debugging

The TCPM (Type-C Port manager) implementation is one of the more complex modules
implemented by the EC code. This module includes extensive debugging and is
enabled by default due to the value provided during both board bringup and on
production systems.

The TCPM provides the following debug levels:
* `DEBUG_DISABLE` (0) - Debugging disabled, no runtime messages displayed
* `DEBUG_LEVEL_1` (1) - Displays all the state transitions for the TC (Type-C)
  and PE (Policy Engine) state machines
* `DEBUG_LEVEL_2` (2) - Displays the raw contents of received PD (Power
  Delivery) packets, excluding PING packets
* `DEBUG_LEVEL_3` (3) - Enables debug messages in the PRL Also displays received
  PING packets.
