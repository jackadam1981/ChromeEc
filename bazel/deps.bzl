# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def _ec_deps_impl(module_ctx):
    def _coreboot_sdk_subtool(arch, version, sha256, bucket = "chromiumos-sdk"):
        name = "ec-coreboot-sdk-%s" % arch
        toolchain_name = "coreboot-sdk-%s" % arch
        http_archive(
            name = name,
            build_file = "//platform/rules_cros_firmware/cros_firmware:BUILD.gcs_subtool",
            sha256 = sha256,
            url = "https://storage.googleapis.com/%s/toolchains/%s/%s.tar.zst" % (bucket, toolchain_name, version),
        )

    _coreboot_sdk_subtool(
        "nds32le-elf",
        "14.2.0-r3/dcf960a7d85793abe1b27aef46a1e5e79e8b995e",
        "1c8c5b206bd13a40761fff9cd104b2dae32790eea1688f3ec98d3975aa26ee07",
    )
    _coreboot_sdk_subtool(
        "i386-elf",
        "14.2.0-r3/b55705844741bee0577171f53edd14dac5df0e43",
        "8967be6a0022e41569a367c4ed31d31284fec36070d5bafb3e0ec472805154b2",
    )
    _coreboot_sdk_subtool(
        "picolibc-i386-elf",
        "14.2.0-r1/cba7c6f468c509ef3fb5b4f87c720c9ac15f155e",
        "27e0e42562a8c8595a0bff32b63c22ab6a33fb60a670430a3ccf94d0e4e40a38",
    )
    _coreboot_sdk_subtool(
        "libstdcxx-i386-elf",
        "14.2.0-r1/2d5247502a026c6ea9ef9fb3acac8fb5d7651780",
        "8638f2ebcc4db17c535805ece15550f0f55e07af3bd3a83d8c160aedd7c238ea",
    )
    _coreboot_sdk_subtool(
        "arm-eabi",
        "14.2.0-r3/d1512baac52606aa45d0bdd38040e67df2e17d7c",
        "d2e4f86a37f8674bb172ccb52a0fe8d1364564f9e37e1464fc7303fb50adb0f3",
    )
    _coreboot_sdk_subtool(
        "picolibc-arm-eabi",
        "14.2.0-r1/efbc26304ebbf40a247546b7aa4c6d998e616c66",
        "36cb7538d07df0491123524fcc7ac23f38d379126dfa30ae657a54c125260412",
    )
    _coreboot_sdk_subtool(
        "libstdcxx-arm-eabi",
        "14.2.0-r1/0f1d905aeac1f73908e7da8691c7d662e06408c5",
        "4a076c6932b2ebdf28e623f1b3f7961162f41ddfd3b778c7672b217d33eba815",
    )
    _coreboot_sdk_subtool(
        "riscv-elf",
        "14.2.0-r3/f86d8c0ebc8e5d03f4193a7c6b9732a52a2778c1",
        "4fcde5976454537569dd07e62c47f35fc2f4db745a4ad269cfb153d27da8d0b1",
    )

    return module_ctx.extension_metadata(
        root_module_direct_deps = [
            "ec-coreboot-sdk-arm-eabi",
            "ec-coreboot-sdk-picolibc-arm-eabi",
            "ec-coreboot-sdk-libstdcxx-arm-eabi",
            "ec-coreboot-sdk-i386-elf",
            "ec-coreboot-sdk-picolibc-i386-elf",
            "ec-coreboot-sdk-libstdcxx-i386-elf",
            "ec-coreboot-sdk-nds32le-elf",
            "ec-coreboot-sdk-riscv-elf",
        ],
        root_module_direct_dev_deps = [],
        reproducible = True,
    )

ec_deps = module_extension(
    implementation = _ec_deps_impl,
)
