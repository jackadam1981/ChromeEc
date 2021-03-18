# Hammer care and feeding

Original: [go/hammercare](http://go/hammercare)

Last updated: 2021-03-18

[TOC]

# Servo

## Start servod

```
cros_sdk --no-ns-pid
sudo servod --port=9000 -b hammer -c hammer.xml
```

## UART console

The simplest solution for most people is to use the `dut-console` script.

First, add this line into your .bashrc (or other shell init script; needed once
only):
```
alias dut-console="~/chromiumos/src/platform/dev/contrib/dut-console"
```

Then simply run `dut-console -c ec`. `dut-console` uses `cu` under the hood, and
works like ssh - to leave, press `<ENTER> <~> <.> <ENTER>`.


```
src/platform/dev/contrib/dut-console -p 9000 -c ec
```

# Flash EC

## Prerequisites

### Find the USB VID:PID of the device

### Stop hammerd

## Hammer connected to , flash via USB

## Hammer connected to base, flash via servo

## Hammer connected via servo only

# Update touchpad firmware
