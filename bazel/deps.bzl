# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

def _ec_deps_impl(module_ctx):
    module_ctx = _fwsdk_deps_impl(module_ctx)
    _coreboot_sdk_subtool(
        "nds32le-elf",
        "11.3.0-r2/47a9bb6b7ef1ea584ed24078ea152a87204a37e1",
        "7299ae598233876ec2f562a1173f8b65de169b1de51cb6e04e003edfb4d04fe7",
    )
    _coreboot_sdk_subtool(
        "i386-elf",
        "11.3.0-r2/5ba88fb0227c76584851bd9cbb24d785e31a717b",
        "72f0b55516120e0919f10ddf28c53a429ccc8132685b6dbd6a8dcefeba92fcc5",
    )
    _coreboot_sdk_subtool(
        "x86_64-elf",
        "11.3.0-r2/7cef840b42c0881fc31eae2031197b844b6fc512",
        "5cd26205b963ca02623e0cc3b53e7eb9725a32d0d08cadfbbb3cf25a6d53c248",
    )
    _coreboot_sdk_subtool(
        "arm-eabi",
        "11.3.0-r2/8adade1392d87565482ea57bfafaf74223cebbe5",
        "312557355983bf732b20dcf7b7553a5b2a13247fc3ad6f21226ff260db1783cd",
    )
    _coreboot_sdk_subtool(
        "aarch64-elf",
        "11.3.0-r2/929d0edbd1d02b5e2c61632d1eb42fe466bf0b69",
        "3a89a034bfafcab8fcc1e6ee24a0b2e785150e9e7892569db0c1f2d54feb8e15",
    )
    _coreboot_sdk_subtool(
        "riscv-elf",
        "11.3.0-r2/c97eb9fef0cf77f9d58d890de4e3e67f5158166f",
        "2345cfbf3dffd2efe0cdfa8d6100a0923d2dc8b77da9d98b9fd07200b18abec1",
    )
    _coreboot_sdk_subtool(
        "iasl",
        "11.3.0-r2/a59cd53c87d0d4e57386c070e9f9a2f1d1cf9b6f",
        "ad1f61b82f13c229e37ac19f43aba835363fed98ff69ff431abf3d11b311bb53",
    )

    return module_ctx.extension_metadata(
        root_module_direct_deps = generated_repos,
        root_module_direct_dev_deps = [],
        reproducible = True,
    )

ec_deps = module_extension(
    implementation = _ec_deps_impl,
)
