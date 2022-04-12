# Zephyr AP Power Sequence Configuration

[TOC]

## Overview

In Chrome devices, one of the main features of Embedded Controller (EC) is to orchestrate power sequences for the platform, one of the key components that are included in this task is the Application Processor (AP).

EC monitors various digital and/or analog signals to act accordingly to drive AP power state transitions by providing synchronized power control signals.

## Kconfig Options

Kconfig Option                         | Default | Documentation
:------------------------------------- | :-----: | :------------
`CONFIG_AP_PWRSEQ`                     | n       | [AP_PWRSEQ]

The following options are available only when `CONFIG_AP_PWRSEQ=y`.

Kconfig sub-option                     | Default | Documentation
:------------------------------------- | :-----: | :------------
`CONFIG_AP_PWRSEQ_AUTOSTART`           | n       | [AP_PWRSEQ_AUTOSTART]
`CONFIG_AP_PWRSEQ_STACK_SIZE`          | n       | [AP_PWRSEQ_STACK_SIZE]
`CONFIG_AP_PWRSEQ_S0IX`                | n       | [AP_PWRSEQ_S0IX]

The following options are available for x86 based Chromebooks only.

Kconfig sub-option                     | Default | Documentation
:------------------------------------- | :-----: | :------------
`CONFIG_X86_NON_DSX_PWRSEQ`            | n       | [X86_NON_DSX_PWRSEQ]
`CONFIG_X86_NON_DSX_PWRSEQ_ADL`        | n       | [X86_NON_DSX_PWRSEQ_ADL]
`CONFIG_X86_NON_DSX_PWRSEQ_CONSOLE`    | n       | [X86_NON_DSX_PWRSEQ_CONSOLE]

## Devicetree Nodes

AP power sequence requires a number of input and output signals to be configured and 

## Board Specific Code

X86 signalling.

## Threads

When this feature is enabled, the routine `pwrseq_loop_thread` is created, the thread stack size is set using `CONFIG_AP_PWRSEQ_STACK_SIZE`, its priority is set with `K_PRIO_COOP(8)`. If `CONFIG_AP_PWRSEQ_AUTOSTART` is set, thread will start running as soon as it is created, otherwise, function `ap_pwrseq_task_start` must be called to initiate thread execution.

## Testing and Debugging

*Provide any tips for testing and debugging the EC feature.*

## Example

*Provide code snippets from a working board to walk the user through
all code that must be created to enable this feature.*

[AP_PWRSEQ]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/subsys/ap_pwrseq/Kconfig?q=config%20AP_PWRSEQ
[AP_PWRSEQ_AUTOSTART]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/subsys/ap_pwrseq/Kconfig?q=config%20AP_PWRSEQ_AUTOSTART

