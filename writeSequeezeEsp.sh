#!/bin/bash

port=$1
baud=$2
Python esptool.py --port $port --baud $baud  write_flash --flash_mode dio --flash_freq 80m --flash_size detect  0x1000 bootloader/bootloader.bin 0x8000 partitions.bin 0xd000 ota_data_initial.bin 0x10000 recovery.bin 0x150000 squeezelite.bin
