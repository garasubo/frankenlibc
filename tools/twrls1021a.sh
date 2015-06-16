#!/bin/sh

FILE=`basename $*`
arm-none-eabi-objcopy -O binary "$*" /srv/tftp/rumpsample/$FILE
