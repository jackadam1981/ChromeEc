# USB-C on EC codebase

USB-C requires a complex state machine as USB-C can operate in many different
modes. This includes but isn't limited to:

*   Negotiated power contracts. Either side of the cable can source or sink
    power up to 100W (if supported by device).
*   Reversed cable mode. This requires a mux to switch the signals before
    getting to SoC.
*   Multiple modes for the 4 differential pairs high-speed signals including
    *   Data mode (up to 4 lanes for usb data)
    *   Display Port Mode (up to 4 lanes for DisplayPort data)
    *   Dock Mode (2 lanes for usb data, and 2 lanes for DisplayPort)
    *   Audio Accessory mode. (1 lane is used for L and R analog audio signal)

For a more complete list of USB-C feature, see the [USB-C spec][USB Spec Id].

This document covers various touch points to consider for USB-C in the EC
codebase.

[TOC]

## Glossary

| Term  | Definitions                                                          |
| ----- | -------------------------------------------------------------------- |
| TCPC  | Type-C Port Controller. Typically a separate IC connected through    |
:       : I2C, sometimes embedded within EC as a hardware sub module. The TCPC :
:       : interprets physical layer signals on CC lines and Vbus, and sends    :
:       : that information to the TCPM to decide what action to take. In older :
:       : designs that do not have separate TCPC hardware, the EC acted as the :
:       : TCPC as well as the TCPM.                                            :
| TCPM  | Type-C Port Manager. Manages the state of the USB-C connection.      |
:       : Makes decisions about what state to transition to. This is the code  :
:       : running on the EC itself.                                            :
| PE    | Policy Engine. According to the [TypeC spec][USB Spec Id], the       |
:       : policy engine is the state machine that decides how the USB-C        :
:       : connection progresses through differ state and which USB-C PD        :
:       : features are available, such as TrySrc                               :
| TC    | Type-C physical layer.                                               |
| PPC   | Power Path Controller (or Power Protection Controller). An optional, |
:       : separate IC that isolates various USB-C signals from each other and  :
:       : the rest of the board. This IC should prevent shorts and over        :
:       : current/voltage scenarios.                                           :
| SSMUX | Super Speed Mux. This is typically the same IC as the TCPC; it       |
:       : enables the mirrored orientation of the USB-C cable to go to the     :
:       : correct pins on SoC. Also, allows the high-speed signal to be used   :
:       : for different purposes, such as usb data or DisplayPort.             :
| DRP   | Dual Role Port. A USB-C port that can act as either a power Source   |
:       : or power Sink.                                                       :
| UFP   | Upward Facing Port. The USB data role that is typical for a          |
:       : peripheral (e.g. HID keyboard).                                      :
| DFP   | Downward Facing Port. The USB Data role that is typical for a host   |
:       : machine (e.g. device running ChromeOS).                              :

## Different PD stacks

Right now platform/ec has two different implementation of USB-C PD stack.

1.  The older implementation is mainly contained within `usb_protocol.c`
2.  The newer implementation is broken up into multiple different files and
    state machines
    *   Policy engine state machine files, such as `usb_pe_*_sm.c`.
    *   Type-C physical layer state machine: `usb_tc_sm.c`

The older implementation supports firmware for devices types other than
Chromebooks. For example, the older stack supports the Zinger, which is the
USB-C charging device that shipped with the Samus Chromebook. The Zinger
implements the charger only side of the USB PD protocol.

The newer implementation only supports Chromebooks at the moment. There are
multiple policy engine definitions to chose from depending on the different
USB-C features the Chromebook should support. As of now, you must choose one and
only one policy engine state machine implementation. The policy engine mostly
defines what PD features (e.g. TrySrc) are implemented on the Chromebook.

## Implementation Considerations

In both older and newer implementations, the following details apply:

*   For each USB-C port, there must be two tasks: `PD_C#` and `PD_INT_C#`, where
    `#` is the port number starting from `0`.
    *   The `PD_C#` task runs the state machine (old or new) for the port and
        communicates with the TCPC, MUX, and PPC. This tasks needs a large task
        space.
    *   The `PD_INT_C#` tasks runs at a higher priority than the state machine
        task, and its soles job it to receive interrupts from the TCPC as quick
        as possible then send appropriate messages to other tasks (including
        `PD_C#`). This task shouldn't need much task space, but the i2c recovery
        code requires a decent amount of task space so it ends up needing a fair
        amount too.
*   Saving PD state between EC jumps
    *   PD communication is disabled in locked RO images (normal state for
        customer devices). When the jump from RO to RW happens relatively
        quickly (e.g. there is not a long memory training step), then there
        aren't many problems when RW takes over and negotiates higher PD
        contracts.
    *   To support factory use cases that don't have a battery (and are
        therefore unlocked), PD communication is enabled in unlocked RO. This
        allows systems without software sync enabled to get a higher power
        contract than 15W in RO.
    *   We save and restore PD state between RO -> RW and RW -> RO jump to allow
        us to maintain a higher negotiated power through the full jump and
        re-initialization process. For example, for each port we save the power
        role, data role, and Vconn sourcing state in battery-backed or
        non-volatile RAM. This allows the firmware image that is initializing to
        restore the USB state without cutting power by performing a SoftReset
        (leaves Vbus intact) instead of a HardReset (drops Vbus).
    *   Both use cases where we actually are able to restore the PD contract
        require an unlocked RO (e.g. factory) otherwise RO cannot communicate
        via PD and will drop the higher PD contract.
        *   The RO->RW use case is for an unlocked (e.g. factory) device that
            negotiated power and we want to keep that contract after we jump to
            RW in the normal software sync boot process
        *   The RW->RO use case happens when we are performing auxiliary FW
            upgrades during software sync and BIOS instructs the EC to jump back
            to RO. If RO is unlocked, we will try to maintain the existing power
            contract.

