# Creating a new EC board

This document describes the high-level steps needed to create a new EC board
variant.

[TOC]

## Conventions
### Key Files
Before you get started, it's important to understand the role of a few key files
in the EC codebase.
- `include/config.h` - Contains the list of top-level configuration options for
  the Chrome EC codebase. Each configuration option is documented inline and is
  considered the authoritative definition.

- `baaseboard/<board>/baseboard.h` - Contains the EC configuration options
  shared by all devices in the board family.

- `baseboard/<board>/baseboard.c` - Contains code shared by all devices in the
  board family.

- `board/<board>/board.h` - EC configuration options specific to a board
  variant.

- `board/<board>/board.c` - Code built only on this board variant.

- `board/<board>/gpio.inc` - This file defines the interrupts, GPIOs, and
  alternate function selection for all pins on the EC chipset.

### GPIO Naming
Many drivers and libraries in the common EC code rely on board variants defining
an exact GPIO signal name. Examples include the `GPIO_LID_OPEN`,
`GPIO_ENTERING_RW`, and `GPIO_SYS_RESET_L` signals. The net names in schematics
often do not match these names exactly. When this occurs, best practice is that
all the `GPIO_INT()`, `GPIO()`, `ALTERNATE()`, and `UNIMPLEMENTED()` definitions
in `gpio.inc` use the schematic net name. You then create #define macros in
`board.h` to map the net names to the EC common names.

Below is an example configuration for the SYS_RESET_L signal.  The schematic net
name of this signal is EC_RST_ODL, and the signal connects to the EC chipset pin
GPIO02.

```c
/* From gpio.inc */
GPIO(EC_RST_ODL,  PIN(0, 2), GPIO_ODR_HIGH)

/* From board.h */
/* Map the schematic net name to the required EC name */
#define GPIO_SYS_RESET_L  GPIO_EC_RST_ODL
```

## Copy the skeleton build template

When creating a new baseboard and board combination, there is a skeleton build
template available to get you started.

The following steps assume you have entered your chroot and set the current
directory to the EC base directory (`~/trunks/src/platform/ec`).

In this example, the you will create a new baseboard and board combination
called "mybaseboard" and "myboard", respectively.

```bash
# Create a local branch
repo start create-myboard .

# Copy the baseboard template
cp -r ./baseboard/ec_template ./baseboard/mybaseboard
# Edit all files under ./baseboard/mybaseboard, search and replace "ec_template" with "mybaseboard"

# Copy the board template
cp -r ./board/ec_template ./board/myboard
# Edit all files under ./board/myboard, search and replace "ec_template" with "myboard"

# Add your changes to the git staging area
git add ./baseboard/mybaseboard
git add ./board/myboard

# Verify your files
git st
    On branch create-myboard
    Your branch is up to date with 'cros/master'.
Changes to be committed:
  (use "git restore --staged <file>..." to unstage)
        new file:   baseboard/mybaseboard/baseboard.c
        new file:   baseboard/mybaseboard/baseboard.h
        new file:   baseboard/mybaseboard/build.mk
        new file:   board/myboard/board.c
        new file:   board/myboard/board.h
        new file:   board/myboard/build.mk
        new file:   board/myboard/ec.tasklist
        new file:   board/myboard/gpio.inc

# Verify your changes build
make BOARD=myboard -j

# Commit your changes locally
git commit

# Build remaining board types - this is a prerequisite to committing your change to gerrit
make buildall -j

# Push your changes to gerrit for review
repo upload . --cbr
```


*TODO* - Create a script that performs all the steps above, except for the repo
upload step.

*TODO* - Reorganize source tree so that variant boards are children
sub-directories of ./baseboard.

## Configure EC Chipset

- **Configure flash configuration** - The EC template files are based on a
  Nuvoton NPCX7 chipset, using an internal 512 KiB flash. Modify the following
  options in `baseboard.h` as needed.
  - `CONFIG_FLASH_SIZE <bytes>` - Set to the size of the internal flash of the
    EC. Must be defined to link the final image.
  - `CONFIG_SPI_FLASH_REGS` - Should always be defined when using internal or
    external SPI flash.
  - If you are using an external flash device, configure these options:
    - `CONFIG_SPI_FLASH`
    - `CONFIG_SPI_FLASH_PORT <port>`
    - `CONFIG_SPI_FLASH_<device_type>`

  *TODO* - Document options for changing the EC chipset type.

