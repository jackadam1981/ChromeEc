all: spiflash

ROOT=../
COMMON=../common
FTDI=../third_party/libftdi/src
MPSSE=../third_party/libmpsse/src

vpath %.h $(COMMON) $(FTDI) $(FTDI)/../build/src $(MPSSE)
vpath %.c $(COMMON) $(FTDI) $(MPSSE)
vpath %.cc $(COMMON)

CFLAGS += -Wall \
	-I $(ROOT) \
	-I /usr/include/libusb-1.0/ \
	-I $(FTDI) -I $(FTDI)/../build/src \
	-I $(MPSSE) \
	-I $(COMMON)
CXXFLAGS += $(CFLAGS) -std=c++0x
LDFLAGS += -lusb-1.0 -lcrypto

FTDI_OBJS=ftdi.o ftdi_stream.o
MPSSE_OBJS=mpsse.o support.o fast.o
LIBOBJS=intelhex.o publickey.o

# Crude dependencies
DEPS=config.h mpsse.h support.h ftdi.h ftdi_i.h ftdi_version_i.h Makefile \
		 intelhex.h publickey.h

ftdi.o: ftdi.c $(DEPS)
ftdi_stream.o: ftdi_stream.c $(DEPS)
mpsse.o: mpsse.c $(DEPS)
support.o: support.c $(DEPS)
fast.o: fast.c $(DEPS)
intelhex.o: intelhex.cc $(DEPS)
publickey.o: publickey.cc $(DEPS)

spiflash: spiflash.cc $(FTDI_OBJS) $(MPSSE_OBJS) $(LIBOBJS) $(DEPS)
	$(CXX) $(CXXFLAGS) spiflash.cc -o spiflash $(FTDI_OBJS) $(MPSSE_OBJS) $(LIBOBJS) $(LDFLAGS)

clean:
	rm -f spiflash $(FTDI_OBJS) $(MPSSE_OBJS) $(LIBOBJS)
