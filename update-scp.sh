#!/bin/bash

if [ "$#" == "0" ]; then
	echo "Usage: $0 <DUT IP>"
	exit
fi

IP=$1
echo "[[[ DUT: $IP ]]]"
#echo '>>> setup ssh key'
#ssh-keygen -f "$HOME/.ssh/known_hosts" -R $IP
#ssh-copy-id -i ~/.ssh/id_rsa.pub root@$IP
echo '>>> ec.obj md5sum at local'
md5sum build/cherry_scp/ec.obj
echo '>>> tx to DUT'
scp build/cherry_scp/ec.obj root@$IP:/lib/firmware/scp.img
echo '>>> scp.img md5sum on DUT'
ssh root@$IP "sync && md5sum /lib/firmware/scp.img"
echo '>>> reboot DUT'
ssh root@$IP reboot
