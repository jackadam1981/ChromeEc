# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
package(default_visibility = ["//platform/ti50/common:__subpackages__"])

load("@rules_rust//rust:defs.bzl", "rust_binary")

rust_binary(
    name = "explain_ap_ro_verification_status",
    srcs = ["src/main.rs"],
    aliases = {
        "//platform/ti50/common/capsules": "ti50",
        "//platform/ti50/common/syscalls": "ti50_syscalls",
    },
    edition = "2021",
    deps = [
        "//platform/ti50/common/capsules",
        "//platform/ti50/common/syscalls",
        "//third_party/tock/tock/kernel",
        "@ti50_std_rust_deps//:thiserror",
    ],
)
