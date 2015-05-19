#!/bin/sh

export CC=${CC-arm-none-eabi-gcc}
export NM=${NM-arm-none-eabi-nm}
export AR=${AR-arm-none-eabi-ar}
export OBJCOPY=${OBJCOPY-arm-none-eabi-objcopy}

# arm-none-eabi-gcc does not turn this off when cpu set
#EXTRA_CPPFLAGS="-D__VFP_FP__"

EXTRA_CFLAGS="-mcpu=cortex-a7 -mfpu=neon -mfloat-abi=hard -marm -mthumb-interwork"
EXTRA_AFLAGS="-mcpu=cortex-a7 -mfpu=neon -mfloat-abi=hard"

# this compiler is very fussy, planning to fix these issues at some point
EXTRA_CWARNFLAGS="-Wno-error"

LINKSCRIPT="${PWD}/platform/twrls1021a/link.ld"
EXTRA_LDSCRIPT="-T ${LINKSCRIPT}"
EXTRA_LDSCRIPT_CC="-Wl,-T,${LINKSCRIPT}"
