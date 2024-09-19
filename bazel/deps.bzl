# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def _cr50_deps_impl(module_ctx):
    def _coreboot_sdk_subtool(arch, version, sha256):
        http_archive(
            name = "cr50-coreboot-sdk-%s" % arch,
            build_file = "//platform/rules_cros_firmware/cros_firmware:BUILD.gcs_subtool",
            sha256 = sha256,
            url = "https://storage.googleapis.com/chromiumos-sdk/toolchains/coreboot-sdk-%s/%s.tar.zst" % (arch, version),
        )

    _coreboot_sdk_subtool(
        "arm-eabi",
        "11.3.0-r2/8adade1392d87565482ea57bfafaf74223cebbe5",
        "312557355983bf732b20dcf7b7553a5b2a13247fc3ad6f21226ff260db1783cd",
    )

    return module_ctx.extension_metadata(
        root_module_direct_deps = [
            "coreboot_sdk",
            "coreboot-sdk-arm-eabi",
        ],
        root_module_direct_dev_deps = [],
        reproducible = True,
    )

cr50_deps = module_extension(
    implementation = _cr50_deps_impl,
)
