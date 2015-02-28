#!/bin/bash

port=$1
hoho_mcdp_update_rw=$2
mcdp_fw=$3
hoho_rw=$4

ec_cmd='ectool --name cros_pd'
flashrom_cmd='flashrom.hoho -p raiden_debug_spi -c W25Q80' 

function _prn {
   local prefix=$1
   local str=$2
   local dstr=`date +"%Y-%m-%d %T"`
   echo "$prefix [${dstr}] $str"
}

function prn_info {
    _prn "-I-" "$1"
}

function prn_warn {
    _prn "-W-" "$1"
}

function prn_fatal {
    _prn "-F-" "$1"
    exit -1
}

function is_hoho {
    $ec_cmd infopddev $port 2>&1 | grep -q "4\.2" > /dev/null
    if [[ "$?" -ne "0" ]] ; then
        prn_fatal "No hoho on port $port"
    fi
}

function is_hoho_rw {
    $ec_cmd infopddev $port 2>&1 | grep -q "CurImg:RW"
    echo $?
}

function hoho_power_cycle {
        $ec_cmd usbpd $port sink
        $ec_cmd usbpd $port auto
        sleep 3
}

function is_hoho_usb {
    local tries=$1
    if [[ "$tries" = "" ]] ; then
        tries=1
    fi

    # seems to be some race for USB enumeration so repeat power-cycle 
    for i in `seq $tries` ; do
        hoho_power_cycle
        lsusb -d 18d1:5010 >/dev/null
        if [[ "$?" -eq "0" ]] ; then
            sleep 1
            return
        else
            echo "hoho power cycle retry $i"
        fi
    done
    lsusb -d 18d1:5010 >/dev/null
    if [[ "$?" -ne "0" ]] ; then
        prn_fatal "Can not find hoho usb enumerated"
    fi
}

function flash_hoho_rw_fw {
    local fw=$1
    local tries=$2
    local rv1=1

    if [[ "$tries" = "" ]] ; then
        tries=1
    fi

    if [ ! -e "$fw" ] ; then
        prn_fatal "Unable to find hoho fw $fw"
    fi

    for i in `seq $tries` ; do
        $ec_cmd flashpd 4 $port $fw &> /dev/null
	rv1=$?
	sleep 4 # reboot time
        if [[ "$rv1" -ne "0" ]] ; then
            prn_warn "cnt${i}: Failed writing hoho fw"
            continue
	else
            rv1=`is_hoho_rw`
            if [[ "$rv1" -ne "0" ]] ; then
                prn_warn "hoho not in RW after flash :: $rv1"
                continue
            fi
        fi
        break
    done

    if [[ "$rv1" -ne "0" ]] ; then
        prn_fatal "Failed writing hoho fw"
    fi
}

function check_mcdp_sha1 {
    local mcdp_src_fw=$1
    local mcdp_rd_fw=$2

    if [ ! -e "$mcdp_src_fw" ] ; then
        prn_fatal "Can not find $mcdp_src_fw"
    fi

    if [ ! -e "$mcdp_rd_fw" ] ; then
        prn_fatal "Can not find $mcdp_rd_fw"
    fi

    local src=`sha1sum -b $mcdp_src_fw | cut -d' ' -f1`
    local rd=`sha1sum -b $mcdp_rd_fw | cut -d' ' -f1`
    [[ "$src" = "$rd" ]]
}

function flash_mcdp_fw {
    local fw=$1
    local tries=$2

    local tmpfile="/tmp/bobcat_rd.bin"
    local rv1=1
    local rv2=1

    if [ ! -e "$fw" ] ; then
        prn_fatal "Unable to find mcdp fw $fw"
    fi

    while true ; do
	$flashrom_cmd &> /dev/null
        if [[ "$?" -eq "0" ]] ; then
            break
        fi
        prn_warn "Failed to find Megachips EEPROM ... retrying"
        hoho_power_cycle
        sleep 5
    done

    for i in `seq $tries` ; do
        $flashrom_cmd --noverify -w $fw &> /dev/null
        rv1=$?
        if [[ "$rv1" -ne "0" ]] ; then
            prn_warn "cnt${i} Flashing megachips failed"
            is_hoho_usb 3
            continue
        fi

        # read back and verify checksum as flashrom verify isn't working reliably
        for j in `seq $tries` ; do
            $flashrom_cmd -r $tmpfile &> /dev/null
            if [[ "$?" -ne "0" ]] ; then
                prn_warn "cnt${j} Reading megachips failed"
                continue
            fi
            break
        done
        check_mcdp_sha1 $fw $tmpfile
	rv2=$?
        if [[ "$rv2" -ne "0" ]] ; then
            prn_warn "cnt${i} Verifying megachips failed"
	    continue
        fi
        break
    done
    if [[ "$rv1" -ne "0" || "$rv2" -ne "0" ]] ; then
        prn_fatal "Failed to flash and verify Megachips"
    fi
}

prn_info "HOHO SPI RW FW : $hoho_mcdp_update_rw"
prn_info "MCDP FW        : $mcdp_fw"
prn_info "HOHO PROD FW   : $hoho_rw"
prn_info "Starting on port $port ..."

# disable USB compliance ( paranoia )
iotools mmio_write32 0xe12080ec 0x18010c01

is_hoho
flash_hoho_rw_fw $hoho_mcdp_update_rw 3
prn_info "Hoho RW re-programmed to allow Megachips SPI writing"

is_hoho_usb 3
flash_mcdp_fw $mcdp_fw 5
prn_info "Megachips SPI re-programmed"

flash_hoho_rw_fw $hoho_rw 3
hoho_power_cycle
prn_info "Hoho RW re-programmed to production FW"

is_hoho
prn_info "Done!"
