#!/bin/sh

if [ "$*" == "" ]; then
	echo "PASS EC binary"
	exit 1
fi

rm -f spi_fw.bin
cp $1 spi_fw.bin


python3 MakePgmHdr.py
sleep 1
dut-control cold_reset:on fw_up:on
sleep 1
dut-control cold_reset:off
sleep 1
dut-control uart1_baudrate:9600
sleep 1
dut-control uart1_baudrate
python3 UART_CR_V2.py

dut-control uart1_baudrate:57600
sleep 1
dut-control uart1_baudrate
python3 CrisisRcvry_Utility_Final.py

sleep 1
dut-control cold_reset:on fw_up:off
sleep 1
# dut-control cold_reset:off

dut-control uart1_baudrate:115200