## Configuration

There are many `CONFIG_*` options and driver structs that are needed in the
board.h and board.c implementation.

### TCPC Config

The `tcpc_config` array of `tcpc_config_t` structs defined in `board.c` (or
baseboard equivalent) should be defined for every board. The index in the
`tcpc_config` array corresponds to the USB-C port number. This struct should
point to the specific TCPC driver that corresponds to the TCPC that is being
used on that port. The i2c port and address for the TCPC are also specified
here.

### SSMUX Config

The `usb_muxes` array of `usb_mux` structs defined in `board.c` (or baseboard
equivalent) should be defined for every board. Normally the standard
`tcpci_tcpm_usb_mux_driver` driver works, especially if TCPCP and MUX are the
same IC.

If the signal strength for the high-speed data lines needs to be tuned for a
specific hardware layout, the `board_init` field on the `usb_mux` is called
every time the mux is woken up from a low power state and should be used for
setting custom board tuning parameters.

### PPC Config

Some boards have an additional IC that sits between the physical USB-C connector
and the rest of the board. This PPC IC provide over voltage and over current
protection on multiple USB-C pins. If present, the PPC IC will gate whether the
Vbus line is an input or output signal, based on i2c messages or gpio pins.

The `ppc_chips` array of `ppc_config_t` structs defined in `board.c` (or
baseboard equivalent) sets the appropriate driver and i2c port/address for the
PPC IC.

### Useful Config Options

Many USB-C policies and features are gates by various `CONFIG_*` options that
should be defined in `board.h` (or baseboard equivalent).

To use the newer USB-C PD stack implementation define *
`CONFIG_USB_SM_FRAMEWORK` * `CONFIG_USB_TYPEC_SM` * `CONFIG_USB_PRL_SM` * And
one of `CONFIG_USB_PE_*` policy engine options

Common USB-C options with short description of purpose

| Option                                  | Purpose                            |
| --------------------------------------- | ---------------------------------- |
| CONFIG_USB_PD_PORT_COUNT                | The number of USB-C ports on a     |
:                                         : board. This is also the array size :
:                                         : for `tcpc_config`, `usb_muxes`,    :
:                                         : and `ppc_chips`.                   :
| CONFIG_USB_POWER_DELIVERY               | Device will speak USB-C PD for     |
:                                         : power. Should be defined.          :
| CONFIG_USB_PD_MAX_SINGLE_SOURCE_CURRENT | Define if the boards supports      |
:                                         : sourcing more power to a           :
:                                         : peripheral device when only one    :
:                                         : USB-C port is in use. For example, :
:                                         : if a device is sourcing power out  :
:                                         : on all of the USB-C ports, then    :
:                                         : the max power for each power is    :
:                                         : most likely 7.5W (5V @ 1.5A). The  :
:                                         : board can decide that it is        :
:                                         : willing to source 15W (5V @ 3A) to :
:                                         : a single peripheral if only a      :
:                                         : single peripheral is attached.     :
| CONFIG_USB_PD_DUAL_ROLE                 | If the device support being a      |
:                                         : power source or sink, i.e. it is a :
:                                         : DRP device.                        :
| CONFIG_USB_PD_ALT_MODE                  | Device support alternate modes     |
:                                         : such as DisplayPort.               :
| CONFIG_USB_PD_ALT_MODE_DFP              | Device supports being a DFP port.  |
:                                         : Needed for DisplayPort to work.    :
| CONFIG_USBC_SS_MUX                      | Device has a SS MUX driver.        |
:                                         : Requires `usb_muxes` to be defined :
:                                         : in `board.c` or equivalent.        :
| CONFIG_USBC_VCONN                       | Device supports sourcing the Vconn |
:                                         : line (in addition to Vbus). Only   :
:                                         : makes sense for boards that can    :
:                                         : act as a power source.             :
| CONFIG_USBC_VCONN_SWAP                  | Device supports swapping Vconn     |
:                                         : between port partners (only        :
:                                         : applies to Vconn enabled and DRP   :
:                                         : devices).                          :
| CONFIG_USB_PD_TRY_SRC                   | Enabled the TrySrc logic/feature   |
:                                         : within the PD state machine. If    :
:                                         : enabled, the device will try to be :
:                                         : a power source when the AP is in   :
:                                         : S0. This is useful because the     :
:                                         : default data role for source is    :
:                                         : UFP, which is appropriate for a    :
:                                         : host USB controller.               :
| CONFIG_USB_PD_TCPC_LOW_POWER            | Enables the TCPC to go into a low  |
:                                         : power state when nothing is        :
:                                         : attached the the USB-C port.       :
| CONFIG_USB_PD_DUAL_ROLE_AUTO_TOGGLE     | Enables TCPCs that support a low   |
:                                         : power auto-toggle mode to use that :
:                                         : feature. Requires                  :
:                                         : CONFIG_USB_PD_TCPC_LOW_POWER to be :
:                                         : defined as well.                   :
| CONFIG_USB_PD_VBUS_DETECT_*             | * is one of TCPC, CHARGER, GPIO,   |
:                                         : PPC, or NONE. Choosing one of      :
:                                         : these options defines which        :
:                                         : component will inform the USB-PD   :
:                                         : state machine that Vbus is present :
:                                         : or not. The correct choice depends :
:                                         : on the HW design.                  :

## Interactions with other tasks

TODO: mention `USB_CHG_P#` and `CHARGER`

## Upgrading FW for TCPCs

TODO: Mention how this works even though it is in depthcharge. Probing now. Need
new driver in depthcharge

[USB Spec Id]: https://www.usb.org/document-library/usb-32-specification-released-september-22-2017-and-ecns
