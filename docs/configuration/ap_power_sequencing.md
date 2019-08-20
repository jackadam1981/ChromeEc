# Configure AP Power Sequencing

This section details the configuration related to managing the system power
states (G3, S5, S3, S0, S0iX, etc). This includes the following tasks:

- Selecting the AP chipset type.
- Configure output GPIOs that enable voltage rails.
- Configure input GPIOs that monitor the voltage rail status (power good
  signals).
- Configure input GPIOs that monitor the AP sleep states.
- Pass through power sequencing signals from the board to the AP, often with
  delays or other sequencing control.

## Config options

The AP chipset options are grouped together in [config.h]. Select exactly one of
the available AP chipset options (e.g. `CONFIG_CHIPSET_APOLLOLAKE`,
`CONFIG_CHIPSET_BRASWELL`, etc). If the AP chipset support is not available,
select `CONFIG_CHIPSET_ECDRIVEN` to enable basic support for handling S3 and S0
power states.

After selecting the chipset, search for additional options that start with
`CONFIG_CHIPSET*` and evaluate whether each option is appropriate to add to
`baseboard.h` or `board.h`.

Finally, evaluate the `CONFIG_POWER_` options for use on your board. In
particular, the `CONFIG_POWER_BUTTON`, and `CONFIG_POWER_COMMON` should be
defined.

## Feature Parameters

None needed in this section.

## GPIOs and Alternate Pins

### EC Outputs to the board

The board should connect the enable signal of one or more voltage rails to the
EC. These enable signals will vary based on the AP type, but are typically
active high signals. For Intel Ice Lake chipsets, this includes enable signals
for the primary 3.3V and primary 5V rails.

```c
GPIO(EN_PP3300_A, PIN(A, 3), GPIO_OUT_LOW)
GPIO(EN_PP5000,   PIN(A, 4), GPIO_OUT_LOW)
```

### EC Outputs to AP

For boards with an x86 AP, the following signals can be connected between the EC
and AP/PCH. Create `GPIO()` entries for any signals used on your board.

- `GPIO_PCH_PWRBTN_L` - Output from the EC that gates the status of the EC input
  `GPIO_POWER_BUTTON_L`. Only used when `CONFIG_POWER_BUTTON_X86` is defined.
- `GPIO_PCH_RSMRST_L` - Output from the EC that gates the status of the EC input
  `GPIO_RSMRST_L_PGOOD`.
- `GPIO_PCH_SYS_PWROK` - Output from the EC that indicates when the system power
  is good and the AP can power up.
- `GPIO_PCH_WAKE_L` - Output from the EC, driven low when there is a wake event.

### Power Signal Interrupts

Each power signal defined in the `power_signal_list[]` array, define a
`GPIO_INT()` entry that connects to the `power_signal_interrupt`. The interrupts
are configured to trigger on both rising edge and falling edge.

The example below shows the power signals used with Ice Lake processors.

```c
GPIO_INT(SLP_S0_L, PIN(D, 5), GPIO_INT_BOTH, power_signal_interrupt)
GPIO_INT(SLP_S3_L, PIN(A, 5), GPIO_INT_BOTH, power_signal_interrupt)
GPIO_INT(SLP_S4_L, PIN(D, 4), GPIO_INT_BOTH, power_signal_interrupt)
GPIO_INT(PG_EC_ALL_SYS_PWRGD, PIN(F, 4), GPIO_INT_BOTH,   power_signal_interrupt)
GPIO_INT(PP5000_A_PG_OD, PIN(D, 7), GPIO_INT_BOTH, power_signal_interrupt)
```

See the [GPIO](./gpio.md) documentation for additional details on
the GPIO macros.

## Data structures

- `const struct power_signal_info power_signal_list[]` - This array defines the
  signals from the AP and from the power subsystem on the board that control the
  power state. For some Intel chipsets, including Apollo Lake and Ice Lake, this
  power signal list is already defined by the corresponding chipset file under
  the `./power` directory.

## Tasks

The `CHIPSET` task monitors and handles the power state changes.  This task
should always be enabled with a priority higher than the `CHARGER` task, but
lower than the `HOSTCMD` and `CONSOLE` tasks.

```c
    TASK_NOTEST(CHIPSET, chipset_task, NULL, LARGER_TASK_STACK_SIZE) \
```

The `POWERBTN` and task should be enabled when using x86 based AP chipsets. The
typical priority is higher than the `CONSOLE` task, but lower than the `KEYSCAN`
task.

```c
    TASK_ALWAYS(POWERBTN, power_button_task, NULL, LARGER_TASK_STACK_SIZE) \
```

## Testing and Debugging

During the first power on of prototype devices, it is recommended to enable
`CONFIG_BRINGUP`. This option prevents the EC from automatically powering on the
AP. You can use the EC console commands `gpioget` and `gpioset` to manually
check power good signals and enabled power rails in a controlled manner. This
option also enables extra debug to log all power signal transitions to the EC
console. With `CONFIG_BRINGUP` enabled, you can trigger the automatic power
sequencing by running the `powerbtn` from the EC console.

*TODO ([b/147808790](http://issuetracker.google.com/147808790)) Add
documentation specific to each x86 processor type.*
