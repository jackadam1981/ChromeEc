# Zephyr Programs

This directory contains all the project specific files for each Zephyr based EC.

## Definitions
The following conventions apply to this directory:

- **program**: The name of a Chromebook reference design. The **program**
  includes all Chromebooks based on a single AP SoC, such as Intel MeteroLake,
  Qualcomm 7c G3, or AMD Mendocino. The **program** corresponds to a single
  board overlay in the ChromeOS SDK.  The term *baseboard* is often used as a
  synonum for **program**.

- **project**: The name of a specific Chromebook model or variant.  All
  Chromebook **programs** contain at least one **project** which serves as the
  reference design(s) for the **program**. The reference **project** may or may
  not use the same name as the **program**. For example, the reference
  **project** for the skyrim **program** is also called skyrim. The corsola
  **program** included two reference **projects**, kingler and krabby. For the
  legacy ECOS builds, *board* was used as a synonym for **project**.

## Directory structure

Each **program** has it's own subdirectory under `zephyr/program`.

```
zephyr/program/
├── brya/
├── corsola/
├── herobrine/
├── intelrvp/
├── it8xxx2_evb/
├── minimal/
├── nissa/
├── npcx_evb/
├── rex/
├── skyrim/
├── trogdor/
└── README.md
```

Under each **program** subdirectory, there is a subdirectory foreach each
**project**, including a subdirectory for the reference **project**.

The minimum configuration for a **program** with just a single reference
**project** is shown below.

```
zephyr/program/skyrim/
├── include/
│   └── <program headers>.h
├── skyrim/
│   ├── include
│   │   └── <project headers>.h
│   ├── src/
│   │   └── <project source>.c
│   ├── CMakeLists.txt
│   ├── project.conf
│   └── project.overlay
├── src/
│   └── <program source>.c
├── BUILD.py
├── CMakeLists.txt
├── Kconfig
├── program.conf
└── *.dtsi*
```

### **Program** Directory Details

Note that all paths are relative to the `zephyr/program/` directory:

- **`<program>`**`/`: Top level directory for the **program**.
- **`<program>`**`/include/`: Directory containing the header files common to
  all **projects** in the **program**. Use of **program** level includes is
  discouraged.  Instead, you should create a generic driver that can be shared
  across all **programs**.
- **`<program>`**`/src/`: Directory containing the C source files common to all
  **projects** in the **program**.
- **`<program>`**`/BUILD.py`: Defines which **projects** can be made from this
  directory, and what the device-tree overlays and Kconfig files are for each
  **project**.
- **`<program>`**`/CMakeLists.txt`: CMake file for the **program**. This file
  defines the rules for compiling the files found under **`<program>`**`/src/`.
  This file also includes the **project** specific CMake file.
- **`<program>`**`/Kconfig` - Defines **program** specific options. Usually this
  defines an option called CONFIG_BOARD_<project> for every **project**.
- **`<program>`**`/program.conf` - Default Kconfig settings for all
  **projects**.
- **`<program>`**`/*.dtsi` - One or more devicetree files, organized by the
  hardware module or EC feature.  See the [Devicetree Best
  Practices](#devicetree-best-practices) section for additional information.
- **`<program>`**`/`**`<project>`**`/`: Top level directory for the **project**.
  Create a separate directory for each **project** defined by the **program**.

### **Project** Directory Details

Note that all paths are relative to the
`zephyr/program/`**`<program>`**`/` directory.

- **`<project>`**`/include/`: The **project** may optionally provide a public
  include directory, but this is discouraged. There are some exceptions where
  the legacy EC code expects the project to define a public header, such as the
  keyboard_customization.h file.
- **`<project>`**`/src/`: Directory containing the C source files specific to
  the **project**.
- **`<project>`**`/CMakeLists.txt`: CMake file for the **project**. This file
  defines the rules for compiling the files found under **`<project>`**`/src/`.
- **`<project>`**`/project.conf`: Kconfig settings for the **project**. Kconfig
  options defined in this file take precedence over settings defined by
  **`<program>`**`/program.conf`.
- **`<project>`**`/project.overlay`:

## Devicetree Best Practices
