#!/bin/bash
port=$1
fw_sign_type=$2
myp="/usr/local/bin"

if [[ "$port" = "" ]] ; then
    port=0
fi

if [[ "$fw_sign_type" = "" ]] ; then
    fw_type="mp"
fi

if [[ "$fw_sign_type" = "dev" ]] ; then
    ${myp}/hoho_mcdp_reflash.sh $port \
        ${myp}/hoho_v1.7.633-7ead931.ec.RW.bin \
        ${myp}/Bobcat_C2_PCON_GG_8M_V0.53.1024k.bin \
        ${myp}/hoho_v1.7.575-96b74f1.ec.RW.bin
else
    ${myp}/hoho_mcdp_reflash.sh $port \
        ${myp}/ec-Hoho_v1.7.633-7ead931.HohoMPSigned.RW.bin \
        ${myp}/Bobcat_C2_PCON_GG_8M_V0.53.1024k.bin \
        ${myp}/ec-Hoho_v1.7.575-96b74f1_6300.89.0.HohoMPSigned.RW.bin
fi
