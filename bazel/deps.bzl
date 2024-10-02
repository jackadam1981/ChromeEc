# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def _ec_deps_impl(module_ctx):
    def _coreboot_sdk_subtool(arch, version, sha256):
        http_archive(
            name = "ec-coreboot-sdk-%s" % arch,
            build_file = "//platform/rules_cros_firmware/cros_firmware:BUILD.gcs_subtool",
            sha256 = sha256,
            url = "https://storage.googleapis.com/chromiumos-sdk/toolchains/coreboot-sdk-%s/%s.tar.zst" % (arch, version),
        )

    _coreboot_sdk_subtool(
        "nds32le-elf",
        "14.2.0-r3/bd5bb04ddc34fa01dd0434ebb60e15d857095dbf",
        "1526127d31eed03bf0645a4097afe7b154e25ac2efc0e97d235becbba147d233",
    )
    _coreboot_sdk_subtool(
        "i386-elf",
        "14.2.0-r3/ea688ec47700247598fc4764596a27e62ebed88f",
        "fdb94315fbecd56513277907d9f3ad08132d05a92fa542c7959be5ec59851745",
    )
    _coreboot_sdk_subtool(
        "arm-eabi",
        "14.2.0-r3/8e94b5a316192651d9dafe1497e6826528f601a3",
        "e9613b9b275a641571722af633cb591a80921cd869bf881e41e62fda556d859c",
    )
    _coreboot_sdk_subtool(
        "riscv-elf",
        "14.2.0-r3/e84d6218dbbf87e59f4cf164b8f1353649712ba6",
        "75f4cbe2b5607fc0c421a60994c8834a3bf5f051029974e87eb1a87db905853b",
    )

    return module_ctx.extension_metadata(
        root_module_direct_deps = [
            "ec-coreboot-sdk-arm-eabi",
            "ec-coreboot-sdk-i386-elf",
            "ec-coreboot-sdk-nds32le-elf",
            "ec-coreboot-sdk-riscv-elf",
        ],
        root_module_direct_dev_deps = [],
        reproducible = True,
    )

ec_deps = module_extension(
    implementation = _ec_deps_impl,
)
