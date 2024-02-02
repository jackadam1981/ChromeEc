# Renode

This directory holds the configuration files for Renode.

## Hardware WP

On `bloonchipper`, do the following:

*   **Enable** - type `sysbus.gpioPortB.GPIO_WP Release` in the renode console.
*   **Disable** - type `sysbus.gpioPortB.GPIO_WP Press` in the renode console.

Note, you can just type `sysbus`, `sysbus.gpioPortB`, or
`sysbus.gpioPortB.GPIO_WP` to learn more about about these modules and the
available functions.
