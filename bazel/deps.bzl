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
            url = "https://storage.googleapis.com/chromeos-throw-away-bucket/toolchains/coreboot-sdk-%s/%s.tar.zst" % (arch, version),
        )

    _coreboot_sdk_subtool(
        "nds32le-elf",
        "14.2/jpmurphy_build",
        "1ddeee8b906381c422932d7abfe075634e75667b81cd7e774858caad80258e28",
    )
    _coreboot_sdk_subtool(
        "i386-elf",
        "14.2/jpmurphy_build",
        "4c75ef43d234b7bac62cd6fd13a3b1fb7aab52d93fc14b7628ed3003340ffeda",
    )
    _coreboot_sdk_subtool(
        "arm-eabi",
        "14.2/jpmurphy_build",
        "910a2ce2f704caa9ea8c7c2a918d23ad79099cc2b1ab3a04bc579952a51420cc",
    )
    _coreboot_sdk_subtool(
        "riscv-elf",
        "14.2/jpmurphy_build",
        "750ce5caafc566d648b51ef6d95c6fef7a95fd02f39aa34fd30f0311b1235b63",
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
