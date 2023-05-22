// Copyright 2022 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use core::env;

fn main() {
    let out_dir = env!("OUT_DIR");
    println!("cargo:rerun-if-changed=.");
    println!("cargo:rerun-if-changed=../common/");
    println!("cargo:rustc-link-arg=static=rot128_eal.o");
    println!("cargo:rustc-link-search=native={out_dir}/RW/common/");
}
