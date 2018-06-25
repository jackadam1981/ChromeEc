#-*- makefile -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Private board specific files build

board-private-y = private-nolib.o

# TODO(b/121406695): We do not include the archive in RO, since we do not care
# about RO image
PRIVPATH=private-mtk/board/kukui_scp
RWPATH=$(out)/RW/$(PRIVPATH)

$(RWPATH)/libprivate.a: $(RWPATH)/private.o $(RWPATH)/private2.o

$(out)/RW/ec.RW.elf: $(RWPATH)/libprivate.a
# Include the whole libprivate archive (not just the symbols that are used)
$(out)/RW/ec.RW.elf: LDFLAGS_EXTRA += -L$(RWPATH) -Wl,--whole-archive -lprivate
# Restore default behaviour
$(out)/RW/ec.RW.elf: LDFLAGS_EXTRA += -Wl,--no-whole-archive
