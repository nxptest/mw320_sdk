#!/bin/bash

if py -3 -V &> /dev/null ; then
  IMG_CONV="py -3 mw_img_conv.py"
elif python3 -V &> /dev/null ; then
  IMG_CONV="python3 mw_img_conv.py"
else
  IMG_CONV="python mw_img_conv.py"
fi

if [ "$#" -ne 1 ] ; then
  echo "Usage: $0 <mcu_firmware>"
  exit 1
fi

path=`dirname $1`
name=`basename $1 .bin`
addr=`head -c8 $1 | od -An -t x1 -j7 | xargs`

if [[ "$addr" == "1f" ]]; then
    echo "$IMG_CONV mcufw $1 $path/$name.fw.bin 0x1F000100"
    $IMG_CONV mcufw $1 $path/$name.fw.bin 0x1F000100
else
    echo "$IMG_CONV mcufw $1 $path/$name.fw.bin 0x00100000"
    $IMG_CONV mcufw $1 $path/$name.fw.bin 0x00100000
fi
