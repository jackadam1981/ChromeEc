#-*- makefile -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Private board specific files build

board-private-y = private-nolib.o vdec.o

# TODO(b/121406695): We do not include the archive in RO, since we do not care
# about RO image
PRIVPATH=private-mtk/board/kukui_scp
RWPATH=$(out)/RW/$(PRIVPATH)

$(RWPATH)/libproject1.a: $(PRIVPATH)/libproject1.a
	@cp $^ $@

$(out)/RW/ec.RW.elf: $(RWPATH)/libproject1.a
# Include the whole libproject1 archive (not just the symbols that are used)
$(out)/RW/ec.RW.elf: LDFLAGS_EXTRA += -L$(RWPATH) -Wl,--whole-archive -lproject1
# Restore default behaviour
$(out)/RW/ec.RW.elf: LDFLAGS_EXTRA += -Wl,--no-whole-archive
