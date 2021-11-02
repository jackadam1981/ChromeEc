Zmake: Build orchestrator for Zephyr-based EC
=============================================

[TOC]

Zephyr's build system is based on CMake.  While the CMake build system
can be invoked manually, but can be tedious as many defines are
necessary to be passed on the command-line to CMake.

Upstream Zephyr projects solve this by using a meta-build tool called
`west`.  The `west` tool provides the following features:

- Downloading and syncing Git repos for Zephyr modules.
- Invoking `cmake` with the right options and calling `ninja`.
- Flashing onto hardware using `openocd` or other flashing tools.

While the `west` tool does some of what we need (setting up `cmake`
calls and running `ninja`), it's also not the right tool for us in
other ways:

- The Git repo integration in `west` is harmful instead of helpful, as
  we want `repo` to manage that for us and portage to be able to check
  out the commits it wants to.
- The flashing support doesn't work for chromebooks, as we have
  external needs `west` doesn't know how to deal with (`dut-control`
  commands).
- `west` has no support for building RO+RW images and packing them
  together in a single binary using FMAP.

Initially we solved our problems by writing a shell script that
invoked cmake to our needs, but the shell script never supported
multiple boards, and quickly grew out of hand.  Today we have `zmake`,
a Python-based `cmake` wrapper.

## Running zmake

The general syntax of a `zmake` command is:

``` shellsession
$ zmake [options...] subcommand [subcommand options...] [subcommand arguments...]
```

### Subcommands

Below is a brief overview of all of the subcommands.  For a full list
of options, run `zmake --help`.

* `configure` - Set up a build directory for a board.
* `build` - Do the build for a previously set-up build directory.
* `test` - Run tests associated with a build.
* `coverage` - Build with coverage options enabled.
* `testall` - Convenience command to compile all builds and run all
  tests.

### Common commands cheat-sheet

Configure and build a project named `lazor`:

``` shellsession
$ zmake configure -b lazor
```

Test that your change doesn't break any builds and all tests pass:

``` shellsession
$ zmake testall
```

## Setting up a new reference board

To set up a new reference board, you need a new directory with the
following files:

- `BUILD.py` - specifies which builds can be made from this directory,
  and what the device-tree overlays and Kconfig files are for each
  build.
- `CMakeLists.txt` - Baseboard-specific C files can be listed here.
- `prj.conf` (optional) - Default Kconfig settings for all projects.
- `prj_${project_name}.conf` (optional) - Project-specific Kconfig
  settings.
- `Kconfig` (optional) - Set options for your reference design here,
  which variants can use to install optional C sources.

An in-depth example of each file is given below:

### BUILD.py

`BUILD.py` is a Python-based config file for setting up your reference
board and the associated variants.  The name `BUILD.py` is important
and case-sensitive: `zmake` searches for files by this
name.

When `BUILD.py` is sourced, the following two globals are defined:

- `here` is a `pathlib.Path` object containing the path to the
  directory `BUILD.py` is located in.
- `register_project` is a function which informs `zmake` of a new
  project to be built.  Your `BUILD.py` file needs to call this
  function one or more times.

`register_project` takes the following keyword arguments:

- `project_name`: The name of the project (typically the model name).
  This name must be unique amongst all projects known to `zmake`, and
  `zmake` will error if you choose a conflicting name.
- `zephyr_board`: The name of the Zephyr board to use for the project.
  The Zephyr build system expects a board directory under
  `boards/${ARCH}/${ZEPHYR_BOARD_NAME}`.  Note that the concept of a
  Zephyr board does not align with the Chrome OS concept of a board:
  for most projects this will typically be the name of the chip used,
  not the name of the model or overlay.
- `zephyr_version`: The version of Zephyr OS to build the project
  with.
- `supported_toolchains`: A list of the toolchain names supported by
  the build.  Valid values are `coreboot-sdk`, `host`, `llvm`, and
  `zephyr`.  Note that only `coreboot-sdk` and `llvm` are supported in
  the chroot, and all projects must be able to build in the chroot, so
  your project must at least list one of `coreboot-sdk` or `llvm`.
