# Nucleo H743ZI

This is a simpler EC example for the ST Nucleo H743ZI
development board.

# Quick Start

The Nucleo dev boards have lots of developer friendly features,
like an in-circuit debugger/programmer/UART-bridge, programmable
LEDs, and a button, to name a few.

The built-in debugger can be connected to using a Micro USB cable.
It provides three great interfaces to the host.
1. Mass storage interface for drag-drop programming
2. Full ST-Link in-circuit debugger
3. UART bridge for logs/consoles

We will use a few of these interfaces below to program and interact
with out Nucleo dev board.

## Build

```bash
make BOARD=nucleo-h743zi -j
```

## Program

The easiest way to flash the Nucleo board is to Copy-Paste/Drag-Drop
the firmware image onto the exposed mass storage drive.

Open a file browser and `Copy` the file in `build/nucleo-h743zi/ec.bin`.
Now, find the removable storage that the Nucleo device has presented,
and `Paste` the file into the directory.

## Interact

After the Nucelo finishes programming, you can open the EC console.
On GNU/Linux, this is mapped to `/dev/ttyACM0`.

Install `minicom` and issue the following command:

```bash
minicom -D/dev/ttyACM0
```

