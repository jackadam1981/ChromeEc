# Fingerprint Library Compiler Flags

## Toolchain

Fingerprint uses LLVM.


## Flags

### Bloonchipper

```bash
# From the following build command:
make BOARD=bloonchipper V=1 build/bloonchipper/RW/core/cortex-m/mpu.o

armv7m-cros-eabi-clang
-std=gnu11
-Wstrict-prototypes
-Wno-pointer-sign
-Werror-implicit-function-declaration
-Wno-ignored-attributes
-mcpu=cortex-m4
-mcpu=cortex-m4
-mthumb
-Oz
-Wl,-mllvm
-Wl,-inline-threshold=-10
-mno-unaligned-access
-mfloat-abi=hard
-g
-ftrapv
-Wall
-Wundef
-Wno-trigraphs
-Wno-format-security
-Wno-address-of-packed-member
-fno-common
-fno-strict-aliasing
-fno-strict-overflow
-Wimplicit-fallthrough
-fno-exceptions
-fno-unwind-tables
-fno-asynchronous-unwind-tables
-Werror
-Werror=uninitialized
-Wno-unused-function
-ffunction-sections
-fno-delete-null-pointer-checks
-fno-PIC
```

### Dartmonkey

```bash
# From the following build command:
make BOARD=dartmonkey V=1 build/dartmonkey/RW/core/cortex-m/mpu.o
```

```bash
armv7m-cros-eabi-clang
-std=gnu11
-Wstrict-prototypes
-Wno-pointer-sign
-Werror-implicit-function-declaration
-Wno-ignored-attributes
-fno-PIC
-mcpu=cortex-m7
-mcpu=cortex-m7
-mthumb
-Oz
-Wl,-mllvm
-Wl,-inline-threshold=-10
-mno-unaligned-access
-mfloat-abi=hard
-g
-ftrapv
-Wall
-Wundef
-Wno-trigraphs
-Wno-format-security
-Wno-address-of-packed-member
-fno-common
-fno-strict-aliasing
-fno-strict-overflow
-Wimplicit-fallthrough
-fno-exceptions
-fno-unwind-tables
-fno-asynchronous-unwind-tables
-Werror
-Werror=uninitialized
-Wno-unused-function
-ffunction-sections
-fno-delete-null-pointer-checks
-fno-PIC
```


## Example Compilation Line

### For C Source

```bash
/usr/bin/ccache armv7m-cros-eabi-clang -std=gnu11 -Wstrict-prototypes -Wno-pointer-sign -Werror-implicit-function-declaration -Wno-ignored-attributes -DOUTDIR=build/bloonchipper/RW -DCHIP=stm32 -DBOARD_TASKFILE=ec.tasklist -DBOARD=bloonchipper -DCORE=cortex-m -DPROJECT=ec -DCHIP_VARIANT=stm32f412 -DCHIP_FAMILY=STM32F4 -DBOARD_BLOONCHIPPER= -DCHIP_STM32= -DCORE_CORTEX_M= -DCHIP_VARIANT_STM32F412= -DCHIP_FAMILY_STM32F4= -DFINAL_OUTDIR=build/bloonchipper -DPROTOBUF_MIN_PROTOC_VERSION=0  -Iinclude  -Icore/cortex-m/include  -Iinclude/driver  -Icore/cortex-m  -Ichip/stm32  -Iboard/bloonchipper  -Iboard/bloonchipper  -Icommon  -Ifuzz  -Ipower  -Itest  -Icts/common  -Icts/  -Ibuild/bloonchipper/gen  -Iprivate  -Icommon  -Icommon/fpsensor  -Icommon/vboot  -Icommon/spi  -Icommon/spi/flash_reg  -Icommon/spi/flash_reg/private  -Icommon/spi/flash_reg/src  -Icommon/spi/flash_reg/public  -Icommon/usbc  -Icommon/mock  -Idriver  -Idriver/ioexpander  -Idriver/retimer  -Idriver/temp_sensor  -Idriver/sha256  -Idriver/cec  -Idriver/bc12  -Idriver/nfc  -Idriver/led  -Idriver/usb_mux  -Idriver/fingerprint  -Idriver/fingerprint/elan  -Idriver/fingerprint/fpc  -Idriver/fingerprint/fpc/libfp  -Idriver/fingerprint/fpc/bep  -Idriver/ppc  -Idriver/charger  -Idriver/battery  -Idriver/tcpm  -Idriver/wpc  -Ilibc  -Ithird_party/boringssl/common  -Icrypto  -Ibuild/bloonchipper  -Ifuzz  -Itest  -Ithird_party  -Ispi/flash_regpublic   -I.    -DTEST_ec= -DTEST_EC=        -Iinclude/driver  -DSECTION_IS_RW= -DSECTION=RW -DHAS_TASK_FPSENSOR= -DHAS_TASK_RWSIG_RO= -fno-PIC -DCHROMIUM_EC= -DHAVE_PRIVATE -DHAS_TASK_HOOKS= -DHAS_TASK_HOSTCMD= -DHAS_TASK_CONSOLE=  -I../../third_party/boringssl -I../../third_party/boringssl/include -I/mnt/host/source/src/platform/ec/third_party/boringssl/include -D__TRUSTY__ -mcpu=cortex-m4 -mcpu=cortex-m4 -mthumb -Oz		 -Wl,-mllvm -Wl,-inline-threshold=-10 -mno-unaligned-access -mfloat-abi=hard -g  -ftrapv -Wall -Wundef -Wno-trigraphs -Wno-format-security -Wno-address-of-packed-member -fno-common -fno-strict-aliasing -fno-strict-overflow -Wimplicit-fallthrough -fno-exceptions -fno-unwind-tables -fno-asynchronous-unwind-tables -Werror -Werror=uninitialized -Wno-unused-function  -ffunction-sections -fno-delete-null-pointer-checks -fno-PIC -MMD -MP -MF build/bloonchipper/RW/core/cortex-m/mpu.o.d -c core/cortex-m/mpu.c -MT build/bloonchipper/RW/core/cortex-m/mpu.o -o build/bloonchipper/RW/core/cortex-m/mpu.o
```

