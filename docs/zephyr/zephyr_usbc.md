# Zephyr EC USBC Configuration

[TOC]

## Overview

[USBC] enables the configuration of a Type-C port for a wide range of computing, display, and charging applications.

## Kconfig Options

Kconfig Option                                                   | Default | Documentation
:--------------------------------------------------------------- | :-----: | :------------
`CONFIG_PLATFORM_EC_USBC`                                        | y       | [EC USBC]

The following options are available only when `CONFIG_PLATFORM_EC_USBC=y`.

Kconfig sub-option

`CONFIG_PLATFORM_EC_CHARGER_INPUT_CURRENT`                       | 512    | [USBC ]
`CONFIG_PLATFORM_EC_BOOT_AP_POWER_REQUIREMENTS`                  | y      | [USBC ]
`CONFIG_PLATFORM_EC_CHARGER_MIN_BAT_PCT_FOR_POWER_ON`            | 3      | [USBC ]
`CONFIG_PLATFORM_EC_CHARGER_MIN_BAT_PCT_FOR_POWER_ON_WITH_AC`    | 1      | [USBC ]
`CONFIG_PLATFORM_EC_CHARGER_MIN_POWER_MW_FOR_POWER_ON_WITH_BATT` | 15000  | [USBC ]
`CONFIG_PLATFORM_EC_CHARGER_MIN_POWER_MW_FOR_POWER_ON`           | 15000  | [USBC ]
`CONFIG_PLATFORM_EC_USBC_OCP`                                    | y      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PID`                                     | 0      | [USBC ]
`CONFIG_PLATFORM_EC_USB_BCD_DEV`                                 | 0      | [USBC ]
`CONFIG_PLATFORM_EC_USB_VID`                                     | 0x18d1 | [USBC ]
`CONFIG_PLATFORM_EC_USB_MS_EXTENDED_COMPAT_ID_DESCRIPTOR`        | n      | [USBC ]
`CONFIG_PLATFORM_EC_USBC_RETIMER_INTEL_BB`                       | n      | [USBC ]
`CONFIG_PLATFORM_EC_USBC_RETIMER_INTEL_BB_RUNTIME_CONFIG`        | y      | [USBC ]
`CONFIG_PLATFORM_EC_USBC_RETIMER_ANX7451`                        | n      | [USBC ]
`CONFIG_PLATFORM_EC_USBC_RETIMER_PS8811`                         | n      | [USBC ]
`CONFIG_PLATFORM_EC_USBC_RETIMER_PS8818`                         | n      | [USBC ]
`CONFIG_PLATFORM_EC_USBC_RETIMER_KB800X`                         | n      | [USBC ]
`CONFIG_PLATFORM_EC_KB800X_CUSTOM_XBAR`                          | n      | [USBC ]
`CONFIG_PLATFORM_EC_USB_POWER_DELIVERY`                          | y      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_INT_SHARED`                           | n      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_PORT_0_SHARED`                        | n      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_PORT_1_SHARED`                        | n      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_PORT_2_SHARED`                        | n      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_PORT_3_SHARED`                        | n      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_HOST_CMD`                             | y      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_PORT_MAX_COUNT`                       | 2      | [USBC ]
`CONFIG_PLATFORM_EC_CONSOLE_CMD_MFALLOW`                         | y      | [USBC ]
`CONFIG_PLATFORM_EC_CONSOLE_CMD_PD`                              | y      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_DEBUG_FIXED_LEVEL`                    | n      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_DEBUG_LEVEL`                          | 0      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_5V_EN_CUSTOM`                         | n      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_5V_CHARGER_CTRL`                      | n      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_VBUS_MEASURE_NOT_PRESENT`             | n      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_VBUS_MEASURE_CHARGER`                 | n      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_VBUS_MEASURE_TCPC`                    | n      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_VBUS_MEASURE_ADC_EACH_PORT`           | n      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_VBUS_MEASURE_BY_BOARD`                | n      | [USBC ]
`CONFIG_PLATFORM_EC_USBC_VCONN`                                  | y      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_DUAL_ROLE`                            | y      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_FRS`                                  | n      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_FRS_PPC`                              | n      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_FRS_TCPC`                             | n      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_DPS`                                  | n      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_DUAL_ROLE_AUTO_TOGGLE`                | n      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_DISCHARGE`                            | y      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_DISCHARGE_GPIO`                       | y      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_DISCHARGE_TCPC`                       | y      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_DISCHARGE_PPC`                        | y      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_REV30`                                | y      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_ALT_MODE`                             | y      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_REQUIRE_AP_MODE_ENTRY`                | y      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_ALT_MODE_DFP`                         | y      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_ALT_MODE_UFP`                         | n      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_USB32_DRD`                            | y      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_DP_HPD_GPIO`                          | n      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_DP_HPD_GPIO_CUSTOM`                   | n      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_DATA_RESET_MSG`                       | n      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_VBUS_DETECT_NONE`                     | n      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_VBUS_DETECT_TCPC`                     | n      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_VBUS_DETECT_CHARGER`                  | n      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_VBUS_DETECT_PPC`                      | n      | [USBC ]
`CONFIG_PLATFORM_EC_USB_TYPEC_SM`                                | y      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PRL_SM`                                  | y      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PE_SM`                                   | y      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_DECODE_SOP`                           | y      | [USBC ]
`CONFIG_PLATFORM_EC_HOSTCMD_PD_CONTROL`                          | y      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_LOGGING`                              | n      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_CONSOLE_CMD`                          | n      | [USBC ]
`CONFIG_PLATFORM_EC_CONSOLE_CMD_USB_PD_PE`                       | y      | [USBC ]
`CONFIG_PLATFORM_EC_CONSOLE_CMD_USB_PD_CABLE`                    | y      | [USBC ]
`CONFIG_PLATFORM_EC_USB_VPD`                                     | n      | [USBC ]
`CONFIG_PLATFORM_EC_USB_CTVPD`                                   | n      | [USBC ]
`CONFIG_PLATFORM_EC_USB_DRP_ACC_TRYSRC`                          | n      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_TRY_SRC`                              | n      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_USB4`                                 | y      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_TBT_COMPAT_MODE`                      | y      | [USBC ]
`CONFIG_PLATFORM_EC_USBC_PPC`                                    | y      | [USBC ]
`CONFIG_PLATFORM_EC_USBC_PPC_POLARITY`                           | n      | [USBC ]
`CONFIG_PLATFORM_EC_USBC_PPC_SBU`                                | n      | [USBC ]
`CONFIG_PLATFORM_EC_USBC_PPC_VCONN`                              | n      | [USBC ]
`CONFIG_PLATFORM_EC_USBC_PPC_AOZ1380`                            | n      | [USBC ]
`CONFIG_PLATFORM_EC_USBC_PPC_RT1718S`                            | n      | [USBC ]
`CONFIG_PLATFORM_EC_USBC_PPC_KTU1125`                            | n      | [USBC ]
`CONFIG_PLATFORM_EC_USBC_PPC_NX20P3483`                          | n      | [USBC ]
`CONFIG_PLATFORM_EC_USBC_PPC_SN5S330`                            | n      | [USBC ]
`CONFIG_PLATFORM_EC_USBC_PPC_SYV682X`                            | n      | [USBC ]
`CONFIG_PLATFORM_EC_USBC_PPC_SYV682C`                            | n      | [USBC ]
`CONFIG_PLATFORM_EC_USBC_PPC_SYV682X_HV_ILIM`                    | 0      | [USBC ]
`CONFIG_PLATFORM_EC_USBC_PPC_SYV682X_NO_CC`                      | n      | [USBC ]
`CONFIG_PLATFORM_EC_USBC_PPC_SYV682X_SMART_DISCHARGE`            | n      | [USBC ]
`CONFIG_PLATFORM_EC_USBC_PPC_DEDICATED_INT`                      | n      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_TCPC_LOW_POWER`                       | y      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_TCPC_LPM_EXIT_DEBOUNCE_US`            | 25000  | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_TCPC_VCONN`                           | n      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_TCPM_TCPCI`                           | n      | [USBC ]
`CONFIG_PLATFORM_EC_USB_MUX`                                     | y      | [USBC ]
`CONFIG_PLATFORM_EC_USB_MUX_AMD_FP6`                             | n      | [USBC ]
`CONFIG_PLATFORM_EC_USB_MUX_IT5205`                              | n      | [USBC ]
`CONFIG_PLATFORM_EC_USB_MUX_PS8743`                              | n      | [USBC ]
`CONFIG_PLATFORM_EC_USB_MUX_TUSB1044`                            | n      | [USBC ]
`CONFIG_PLATFORM_EC_USBC_SS_MUX`                                 | y      | [USBC ]
`CONFIG_PLATFORM_EC_USB_MUX_RUNTIME_CONFIG`                      | y      | [USBC ]
`CONFIG_PLATFORM_EC_USBC_SS_MUX_DFP_ONLY`                        | n      | [USBC ]
`CONFIG_PLATFORM_EC_USB_MUX_VIRTUAL`                             | n      | [USBC ]
`CONFIG_PLATFORM_EC_USBC_RETIMER_FW_UPDATE`                      | y      | [USBC ]
`CONFIG_PLATFORM_EC_CONSOLE_CMD_PPC_DUMP`                        | y      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_TCPM_ITE_ON_CHIP`                     | n      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_ITE_ACTIVE_PORT_COUNT`                | 1      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_PPC`                                  | n      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_TCPC_RUNTIME_CONFIG`                  | y      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_TCPM_MULTI_PS8XXX`                    | n      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_TCPM_ANX7447`                         | n      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_TCPM_ANX7447_AUX_PU_PD`               | n      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_TCPM_ANX7447_OCM_ERASE_COMMAND`       | n      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_TCPM_NCT38XX`                         | n      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_TCPM_PS8751`                          | n      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_TCPM_PS8805`                          | n      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_TCPM_PS8805_FORCE_DID`                | y      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_TCPM_PS8815`                          | n      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_TCPM_PS8815_FORCE_DID`                | y      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_TCPM_RAA489000`                       | n      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_TCPM_RT1715`                          | n      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_TCPM_RT1718S`                         | n      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_TCPM_TUSB422`                         | n      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_TCPM_MUX`                             | n      | [USBC ]
`CONFIG_PLATFORM_EC_CONSOLE_CMD_TCPC_DUMP`                       | n      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_TCPM_DRIVER_IT83XX`                   | n      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_TCPM_DRIVER_IT8XXX2`                  | n      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_PULLUP`                               | 1      | [USBC ]
`CONFIG_PLATFORM_EC_USB_PD_ONLY_FIXED_PDOS`                      | n      | [USBC ]
`CONFIG_PLATFORM_EC_USB_CHARGER`                                 | y      | [USBC ]
`CONFIG_PLATFORM_EC_BC12_DETECT_PI3USB9201`                      | n      | [USBC ]
`CONFIG_PLATFORM_EC_BC12_DETECT_MT6360`                          | n      | [USBC ]
`CONFIG_PLATFORM_EC_MT6360_BC12_GPIO`                            | n      | [USBC ]
`CONFIG_PLATFORM_EC_BC12_SINGLE_DRIVER`                          | y      | [USBC ]


*Note - Avoid documenting `CONFIG_` options in the markdown as the relevant
`Kconfig*` contains the authoritative definition. Link directly to the Kconfig
option in source like this: [I2C Passthru Restricted].*

## Devicetree Nodes

*Detail the devicetree nodes that configure the feature.*

*Note - avoid documenting node properties here.  Point to the relevant `.yaml`
file instead, which contains the authoritative definition.*

## Board Specific Code

*Document any board specific routines that a user must create to successfully
compile and run. For many features, this can section can be empty.*

## Threads

*Document any threads enabled by this feature.*

## Testing and Debugging

*Provide any tips for testing and debugging the EC feature.*

## Example

*Provide code snippets from a working board to walk the user through
all code that must be created to enable this feature.*

<!--
The following demonstrates linking to a code search result for a Kconfig option.
Reference this link in your text by matching the text in brackets exactly.
-->
[I2C Passthru Restricted]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig?q=%22config%20PLATFORM_EC_I2C_PASSTHRU_RESTRICTED%22&ss=chromiumos
