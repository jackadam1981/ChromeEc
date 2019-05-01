# BusPirate Python Library and Tool

A simple python library and tool for programming the
[BusPirate](http://dangerousprototypes.com/docs/Bus_Pirate) and programming
STM32 chips connected to the BusPirate.

## Software Setup

* Make sure `virtualenv` is installed:
```
sudo apt-get install python-venv
```

* Setup virtual environment:
```bash
python3.6 -m venv env
source ./env/bin/activate
```

* Install dependencies:
```bash
pip install -r requirements.txt
```

* Create a `udev` rule so BusPirate comes up as `/dev/buspirate` by creating the file `/etc/udev/rules.d/99-BusPirate.rules`:
```
SUBSYSTEM=="tty", ATTRS{idVendor}=="0403", ATTRS{idProduct}=="6001", SYMLINK+="buspirate", MODE="0666"
```

## Hardware Setup

Colors below based on this image:

![](http://dangerousprototypes.com/docs/images/b/be/Bp-cable-color-hk.png)

Also see [this page](https://learn.sparkfun.com/tutorials/bus-pirate-v36a-hookup-guide) for more details.

### STM32F412

#### UART

* `3V3` (red) to `3V3`
* `GND` (brown) to `GND`
* `AUX` (blue) to `RST`
* `MOSI` (gray) to `UART TX`
* `MISO` (black) to `UART RX`

For bootloader mode:
* `3V3` (red) to `BOOT0`

For standard mode:
* `GND` (brown) to `BOOT0`

#### SPI

* `3V3` (red) to `3V3`
* `GND` (brown) to `GND`
* `AUX` (blue) to `RST`
* `MOSI` (gray) to `MOSI`
* `MISO` (black) to `MISO`
* `CLK` (purple) to `CLK`
* `CS` (white) to `CS`

For bootloader mode:
* `3V3` (red) to `BOOT0`

For standard mode:
* `GND` (brown) to `BOOT0`

## Running

See the output of the `--help` flag:

```bash
buspirate_tool.py --help
```

## Running Tests

```
./run_tests.sh
```

## Additional Resources

* [Purchase a BusPirate](https://www.sparkfun.com/products/12942)
* [Purchase a BusPirate Cable](https://www.sparkfun.com/products/9556)
* [Purchase a USB Mini-B Cable](https://www.sparkfun.com/products/11301)
