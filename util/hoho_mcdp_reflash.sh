#!/bin/bash

port=$1
hoho_mcdp_update_rw=$2
mcdp_fw=$3
hoho_rw=$4

ec_cmd='ectool --name cros_pd'
flashrom_cmd='flashrom.hoho -p raiden_debug_spi -c W25Q80' 

function prn_fatal {
    echo "-F- $1"
    exit -1
}

function is_hoho {
    $ec_cmd infopddev $port 2>&1 | grep -q "4\.2" > /dev/null
    if [[ $? -ne 0 ]] ; then
        prn_fatal "No hoho on port $port"
    fi
}

function power_cycle_hoho {
        $ec_cmd usbpd $port sink
        sleep 1
        $ec_cmd usbpd $port auto
        sleep 2
}

function is_hoho_usb {
    # seems to be some race for USB enumeration so repeat power-cycle 
    for i in `seq 10` ; do
        power_cycle_hoho
        lsusb -d 18d1:5010 >/dev/null
        if [[ $? -eq 0 ]] ; then
            return
        else
            echo "hoho power cycle retry $i"
        fi
    done
    lsusb -d 18d1:5010 >/dev/null
    if [[ $? -ne 0 ]] ; then
        prn_fatal "Can't find hoho usb enumerated"
    fi
}

function flash_hoho_fw {
    local fw=$1
    if [ ! -e "$fw" ] ; then
        prn_fatal "Unable to find hoho fw $fw"
    fi
    $ec_cmd flashpd 4 $port $fw
}

function flash_mcdp_fw {
    local fw=$1
    if [ ! -e "$fw" ] ; then
        prn_fatal "Unable to find mcdp fw $fw"
    fi
    #$flashrom_cmd -r bobcat_read.bin
    $flashrom_cmd --wp-status
    $flashrom_cmd -w $fw
    if [[ $? -ne 0 ]] ; then
        prn_fatal "Flashing megachips failed"
    fi
}

# disable USB compliance
iotools mmio_write32 0xe12080ec 0x18010c01
is_hoho
flash_hoho_fw $hoho_mcdp_update_rw
is_hoho_usb
flash_mcdp_fw $mcdp_fw
sleep 5
flash_hoho_fw $hoho_rw
power_cycle_hoho
sleep 5
