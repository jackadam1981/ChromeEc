# CMake Example

This directory contains an example of a library built by CMake that can be
cross-compiled and linked with the EC code.

## Building the Library

Run the following command to build the library using the EC CMake toolchain
file:

```bash
(chroot) $ rm -rf example/build && mkdir -p example/build && pushd example/build && cmake -DCMAKE_TOOLCHAIN_FILE=~/chromiumos/src/platform/ec/cmake/toolchain.cmake .. && make && popd
```

## Build the EC board

Once the example library has been compiled, you can build a board that links
against it:

```bash
(chroot) $ rm -rf build && make BOARD=bloonchipper -j
```
