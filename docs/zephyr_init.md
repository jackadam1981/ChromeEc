# Zephyr OS-based EC Initialization Order

Zephyr provides Z_INIT_ENTRY_DEFINE() & the extend macro to install the initial
function. The initialize flow for different level would be like the following
(not very detailed):
 * architecture-specific initialization
 * `PRE_KERNEL_1` level
 * `PRE_KERNEL_2` level
 * `POST_KERNEL` level
 * `APPLICATION` level
 * main()

The kernel and driver initial functions separate into specific initialize
levels. It couldn't put all initial functions in main() for the Zephyr OS-based
EC. It is also hard to maintain those initial priority which separates into
different files.

This file defines some Zephyr OS-based EC initial priorities which have critical
sequence requirement for initializing:

## PRE_KERNEL_1
* Priority (0-9) `"Buffer for the system testability"`:  
The highest priority could be used in zephyr. Don't use it when system
development. Buffer it for the following system development & testing.
* Priority (10-19) `"Chip level system pre-initialization"`:  
Chip drivers should & only finish the initialize stuff for the following
PLATFORM_EC_SYSTEM_PRE_INIT.(e.g., cros_system init for the chip level reset
cause, cros_bbram init for the system reset flag).
* Priority (20) `"PLATFORM_EC_SYSTEM_PRE_INIT"`:  
CROS system uses some critical data (e.g., system reset cause) for
initialization. It should prepare those data before this priority for the
following initialization.
* TODO
## PRE_KERNEL_2
* TODO

## POST_KERNEL
* TODO

## APPLICATION
* TODO

## main()
* TODO
* Start the tasks.