### For C++ Source

```bash
armv7m-cros-eabi-clang++ -std=gnu++2a -DOUTDIR=build/bloonchipper/RW -DCHIP=stm32 -DBOARD_TASKFILE=ec.tasklist -DBOARD=bloonchipper -DCORE=cortex-m -DPROJECT=ec -DCHIP_VARIANT=stm32f412 -DCHIP_FAMILY=STM32F4 -DBOARD_BLOONCHIPPER= -DCHIP_STM32= -DCORE_CORTEX_M= -DCHIP_VARIANT_STM32F412= -DCHIP_FAMILY_STM32F4= -DFINAL_OUTDIR=build/bloonchipper -DPROTOBUF_MIN_PROTOC_VERSION=0  -Iinclude  -Icore/cortex-m/include  -Iinclude/driver  -Icore/cortex-m  -Ichip/stm32  -Iboard/bloonchipper  -Iboard/bloonchipper  -Icommon  -Ifuzz  -Ipower  -Itest  -Icts/common  -Icts/  -Ibuild/bloonchipper/gen  -Iprivate  -Icommon  -Icommon/fpsensor  -Icommon/vboot  -Icommon/spi  -Icommon/spi/flash_reg  -Icommon/spi/flash_reg/private  -Icommon/spi/flash_reg/src  -Icommon/spi/flash_reg/public  -Icommon/usbc  -Icommon/mock  -Idriver  -Idriver/ioexpander  -Idriver/retimer  -Idriver/temp_sensor  -Idriver/sha256  -Idriver/cec  -Idriver/bc12  -Idriver/nfc  -Idriver/led  -Idriver/usb_mux  -Idriver/fingerprint  -Idriver/fingerprint/elan  -Idriver/fingerprint/fpc  -Idriver/fingerprint/fpc/libfp  -Idriver/fingerprint/fpc/bep  -Idriver/ppc  -Idriver/charger  -Idriver/battery  -Idriver/tcpm  -Idriver/wpc  -Ilibc  -Ithird_party/boringssl/common  -Icrypto  -Ibuild/bloonchipper  -Ifuzz  -Itest  -Ithird_party  -Ispi/flash_regpublic  -I.    -DTEST_ec= -DTEST_EC=        -Iinclude/driver  -DSECTION_IS_RW= -DSECTION=RW -DHAS_TASK_FPSENSOR= -DHAS_TASK_RWSIG_RO= -fno-PIC -DCHROMIUM_EC= -DHAVE_PRIVATE -DHAS_TASK_HOOKS= -DHAS_TASK_HOSTCMD= -DHAS_TASK_CONSOLE=  -I../../third_party/boringssl -I../../third_party/boringssl/include -I/mnt/host/source/src/platform/ec/third_party/boringssl/include -D__TRUSTY__ -mcpu=cortex-m4 -mcpu=cortex-m4 -mthumb -Oz		 -Wl,-mllvm -Wl,-inline-threshold=-10 -mno-unaligned-access -mfloat-abi=hard -g  -ftrapv -Wall -Wundef -Wno-trigraphs -Wno-format-security -Wno-address-of-packed-member -fno-common -fno-strict-aliasing -fno-strict-overflow -Wimplicit-fallthrough -fno-exceptions -fno-unwind-tables -fno-asynchronous-unwind-tables -Werror -Werror=uninitialized -Wno-unused-function  -ffunction-sections -fno-delete-null-pointer-checks -fno-PIC -Wvla -ftrapv -Wall -Wundef -Wno-trigraphs -Wno-format-security -Wno-address-of-packed-member -fno-common -fno-strict-aliasing -fno-strict-overflow -Wimplicit-fallthrough -fno-exceptions -fno-unwind-tables -fno-asynchronous-unwind-tables -Werror -Werror=uninitialized -Wno-unused-function -DPROTOBUF_INLINE_NOT_IN_HEADERS=0 -MMD -MP -MF build/bloonchipper/RW/common/fpsensor/fpsensor_crypto.o.d -c common/fpsensor/fpsensor_crypto.cc -MT build/bloonchipper/RW/common/fpsensor/fpsensor_crypto.o -o build/bloonchipper/RW/common/fpsensor/fpsensor_crypto.o
```
