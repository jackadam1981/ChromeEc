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
        "14.2/47a9bb6b7ef1ea584ed24078ea152a87204a37eb",
        "1ddeee8b906381c422932d7abfe075634e75667b81cd7e774858caad80258e28",
    )
    _coreboot_sdk_subtool(
        "i386-elf",
        "14.2/5ba88fb0227c76584851bd9cbb24d785e3000015",
        "",
    )
    _coreboot_sdk_subtool(
        "arm-eabi",
        "14.2/8adade1392d87565482ea57bfafaf74223000015",
        "",
    )
    _coreboot_sdk_subtool(
        "riscv-elf",
        "14.2/c97eb9fef0cf77f9d58d890de4e3e67f51000015",
        "",
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
