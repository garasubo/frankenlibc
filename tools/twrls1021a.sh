#!/bin/sh

FILE=`basename $*`
sudo arm-none-eabi-objcopy -O binary "$*" /srv/tftp/rumpsample/$FILE
