#!/bin/sh

export CC=${CC-riscv64-unknown-linux-gnu-gcc}
export NM=${NM-riscv64-unknown-linux-gnu-nm}
export AR=${AR-riscv64-unknown-linux-gnu-ar}
export OBJCOPY=${OBJCOPY-riscv64-unknown-linux-gnu-objcopy}

LINKSCRIPT="${PWD}/platform/pk/link.ld"
EXTRA_LDSCRIPT="-T ${LINKSCRIPT}"
EXTRA_LDSCRIPT_CC="-Wl,-T,${LINKSCRIPT}"

# Tools slow to build, so omit by default
MAKETOOLS="${MAKETOOLS-no}"
