#!/bin/sh

export CC=${CC-arm-none-eabi-gcc}
export NM=${NM-arm-none-eabi-nm}
export AR=${AR-arm-none-eabi-ar}
export OBJCOPY=${OBJCOPY-arm-none-eabi-objcopy}

# arm-none-eabi-gcc does not turn this off when cpu set
#EXTRA_CPPFLAGS="-D__VFP_FP__"

EXTRA_CFLAGS="-mcpu=cortex-a7"
EXTRA_AFLAGS="-mcpu=cortex-a7"

# this compiler is very fussy, planning to fix these issues at some point
EXTRA_CWARNFLAGS="-Wno-error"

LINKSCRIPT="${PWD}/platform/twrls1021a/link.ld"
EXTRA_LDSCRIPT="-T ${LINKSCRIPT}"
EXTRA_LDSCRIPT_CC="-Wl,-T,${LINKSCRIPT}"


EXTRAFLAGS="-F CPPFLAGS=${EXTRA_CPPFLAGS} -F ACFLAGS=${EXTRA_AFLAGS} -F CWARNFLAGS=${EXTRA_CWARNFLAGS} -F CFLAGS=${EXTRA_CFLAGS} -F CPPFLAGS=-mfpu=neon -F ACLFLAGS=-mfpu=neon -F ACLFLAGS=-mfloat-abi=hard -F CPPFLAGS=-mfloat-abi=hard ${EXTRAFLAGS}"
