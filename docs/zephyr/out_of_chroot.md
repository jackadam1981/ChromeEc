# Building Zephyr Out-of-Chroot

It is possible to build Zephyr outside of the Chromium OS chroot,
albeit with additional steps.

**You have the choice of using `west` or `zmake`.**

*See steps below for either option.*

*** note
**Note:** These steps are maintained on a best-effort basis, and may
not be accurate depending on your Linux distribution and your system's
specific environment.
***

[TOC]

## Setup your build environment using west

**Note:** *This currently only works for testing. Integration with Zephyr's
[Sysbuild](https://docs.zephyrproject.org/latest/build/sysbuild/index.html) is
needed to be able to build/flash images with commands like `west build
projects/herobrine`*

`west` will create <project_dir> and sync the code and dependencies into it with
the following commands:

```bash
west init -m https://chromium.googlesource.com/chromiumos/platform/ec/ <project_dir>
west update
west setup
```
This will give you the following project directory
```
<project_dir>
    ├── modules
    │   ├── cmsis
    │   ├── cryptoc
    │   ├── ec
    │   ├── hal_stm32
    │   └── nanopb
    ├── tools
    │   └── zephyr-sdk-0.15.2
    └── zephyr
        ├── arch
        ├── boards
        ├── cmake
        ├── doc
        ├── drivers
        ├── dts
        ├── include
        ├── kernel
        ├── lib
        ├── misc
        ├── modules
        ├── samples
        ├── scripts
        ├── share
        ├── soc
        ├── submanifests
        ├── subsys
        └── tests
```

Confirm directory is properly setup by building with twister in the virtual env
```bash
source .venv/bin/activate
cd modules/ec
./twister -c -T zephyr/test/math
```

***

## Setup build environment with zmake
### Remove west, if installed [b/184654974](http://b/184654974)

Zephyr's Cmake system will try to attach itself to the west tool if it finds it
installed, conflicting with manual cmake invocations. If you installed west,
you'll need to remove it:

```bash
python3 -m pip uninstall west
```

### Install zmake

You can install zmake with pip:

```bash
cd ~/chromiumos/src/platform/ec
python3 -m pip install -e zephyr/zmake --user
```

Ensure that `~/.local/bin` is in your `PATH`.

You may also need to install these items:

```bash
sudo apt-get install cmake ninja-build python3-pyelftools gcc-multilib \
    python3-pykwalify python3-colorama python3-testfixtures
```

### Install binman

First build pylibfdt:

```bash
cd somewhere
sudo apt-get install flex bison swig
git clone git://git.kernel.org/pub/scm/utils/dtc/dtc.git
cd dtc
make
make install PREFIX=~/.local         # You can install this where it suits
```

If you have a Chromium OS checkout then you should do:

```bash
cd ~/.local/bin
ln -s ~/chromiumos/src/third_party/u-boot/files/tools/binman/binman
```

otherwise:

```bash
cd somewhere
git clone https://source.denx.de/u-boot/u-boot.git
cd ~/.local/bin
ln -s somewhere/u-boot/tools/binman/binman
```

### Install Zephyr toolchain

If using the Zephyr toolchain (`-t zephyr`), follow the [upstream
documentation] to install the Zephyr build tools.

[upstream documentation]: https://docs.zephyrproject.org/getting_started/index.html#install-a-toolchain
