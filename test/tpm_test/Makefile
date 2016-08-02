# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

ifeq ($(V),)
Q := @
else
Q :=
endif

obj = ../../build/tpm_test
src = .
SWIG = /usr/bin/swig

CFLAGS = -fPIC
CFLAGS += -I /usr/include/python2.7
CFLAGS +=  -DLIBFTDI1=1
CFLAGS +=  -c

TARGET = ftdi_spi_tpm
.PRECIOUS: $(obj)/ftdi_spi_tpm_wrap.c

$(obj)/_$(TARGET).so:

OBJS = $(obj)/$(TARGET).o $(obj)/$(TARGET)_wrap.o $(obj)/mpsse.o \
	$(obj)/support.o

DEPS := $(OBJS:.o=.o.d)

$(OBJS): | $(obj)

$(obj)/%.o: $(obj)/%.c
	@echo "  CC      $(notdir $@)"
	$(Q)gcc $(CFLAGS) -o $@ $<

$(obj)/%.o: $(src)/%.c
	@echo "  CC      $(notdir $@)"
	$(Q)gcc $(CFLAGS) -Wall -Werror -MMD -MF $@.d -o $@ $<

$(obj)/_$(TARGET).so: $(OBJS) $(obj)/$(TARGET).py
	@echo "  LD      $(notdir $@)"
	$(Q)rm -f $@
	$(Q)gcc -shared $(OBJS) -lftdi1 -o $@

$(obj)/%_wrap.c: $(src)/%.i
	@echo "  SWIG    $(notdir $@)"
	$(Q)swig -python -outdir $(obj) -o $@ $<

clean:
	@rm -rf $(obj)/

$(obj):
	@echo "  MKDIR   $(obj)"
	$(Q)mkdir -p $(obj)

-include $(DEPS)


# TODO: refactor into targets above.
#
# all: bn_test

# CC=gcc
# CFLAGS += -Wall -Wunused -Werror -ggdb -I. -I../../../../third_party/cryptoc/include -Itestlib -DCR50_NO_BN_ASM
# LDFLAGS += -lcrypto

# # Crude dependencies
# DEPS= ../../chip/g/dcrypto/dcrypto.h ../../chip/g/dcrypto/internal.h testlib/common.h testlib/trng.h Makefile

# bn.o: ../../chip/g/dcrypto/bn.c $(DEPS)
# bn_test.o: bn_test.c $(DEPS)
# common.o: testlib/common.c $(DEPS)

# OBJS=../../chip/g/dcrypto/bn.o testlib/common.o

# bn_test: bn_test.c $(DEPS) $(OBJS)
# 	$(CC) $(CFLAGS) bn_test.c -o bn_test $(OBJS) $(LDFLAGS)

# clean:
# 	rm -f bn_test $(OBJS)
