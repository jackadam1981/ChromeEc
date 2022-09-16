#!/bin/bash

WORKDIR=/home/mtk15399/chromiumos/src/platform/ec
IP=172.18.193.23
#BOARD=dragonfruit
BOARD=cherry

#make BOARD=dragonfruit_scp_core0
#make BOARD=${BOARD}_scp
#make BOARD=${BOARD}_scp_core1

#scp $WORKDIR/build/cherry_scp/ec.obj root@$IP:/lib/firmware/scp.img
scp $WORKDIR/build/${BOARD}_scp/ec.obj root@$IP:/lib/firmware/scp.img
scp $WORKDIR/build/${BOARD}_scp_core1/ec.obj root@$IP:/lib/firmware/scp-dual.img
md5sum $WORKDIR/build/${BOARD}_scp/ec.obj
ssh root@$IP "md5sum /lib/firmware/scp.img"
md5sum $WORKDIR/build/${BOARD}_scp_core1/ec.obj
ssh root@$IP "md5sum /lib/firmware/scp-dual.img"
ssh root@$IP "sync; reboot;"
