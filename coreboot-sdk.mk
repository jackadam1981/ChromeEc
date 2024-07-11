# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

ifdef TOOLCHAIN
TOOLCHAIN_INSTALL_PATH=/opt/coreboot-sdk/${TOOLCHAIN}/${TOOLCHAIN_VERSION}/${TOOLCHAIN_HASH}
COREBOOT_SDK_URI_PATH=https://storage.googleapis.com/chromiumos-sdk/toolchains/coreboot-sdk
TARBALL=${TOOLCHAIN_HASH}.tar.zst
TOOLCHAIN_URI=${COREBOOT_SDK_URI_PATH}-${TOOLCHAIN}/${TOOLCHAIN_VERSION}/${TARBALL}

toolchain_status := $(shell	if [[ ! -d $(TOOLCHAIN_INSTALL_PATH) ]]; then \
		sudo mkdir -p $(TOOLCHAIN_INSTALL_PATH) && \
		sudo chmod 777 $(TOOLCHAIN_INSTALL_PATH) && \
		pushd $(TOOLCHAIN_INSTALL_PATH) && \
		curl -L -O $(TOOLCHAIN_URI) && \
		sudo tar -I pzstd --no-same-owner -xf "./$(TARBALL)" && \
		popd; \
	fi )

endif