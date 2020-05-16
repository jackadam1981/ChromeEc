obj-m := i2c-pseudo.o

.PHONY: all

all: modules

CFLAGS_i2c-pseudo.o += "-DHAVE_STREAM_OPEN=$(shell "$M"/check_stream_open.sh)"

.DEFAULT:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build \
		M=$(shell pwd) \
		$(MAKECMDGOALS)