- `output_packer`: An output packer type which defines which builds
  get generated, and how they get assembled together into a binary.
- `modules`: A list of module names required by the project.  Defaults
  to all modules known by `zmake`.  There is no harm to including
  unnecessary modules as modules are typically guarded by Kconfig
  options, so the only reason to set this is if your project needs to
  build in a limited environment where not all modules are available.
- `is_test`: `True` if the code should be executed as a unit test
  after compilation, `False` otherwise.
- `dts_overlays`: A list of files which should be concatenated
  together and applied as a Zephyr device-tree overlay.
- `project_dir`: The path to where `CMakeLists.txt` and `Kconfig` can
  be found for the project, defaulting to `here`.

Note that most projects will not want to call `register_project`
directly, but instead one of the helper functions, which sets even
more defaults for you:

- `register_host_project`: Define a project which runs in the chroot
  (not on hardware).
- `register_host_test`: Just like `register_host_project`, but
  `is_test` gets set to `True`.
- `register_raw_project`: Register a project which builds a single
  `.bin` file, no RO+RW packing, no FMAP.
- `register_binman_project`: Register a project which builds RO and RW
  sections, packed together, and including FMAP.
- `register_npcx_project`: Just like `register_binman_project`, but
  expects a file generated named `zephyr.npcx.bin` for the RO section
  with Nuvoton's header.

You can find the implementation of these functions in
`zephyr/zmake/zmake/configlib.py`.

`BUILD.py` files are auto-formatted with `black`.  After editing a
`BUILD.py` file, please run `black BUILD.py` on it.

### CMakeLists.txt

This file, should at minimium contain the following:

``` cmake
cmake_minimum_required(VERSION 3.20.1)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ec)
```

You may additionally want to specify any C files or include
directories your project needs using `zephyr_library_sources` or
`zephyr_library_include_directories`.

### prj.conf and prj_${project_name}.conf

`prj.conf` has default Kconfig settings for all projects, and
`prj_${project_name}.conf` can contain overrides for certain projects.
The format is `KEY=VALUE`, as typical for Kconfig.

### Kconfig

If certain projects need project-specific C files or ifdefs, the only
way to do so is to create a `Kconfig` file with the options schema you
want, and use it to toggle the inclusion of certain files.

The file must end with a single line that reads
`source "Kconfig.zephyr"`.  Note that this file is optional, so it's
recommended to only include it if you really need it.

## Setting up a new variant of a reference board

**Unlike our legacy EC, there are no files or directories to copy and
paste to setup a new variant in Zephyr code.**

Simply add a `register_project`-based call to the existing `BUILD.py`
for your reference board.

Below is an example of how projects may wish to structure this in
`BUILD.py`:

``` python
# Copyright 2021 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

def register_variant(project_name, chip="it8xx2", extra_dts_overlays=()):
    register_binman_project(
        project_name=project_name,
        zephyr_board=chip,
        dts_overlays=[
            here / "base_power_sequence.dts",
            here / "i2c.dts",
            **extra_dts_overlays,
        ],
    )


# Reference board
register_variant(
    project_name="asurada",
    extra_dts_overlays=[here / "reference_gpios.dts"],
)

# Variants
register_variant(
    project_name="hayato",
    extra_dts_overlays=[here / "hayato_gpios.dts"],
)
```

## Making changes to Zmake

`zmake` will automatically re-exec itself from the `platform/ec` if
running inside of the chroot and not inside of an ebuild context.
This means, usually there's nothing to emerge after editing `zmake`,
however, if for some reason you're needing to emerge, the package to
`cros-workon --host` and emerge is `chromeos-base/zephyr-build-tools`.

The `zmake` sources are auto-formatted with `black`.  To re-format
after making changes:

``` shellsession
(chroot) $ pwd
/mnt/host/source/src/platform/ec/zephyr/zmake
(chroot) $ black .
```

To run tests:

``` shellsession
(chroot) $ pwd
/mnt/host/source/src/platform/ec/zephyr/zmake
(chroot) $ ./run_tests.sh
```

The tests are written in [Pytest](https://pytest.org).  Please be sure
to add tests when adding features.
