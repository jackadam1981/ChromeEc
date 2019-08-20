# Creating a new EC board

This document describes the high-level steps needed to create a new EC board
variant.

[TOC]

## Conventions
### Key Files
Before you get started, it's important to understand the role of a few key files
in the EC codebase.

- [`include/config.h`](../include/config.h) {#config_h} - Contains the list of
  top-level configuration options for the Chrome EC codebase. Each configuration
  option is documented inline and is considered the authoritative definition.

- `baseboard/<name>/` - This directory contains header files and source files
  shared by board variants in a board family.
    - `baseboard.h` - Contains the EC configuration options shared by all
      devices in the board family.
    - `baseboard.c` - Contains code shared by all devices in the board family.
    - `build.mk` - The board family makefile specifies C source files compiled
      into all board variants.

- `board/<board>` - Files in this directory are only built for a single board
  variant.
    - `board.h` - EC configuration options specific to a board variant.
    - `board.c` - Code built only on this board variant.
    - `build.mk` - The board variant makefile defines the EC chipset family,
      defines the baseboard name, and specifies the C source files that are
      compiled.
    - `gpio.inc` - This C header file defines the interrupts, GPIOs, and
      alternate function selection for all pins on the EC chipset.
    - `ec.tasklist` - This C header defines the lists of tasks that are enabled
      on the board.  See the main EC documentation more details on [EC tasks].

### GPIO Naming
Many drivers and libraries in the common EC code rely on board variants defining
an exact GPIO signal name. Examples include the `GPIO_LID_OPEN`,
`GPIO_ENTERING_RW`, and `GPIO_SYS_RESET_L` signals. The net names in schematics
often do not match these names exactly. When this occurs, best practice is that
all the `GPIO_INT()`, `GPIO()`, `ALTERNATE()`, and `UNIMPLEMENTED()` definitions
in `gpio.inc` use the schematic net name. You then create #define macros in
`board.h` to map the net names to the EC common names.

Below is an example configuration for the SYS_RESET_L signal.  The schematic net
name of this signal is EC_RST_ODL and the signal connects to the EC chipset pin
GPIO02.

```c
/* From gpio.inc */
GPIO(EC_RST_ODL,  PIN(0, 2), GPIO_ODR_HIGH)

/* From board.h */
/* Map the schematic net name to the required EC name */
#define GPIO_SYS_RESET_L  GPIO_EC_RST_ODL
```

### How to use this document
Each of the following sections details a single feature set that may need to be
modified or configured for your new board. The features sets are organized so
that they can be implemented with a reasonably sized change list, and can be
worked on independently.

Each feature set includes the following sub-tasks:

- **Config Options** - This section details the `CONFIG_*` options relevant to
  the feature. Use the documentation found in [config.h] to determine whether
  each option should be enabled (using #define) or disabled (using #undef) in
  the relevant `baseboard.h` or `board.h` file.
- **Feature Parameters** - This section details parameters that control the
  operation of the feature. Similar to the config options, feature parameters
  are defined in [config.h] and prefixed with `CONFIG_*`.  However, feature
  parameters are assigned a default value, which can be overridden in by
  `baseboard.h` or `board.h`.
- **GPIOs and Alternate Pins** - This section details signals and pins relevant
  to the feature. Add the required `GPIO_INT()`, `GPIO()`, `ALTERNATE()`, and
  `UNIMPLEMENTED()` definitions to `gpio.inc`, making sure to follow the [GPIO
  naming conventions].
- **Data Structures** - This section details the data structures required to
  configure the feature for correct operation. Add the data structures to
  `baseboard.c` or `board.c`. Note that most data structures required by the
  common EC code should be declared `const` to save on RAM usage.

## Create the EC build target

The first step when creating a new EC board, is to create the required files in
the `./baseboard` and `./board` directories. There are scripts under the
`./util` directory that help copying files from an existing reference board and
for creating a new baseboard from scratch.

### Creating a board variant
If you are creating a board variant from an existing reference board, run the
script `./util/create_variant.sh`. This script copies of all the files from an
existing board variant to a new board variant subdirectory. If your new board
does not use the same EC and AP chipsets as an existing board, it is recommended
that you [create a new baseboard](#Creating-a-new-baseboard).

```bash
# This example creates a new variant board called myboard based on the hatch
# board, referencing bug 140261109
./util/create_variant.sh hatch myboard b:140261109

# After the script completes, check the files, make any necessary edits
# and then build all platforms.
make buildall -j

# Upload your changes for review when you are ready.
repo upload . --cbr
```

### Creating a new baseboard

As an alternative to copying an existing board, you can also create a new
baseboard and board combination from scratch.  Run the script
`./util/create_baseboard.sh` to create a skeleton build, containing the minimum
set of files to successfully compile and run code on an EC chipset.

*TODO - Create ./util/create_baseboard.sh script. https://crbug.com/999705

*TODO* - Reorganize source tree so that variant boards are children
sub-directories of ./baseboard.

## Configure EC Chipset

- **Config options**

    Note that you created a variant board, you can typically skip changing the
    config options related to the SPI flash.

    - `CONFIG_SPI_FLASH_REGS` - Should always be defined when using internal or
      external SPI flash.
    - `CONFIG_SPI_FLASH` - Define only if your board uses an external flash.
    - `CONFIG_SPI_FLASH_<device_type>` - Only used if your board as an external
      flash. Select exactly one the supported flash devices to compile in the
      required driver.
    - Addition EC Chipset options are prefixed with `CONFIG_HIBERNATE*` and
      should be evaluated for relevance on your board.

- **Feature Parameters**

    - `CONFIG_FLASH_SIZE <bytes>` - Set to the size of the internal flash of the
      EC. Must be defined to link the final image.
    - `CONFIG_SPI_FLASH_PORT <port>` - Only used if your board as an external
      flash.

- **GPIOs and Alternate Pins**

    Configure the signals which will wakeup the EC from hibernate or deep sleep.
    Typical wakeup sources include:

    - `GPIO_LID_OPEN` - An active high signal that indicates the lid has been
      opened. The source of the signal is typically from a
      [GMR](./ec_terms.md#gmr) or Hall-Effect sensor. The `GPIO_INT()` entry for
      this signal should be connected to the lid interrupt as shown in the
      example below.
      ```c
      GPIO_INT(LID_OPEN, PIN(D, 2), GPIO_INT_BOTH | GPIO_HIB_WAKE_HIGH, interrupt)
      ```
    - `GPIO_AC_PRESENT` - A signal from the battery charger that indicates the
      device is connected to AC power.
      ```c
      GPIO_INT(AC_PRESENT, PIN(0, 0), GPIO_INT_BOTH | GPIO_HIB_WAKE_HIGH, ower_interrupt)
      ```
    - `GPIO_POWER_BUTTON_L` - An active low signal from the power switch.
      ```c
      GPIO_INT(POWER_BUTTON_L, PIN(0, 1), GPIO_INT_BOTH, r_button_interrupt)
      ```
    - `GPIO_EC_RST_ODL` - On some EC chipsets, low power modes can disable the
      reset pin. In this case, no interrupt handler needs to be registered to
      the signal, but the GPIO pin must still be configured to wake on both edge
      types.
      ```c
      GPIO(EC_RST_ODL, PIN(0, 2), GPIO_INT_BOTH | GPIO_HIB_WAKE_HIGH)
      ```

    If your EC chipset supports PSL (power switch logic), then you should also
    configure the alternate function for all wake pins used.

    ```c
    /* GPIOD2 = EC_LID_OPEN */
    ALTERNATE(PIN_MASK(D, BIT(2)), 0, MODULE_PMU, 0)
    /* GPIO00 = ACOK_OD,
       GPIO01 = H1_EC_PWR_BTN_ODL
       GPIO02 = EC_RST_ODL */
    ALTERNATE(PIN_MASK(0, BIT(0) | BIT(1) | BIT(2)), 0, MODULE_PMU, 0)
    ```

- **Data structures**

    - `const enum gpio_signal hibernate_wake_pins[]` - add all GPIO signals that
      should trigger a wakeup.
    - `const int hibernate_wake_pins_used = ARRAY_SIZE(hibernate_wake_pins);` -
      configures the number of wake signals used on the board.

## Configure AP to EC Communication

- **Config options**

    Configure the AP to EC communication channel, picking exactly one of the
    following options.

    - `CONFIG_HOSTCMD_SPS` - [SPI slave](./ec_terms.md#spi) (SPS) interface
    - `CONFIG_HOSTCMD_HECI` - HECI interface
    - `CONFIG_HOSTCMD_LPC` - [LPC](./ec_terms.md#lpc) bus
    - `CONFIG_HOSTCMD_ESPI` - [eSPI](./ec_terms.md#espi) bus

    In [config.h] search for options that start the same name as your selected
    communication interface.  Override defaults as needed.

- **Feature Parameters**

    None needed in this section.

- **GPIOs and Alternate Pins**

    Create `ALTERNATE()` entries for all EC signals used for AP communication.
    This step can be skipped for any pins that default to communication channel
    functionality.

- **Data structures**

    None needed in this section.

## Configure AP to EC Signals

- **Config options**

    None needed in this section.

- **Feature Parameters**

    None needed in this section.

- **GPIOs and Altnerate Pins**

    The EC code requires the following signals between the AP and the EC to be
    defined by each board variant.

    - `GPIO_EC_INT_L` - Output from the EC, driven low to indicate an event on
      the EC is ready for servicing by the AP. A typical use is when the EC has
      new sensor data to report to the AP.
    - `GPIO_ENTERING_RW` - Output from the EC, driven high to indicate when the
      EC code transitions from RO to RW code.
    - `GPIO_SYS_RESET_L` - Output from the EC, driven low to put the AP into
      reset.

    For boards with an x86 AP, the following signals can be connected between
    the EC and AP/PCH. Create `GPIO()` entries for any signals used on your
    board.

    - `GPIO_PCH_PWRBTN_L` - Output from the EC that gates the status of the EC
      input `GPIO_POWER_BUTTON_L`. Only used when `CONFIG_POWER_BUTTON_X86` is
      defined.
    - `GPIO_PCH_RSMRST_L` - Output from the EC that gates the status of the EC
      input `GPIO_RSMRST_L_PGOOD`.
    - `GPIO_PCH_SLP_S3_L` - Power signal interrupt, asserted low when AP enters
      S3 sleep state.
    - `GPIO_PCH_SLP_S5_L` - Power signal interrupt, asserted low when AP enters
      S5 sleep state.
    - `GPIO_PCH_SYS_PWROK` - Output from the EC that indicates when the system
      power is good and the AP can power up.
    - `GPIO_PCH_WAKE_L` - Output from the EC, driven low when there is a wake
      event.

- **Data structures**

    None needed in this section.

## Configure AP Chipset

- **Config options**

    The AP chipset options are grouped together in [config.h]. Select exactly
    one of the available AP chipset options. If the AP chipset support is not
    available, select `CONFIG_CHIPSET_ECDRIVEN` to enable basic support for
    handling S3 and S0 power states.

    After selecting the chipset, search for additional options that start with
    `CONFIG_CHIPSET*` and evaluate whether each option is appropriate to add to
    `baseboard.h` or `board.h`.

- **Feature Parameters**

    None needed in this section.

- **GPIOs and Altnerate Pins**

    Define a `GPIO_INT()` entry for each power signals defined by
    `power_signal_list[]`. These signals should all be connected to the
    `power_signal_interrupt()`.

    The example below shows the power signals used with Ice Lake processors.

    ```c
    GPIO_INT(SLP_S0_L, PIN(D, 5), GPIO_INT_BOTH, power_signal_interrupt)
    GPIO_INT(SLP_S3_L, PIN(A, 5), GPIO_INT_BOTH, power_signal_interrupt)
    GPIO_INT(SLP_S4_L, PIN(D, 4), GPIO_INT_BOTH, power_signal_interrupt)
    GPIO_INT(PG_EC_ALL_SYS_PWRGD, PIN(F, 4), GPIO_INT_BOTH,   power_signal_interrupt)
    GPIO_INT(PP5000_A_PG_OD, PIN(D, 7), GPIO_INT_BOTH, power_signal_interrupt)
    ```

- **Data Structures**

    - `const struct power_signal_info power_signal_list[]` - This array defines
      the signals from the AP and from the power subsystem on the board that
      define the power state. For some Intel chipsets, including Apollo Lake and
      Ice Lake, this power signal list is defined by the corresponding chipset
      file under the `./power` directory.


## Configure I2C Buses

- **Config options**

    The I2C options are prefixed with `CONFIG_I2C*`. Evaluate whether each
    option is appropriate to add to your board.

    A typical EC and board should at a mimimum set `CONFIG_I2C` and
    `CONFIG_I2C_MASTER`.

- **Feature Parameters**

    The following parameters control the behavior of the I2C library. [config.h]
    defines a reasonable default value, but you may need to change the default
    value for your board.

    - `CONFIG_I2C_CHIP_MAX_READ_SIZE <bytes>`
    - `CONFIG_I2C_NACK_RETRY_COUNT <count>`
    - `CONFIG_I2C_EXTRA_PACKET_SIZE <bytes>` - Only used on STM32 EC's if
      CONFIG_HOSTCMD_I2C_SLAVE_ADDR_FLAGS is defined.

- **GPIOs and Altnerate Pins**

    In the gpio.inc file, you need to define a GPIO for the clock (SCL) and data
    (SDA) pin used on each active I2C bus. The corresponding GPIOs are then
    included in the `i2c_ports[]` array defined below. The is permits the I2C
    library to perform common bus recovery actions without involvement by the EC
    specific I2C device driver.

    You also need to define the alternate function assignment for all I2C pis
    using the `ALTERNATE()` macro.  This step can be skipped for any pins that
    default to I2C functionality.

- **Data Structures**

    - `const struct i2c_port_t i2c_ports[]` - This array should be defined in
      your baseboard.c or board.c file.  This array defines the mapping of
      internal I2C port numbers used by the I2C library to the physical I2C
      ports connected to the EC.
    - `const unsigned int i2c_port_used = ARRAY_SIZE(i2c_ports)` - Defines the
      number of internal I2C ports accessible by the I2C library.

## Configure CrOS Board Information (CBI)

- **Config options** If your board includes an EEPROM to store [CBI], then add
    the following config options to `baseboard.h` or `board.h`.

    - `CONFIG_BOARD_VERSION_CBI`
    - `CONFIG_CROS_BOARD_INFO`

- **Feature Parameters**

    - `I2C_ADDR_EEPROM_FLAGS <7-bit addr>` - Defines the 7-bit slave address for
      the EEPROM containing CBI.

- **GPIOs and Altnerate Pins**

    None needed - the I2C pins should be configured when configuring the I2C
    buses.

- **Data Structures**

    None needed in this section.

## Configure Keyboard

- **Config options**

    Keyboard options start with `CONFIG_KEYBOARD*`. Evaluate whether each option
    is appropriate to add to `baseboard.h` or `board.h`.

    Your board should select only one of these options to configure the protocol
    used to send keyboard events to the AP.

    - `CONFIG_KEYBOARD_PROTOCOL_8042` - Systems with an x86 AP typically use the
      8042 protocol.
    - `CONFIG_KEYBOARD_PROTOCOL_MKBP` - Systems without an x86 AP (e.g. ARM)
      typically use the MKBP protocol.

- **Feature Parameters**

    - `CONFIG_KEYBOARD_KSO_BASE <pin>`

- **GPIOs and Alternate Pins**

    Define `ALTERNATE()` pin entries for all keyboard matrix signals, to connect
    the signals to the keyboard controller of the EC chipset.

    ```c
    /* Example Keyboard pin setup */
    #define GPIO_KB_INPUT (GPIO_INPUT | GPIO_PULL_UP)
    ALTERNATE(PIN_MASK(3, 0x03), 0, MODULE_KEYBOARD_SCAN, GPIO_KB_INPUT) /*      KSI_00-01 */
    ALTERNATE(PIN_MASK(2, 0xFC), 0, MODULE_KEYBOARD_SCAN, GPIO_KB_INPUT) /*      KSI_02-07 */
    ALTERNATE(PIN_MASK(2, 0x03), 0, MODULE_KEYBOARD_SCAN, GPIO_ODR_HIGH) /*      KSO_00-01 */
    ALTERNATE(PIN_MASK(1, 0x7F), 0, MODULE_KEYBOARD_SCAN, GPIO_ODR_HIGH) /*      KSO_03-09 */
    ALTERNATE(PIN_MASK(0, 0xF0), 0, MODULE_KEYBOARD_SCAN, GPIO_ODR_HIGH) /*      KSO_10-13 */
    ALTERNATE(PIN_MASK(8, 0x04), 0, MODULE_KEYBOARD_SCAN, GPIO_ODR_HIGH) /*   KSO_14    */
    ```

- **Data structures**

    - `struct keyboard_scan_config keyscan_config` - This must be defined in if
      the `CONFIG_KEYBOARD_BOARD_CONFIG` option is defined.

- **Additional Notes**

    - If you're including keyboard support, you should also define
      `CONFIG_CMD_KEYBOARD` to enable keyboard debug commands from the EC
      console.
    - `CONFIG_KEYBOARD_PROTOCOL_MKBP` automatically enables `CONFIG_MKBP_EVENT`.
      Boards that enable `CONFIG_KEYBOARD_PROTOCOL_8042` will often also define
      `CONFIG_MKBP_EVENT` for sensor events. Refer to [Configure
      Sensors](#Configure-Sensors) for more information.
    - On Boards that use the H1 secure microcontroller, one KSI (keyboard scan
      input) signal and one KSO (keyboard scan output) signal are routed through
      the H1 microcontroller. There are additional GPIO and configuration
      options that must be enabled in this case.
        - The KSO_02/COL2 signal is always inverted. Explicitly configure the
          GPIO to default low.
          ```c
          GPIO(KBD_KSO2, PIN(1, 7), GPIO_OUT_LOW) /* KSO_02 inverted */
          ```
        - Add the define `CONFIG_KEYBOARD_COL2_INVERTED` to `baseboard.h` or
          `board.h`.
        - If required by the board, define one of the following options to
          configure the KSI pin routed to the H1 microcontroller.
            - `CONFIG_KEYBOARD_PWRBTN_ASSERTS_KSI2`
            - `CONFIG_KEYBOARD_PWRBTN_ASSERTS_KSI3`

## Configure LEDS
Configure the GPIO based platform LEDs.

- **Config options**

    In [config.h] search for options that start with `CONFIG_LED*` and evaluate
    whether each option is appropriate to add to `baseboard.h` or `board.h`.

- **Feature Parameters**

    - `CONFIG_LED_PWM_CHARGE_COLOR <ec_led_color>`
    - `CONFIG_LED_PWM_NEAR_FULL_COLOR <ec_led_color>`
    - `CONFIG_LED_PWM_CHARGE_ERROR_COLOR <ec_led_color>`
    - `CONFIG_LED_PWM_SOC_ON_COLOR <ec_led_color>`
    - `CONFIG_LED_PWM_SOC_SUSPEND_COLOR <ec_led_color>`
    - `CONFIG_LED_PWM_LOW_BATT_COLOR <ec_led_color>`
    - `CONFIG_LED_PWM_COUNT <count>`

- **GPIOs and Alternate Pins**

    Create `GPIO()` entries for all signals that connect to platform LEDs. The
    default state of the pins should be set so that the LED is off (typically
    high output).

- **Data structrures**
    - `struct led_descriptor
      led_bat_state_table[LED_NUM_STATES][LED_NUM_PHASES]` - Must be defined
      when `CONFIG_LED_ONOFF_STATES` is used. Defines the LED states for the
      platform for various charging states.

- **LED Driver Chips**

    LED driver chips are used to control LCD panel backlight. The backlight
    control is separate from the platform LEDs.

## Configure Sensors
*TODO*

## Configure Power Sequencing
*TODO*

## Configure Charger

See the [USB Power Considerations](./usb_power.md) for details on the role of
USB charging on Chromebook.s

## Configure BC1.2 Charger Detector
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

[CBI]: https://chromium.googlesource.com/chromiumos/docs/+/master/design_docs/cros_board_info.md
[config.h]: ./new_board_checklist.md#config_h
[EC tasks]: ../README.md#Tasks
[GPIO naming conventions]: #GPIO-Naming