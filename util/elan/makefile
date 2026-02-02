# -------------------------
# NDK and toolchain
# -------------------------
NDK := /home/elan/Desktop/android-ndk-r26b
TOOLCHAIN := $(NDK)/toolchains/llvm/prebuilt/linux-x86_64
TARGET := aarch64-linux-android
API := 21
CXX := $(TOOLCHAIN)/bin/$(TARGET)$(API)-clang++
AR := $(TOOLCHAIN)/bin/llvm-ar
RANLIB := $(TOOLCHAIN)/bin/llvm-ranlib

# -------------------------
# Source files
# -------------------------
SRC := src/iaptool.cpp src/iapcontrol.cpp src/filecontrol.cpp
OBJ := $(SRC:.cpp=.o)
TARGET_BIN := iaptool_android

# -------------------------
# Include paths
# -------------------------
HIDAPI_INC := ./include
LIBUSB_INC := ./include   # libusb.h is here

CXXFLAGS := -std=c++17 -O2 -fPIC -fPIE -I$(HIDAPI_INC) -I$(LIBUSB_INC)

# -------------------------
# Libraries
# -------------------------
HIDAPI_LIB := ./libs/arm64-v8a/libhidapi-libusb.a
LIBUSB_LIB := ./libs/libusb-1.0.a
LDFLAGS := -fPIE -pie $(HIDAPI_LIB) $(LIBUSB_LIB) -landroid -llog -static-libstdc++

# -------------------------
# Build rules
# -------------------------
all: $(TARGET_BIN)

$(TARGET_BIN): $(OBJ)
	$(CXX) $(OBJ) -o $@ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# -------------------------
# Clean
# -------------------------
clean:
	@echo "Cleaning build files..."
	rm -f $(OBJ) $(TARGET_BIN)

.PHONY: all clean