- **GPIOs and Altnerate Pins** - Configure the signals which will wakeup the EC
   from hibernate or deep sleep.Typical wakeup sources include:
  - `GPIO_LID_OPEN` - An active high signal that indicates the lid has been
    opened. The source of the signal is typically from a [GMR](./ec_terms#gmr)
    or Hall-Effect sensor. The `GPIO_INT()` entry for this signal should be
    connected to the lid interrupt as shown in the example below.
    ```c
    GPIO_INT(LID_OPEN, PIN(D, 2), GPIO_INT_BOTH | GPIO_HIB_WAKE_HIGH, lid_interrupt)
    ```
  - `GPIO_AC_PRESENT` - A signal from the battery charger that indicates the
    device is connected to AC power.
    ```c
    GPIO_INT(AC_PRESENT, PIN(0, 0), GPIO_INT_BOTH | GPIO_HIB_WAKE_HIGH, extpower_interrupt)
    ```
  - `GPIO_POWER_BUTTON_L` - An active low signal from the power switch.
    ```c
    GPIO_INT(POWER_BUTTON_L, PIN(0, 1), GPIO_INT_BOTH, power_button_interrupt)
    ```
  - `GPIO_SYS_RESET_L` - An active low signal that resets the EC.

- **Data structures**
  - `const enum gpio_signal hibernate_wake_pins[]` - add all GPIO signals that
    should trigger a wakeup.

## Configure AP to EC Communication

Configure the AP to EC communication channel, picking exactly one of the
following options.
- `CONFIG_HOSTCMD_SPS` - [SPI slave](./ec_terms.md#spi) (SPS) interface
- `CONFIG_HOSTCMD_HECI` - HECI interface
- `CONFIG_HOSTCMD_LPC` - [LPC](./ec_terms.md#lpc) bus
- `CONFIG_HOSTCMD_ESPI` - [eSPI](./ec_terms.md#espi) bus

In `include/config.h` search for options that start the same name as your
selected communication interface.  Override defaults as needed.

## Configure AP to EC Signals
The EC code requires the following signals between the AP and the EC to be
defined by each board variant.
- `GPIO_ENTERING_RW` - Output from the EC, driven high to indicate when the EC
  code transitions from RO to RW code.
- `GPIO_SYS_RESET_L` - Output from the EC, driven low to put the AP into reset.
- `GPIO_PCH_WAKE_L` - Used on boards using the eSPI or LPC host interface.
  Output from the EC, driven low when there is a wake event.
- `GPIO_PCH_PWRBTN_L` - Used with CONFIG_POWER_BUTTON_X86.

## Configure AP Chipset

- **Config options** - Select exactly one of the following config options to
  configure the power interface for the AP chipset. If the AP chipset support is
  not available, select the `CONFIG_CHIPSET_ECDRIVEN` to enable basic support
  for handling S3 and S0 power states.
  - `CONFIG_CHIPSET_APOLLOLAKE`
  - `CONFIG_CHIPSET_BRASWELL`
  - `CONFIG_CHIPSET_CANNONLAKE`
  - `CONFIG_CHIPSET_COMETLAKE`
  - `CONFIG_CHIPSET_ECDRIVEN`
  - `CONFIG_CHIPSET_GEMINILAKE`
  - `CONFIG_CHIPSET_ICELAKE`
  - `CONFIG_CHIPSET_MT817X`
  - `CONFIG_CHIPSET_MT8183`
  - `CONFIG_CHIPSET_RK3288`
  - `CONFIG_CHIPSET_RK3399`
  - `CONFIG_CHIPSET_SKYLAKE`
  - `CONFIG_CHIPSET_SDM845`
  - `CONFIG_CHIPSET_STONEY`

  After selecting the chipset, search for options that start with
  `CONFIG_CHIPSET*` and evaluate whether each option is appropriate to add to
  `baseboard.h` or `board.h`.

- **Data Structures**
  - `const struct power_signal_info power_signal_list[]` - This array defines
    the signals from the AP and from the power subsystem on the board that
    define the power state. For some Intel chipsets, including Apollo Lake and
    Ice lake, this power signal list is defined by the corresponding chipset
    file under the `./power` directory.

- **GPIOs and Altnerate Pins** - In the gpio.inc file, you need to define a
  `GPIO_INT()` entry for each power signals defined by `power_signal_list[]`.
  These signals should all be connected to the `power_signal_interrupt()`.
  Please follow the [GPIO naming conventions](#GPIO-Naming) for power signals
  defined by the chipset.

## Configure I2C Buses

- **Config options** - In `include/config.h` search for options that start with
  `CONFIG_I2C*` and evaluate whether each option is appropriate to add to
  `baseboard.h` or `board.h`.

  A typical EC and board should at a mimimum set `CONFIG_I2C` and
  `CONFIG_I2C_MASTER`.

- **Library Parameters** -  The following parameters control the behavior of the
  I2C library. config.h defines a reasonable default value, but you may need to
  change the default value for your board.

  - `CONFIG_I2C_CHIP_MAX_READ_SIZE <bytes>`
  - `CONFIG_I2C_NACK_RETRY_COUNT <count>`
  - `CONFIG_I2C_EXTRA_PACKET_SIZE <bytes>` - Only used on STM32 EC's if
    CONFIG_HOSTCMD_I2C_SLAVE_ADDR_FLAGS is defined.

- **GPIOs and Altnerate Pins** - In the gpio.inc file, you need to define a GPIO
  for the clock (SCL) and data (SDA) pin used on each active I2C bus. The
  corresponding GPIOs are then included in the `i2c_ports[]` array defined
  below. The is permits the I2C library to perform common bus recovery actions
  without involvement by the EC specific I2C device driver.

  You also need to define the alternate function assignment for all I2C pis
  using the `ALTERNATE()` macro.  This step can be skipped for any pins that
  default to I2C functionality.

- **Data Structures**
  - `const struct i2c_port_t i2c_ports[]` - This array should be defined in your
    baseboard.c or board.c file.  This array defines the mapping of internal I2C
    port numbers used by the I2C library to the physical I2C ports connected to
    the EC.
  - `const unsigned int i2c_port_used = ARRAY_SIZE(i2c_ports)` - Defines the
    number of internal I2C ports accessible by the I2C library.

## Configure CrOS Board Information (CBI) {#config_cbi}

If your board includes an EEPROM to store [CBI](./ec_terms.md#cbi), then add the
following config options to `baseboard.h` or `board.h`.

- `CONFIG_BOARD_VERSION_CBI`
- `CONFIG_CROS_BOARD_INFO`
- `I2C_ADDR_EEPROM_FLAGS <7-bit addr>` - Defines the 7-bit slave address for the
  EEPROM containing CBI.

## Configure Keyboard
TODO: Incorporate information described in this
[bug](https://crbug.com/136284831).


- **Config options** - In `include/config.h` search for options that start with
  `CONFIG_KEYBOARD*` and evaluate whether each option is appropriate to add to
  `baseboard.h` or `board.h`.

  Your board should select only one of these options to configure the protocol
  used to send keyboard events to the AP.
    - `CONFIG_KEYBOARD_PROTOCOL_8042` - Systems with an x86 AP typically use the
      8042 protocol.
    - `CONFIG_KEYBOARD_PROTOCOL_MKBP` - Systems without an x86 AP (e.g. ARM)
      typically use the MKBP protocol.

  If you're including keyboard support, you should also define
  `CONFIG_CMD_KEYBOARD` to enable keyboard debug commands from the EC console.

- **Library parameters**
    - `CONFIG_KEYBOARD_KSO_BASE <pin>`

- **GPIOs and Altnerate Pins** - Define `ALTERNATE()` pin entries for all
  keyboard matrix signals, to connect the signals to the keyboard controller of
  the EC chipset.

```c
/* Example Keyboard pin setup */
#define GPIO_KB_INPUT (GPIO_INPUT | GPIO_PULL_UP)
ALTERNATE(PIN_MASK(3, 0x03), 0, MODULE_KEYBOARD_SCAN, GPIO_KB_INPUT) /* KSI_00-01 */
ALTERNATE(PIN_MASK(2, 0xFC), 0, MODULE_KEYBOARD_SCAN, GPIO_KB_INPUT) /* KSI_02-07 */
ALTERNATE(PIN_MASK(2, 0x03), 0, MODULE_KEYBOARD_SCAN, GPIO_ODR_HIGH) /* KSO_00-01 */
ALTERNATE(PIN_MASK(1, 0x7F), 0, MODULE_KEYBOARD_SCAN, GPIO_ODR_HIGH) /* KSO_03-09 */
ALTERNATE(PIN_MASK(0, 0xF0), 0, MODULE_KEYBOARD_SCAN, GPIO_ODR_HIGH) /* KSO_10-13 */
ALTERNATE(PIN_MASK(8, 0x04), 0, MODULE_KEYBOARD_SCAN, GPIO_ODR_HIGH) /* KSO_14 */
```

- **Data structures**
    - `struct keyboard_scan_config keyscan_config` - This must be defined in if
      the `CONFIG_KEYBOARD_BOARD_CONFIG` option is defined.

## Configure LEDS
Configure the GPIO based platform LEDs.

- **Config options** - In `include/config.h` search for options that start with
  `CONFIG_LED*` and evaluate whether each option is appropriate to add to
  `baseboard.h` or `board.h`.

- **Library parameters**
    - `CONFIG_LED_PWM_CHARGE_COLOR <ec_led_color>`
    - `CONFIG_LED_PWM_NEAR_FULL_COLOR <ec_led_color>`
    - `CONFIG_LED_PWM_CHARGE_ERROR_COLOR <ec_led_color>`
    - `CONFIG_LED_PWM_SOC_ON_COLOR <ec_led_color>`
    - `CONFIG_LED_PWM_SOC_SUSPEND_COLOR <ec_led_color>`
    - `CONFIG_LED_PWM_LOW_BATT_COLOR <ec_led_color>`
    - `CONFIG_LED_PWM_COUNT <count>`

- **Data structrures**
    - `struct led_descriptor
      led_bat_state_table[LED_NUM_STATES][LED_NUM_PHASES]` - Must be defined
      when `CONFIG_LED_ONOFF_STATES` is used. Defines the LED states for the
      platform for various charging states.

- **LED Driver Chips** - LED driver chips are used to control LCD panel
  backlight. The backlight control is separate from the platform LEDs.

## Configure Sensors
*TODO*

## Configure Power Sequencing
*TODO*

## Configure Charger
*TODO*

## Configure Battery
*TODO*

## Configure USB-C
*TODO*

## Configure PD
*TODO*

## Configure PPC
*TODO*

## Configure TCPC
*TODO*






