# Renode

This directory holds the configuration files for Renode.

## Hardware WP

On `bloonchipper`, do the following:

*   **Enable HW-WP** - type in `sysbus.gpioPortB.GPIO_WP Release` in the renode
    console to enable HW-WP.
*   **Disable HW-WP** - type in `sysbus.gpioPortB.GPIO_WP Press` in the renode
    console to disable HW-WP.

Note, you can just type `sysbus`, `sysbus.gpioPortB`, or
`sysbus.gpioPortB.GPIO_WP` to learn more about about these modules and the
available functions.
