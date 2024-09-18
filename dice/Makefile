# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

CC ?= gcc
PROGRAM := test_dice
LIBS    := -lcrypto
CFLAGS  += -std=gnu99 \
	-g \
	-Wall \
	-Werror \
	-Wpointer-arith \
	-Wcast-align \
	-Wundef \
	-Wsign-compare \
	-Wredundant-decls \
	-Wmissing-declarations
RESULT_FILE := test.cbor

ifneq ($(DEBUG),)
CFLAGS += -O0
else
CFLAGS += -O3
endif

all: test

test: $(PROGRAM)
	./$(PROGRAM) $(RESULT_FILE)

SOURCES := test.c dice.c platform_host.c
OBJS := $(patsubst %.c,%.o,$(SOURCES))
DEPS := $(patsubst %.c,%.d,$(SOURCES))

# chip/g updater
$(PROGRAM): $(OBJS) Makefile
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) $(LIBS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -MMD -MF $(basename $@).d -o $@ $<

.PHONY: clean
clean:
	rm -rf $(PROGRAM) *~ *.o *.d $(RESULT_FILE)

-include $(DEPS)
