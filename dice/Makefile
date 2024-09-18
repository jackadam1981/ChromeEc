# Copyright 2015 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

CC ?= gcc
PROGRAM := test_dice
LIBS    :=
CFLAGS  += -std=gnu99 \
	-g \
	-Wall \
	-Werror \
	-Wpointer-arith \
	-Wcast-align \
	-Wcast-qual \
	-Wundef \
	-Wsign-compare \
	-Wredundant-decls \
	-Wmissing-declarations

ifneq ($(DEBUG),)
CFLAGS += -O0
else
CFLAGS += -O3
endif

all: test

test: $(PROGRAM)
	./$(PROGRAM)

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
	rm -rf $(PROGRAM) *~ *.o *.d

-include $(DEPS)
