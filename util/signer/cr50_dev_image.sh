#!/bin/bash
#
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Create a Cr50 nodelocked development firmware binary
#
# This script combines Cr50 nodelocked RO images with the latest locally built
# Cr50 RW imgaes.
#
# Nodelocked Image Build Instructions :
#   1. From the Cr50 console, run the 'sysinfo' command to determine the DEV_ID
#      of your Cr50.
#   2. Request signed and nodelocked Cr50 images matching your DEV_ID.
#   3. Copy nodelocked Cr50 images into the directory
#          platform/ec/private-cr50-nodelocked
#   4. Build the Cr50 firmware
#          (chroot) cd ~/trunk/src/platform/ec
#          (chroot) make CR50_DEV=1 BOARD=cr50 -j
#   5. Run this script command, passing in the DEV_ID of your Cr50.
#          (chroot) ./util/signer/cr50_dev_image.sh "0xAAAAAAAA 0xBBBBBBBB"
#   6. This script creates a nodelocked development image under ./build/cr50
#      matching the DEV_ID passed in.
#          ./build/cr50/cr50.bin.AAAAAAAA_BBBBBBBB
#
# Flashing the node image onto your Cr50:
#   Option 1 - SuzyQ (preferred) -
#     1. Connect a SuzyQ cable to your board
#     2. Add the "-s" option to this script to automatically flash the Cr50
#          (chroot) ./util/signer/cr50_dev_image.sh "0xAAAAAAAA 0xBBBBBBBB" -s
#
#   You can also manually flash the Cr50 by running gsctool from your chroot.
#        (chroot) sudo extra/usb_updater/gsctool \
#                     build/cr50/cr50.bin.AAAAAAAA_BBBBBBBB
#
#   Option 2 - copy the firmware file onto your board, and then run the gsctool
#   utility from the AP.  This requires that your board is on the test network
#   and configured for development mode.
#     1. Copy the firmware file to your board automatically by adding the
#        "-r <ipaddr>" option to this script
#          (chroot) ./util/signer/cr50_dev_image.sh "0xAAAAAAAA 0xBBBBBBBB" \
#                   -r <ipaddr>
#
#        You can manually copy the image to your target using the rsync command
#        from your chroot.
#          (chroot) rsync build/cr50/cr50.bin.AAAAAAAA_BBBBBBBB \
#                      root@<target_ip_addr>:/opt/google/cr50/firmware
#     2. Login as root on the AP.  Then run gsctool to flash the development
#        image on to the Cr50.
#          (ap) gsctool -a /opt/google/cr50/firmware/cr50.bin.AAAAAAAA_BBBBBBBB
#

function usage() {
	cat <<EOF

usage:	$0 -d <dev_id>
   or:	$0 -d <dev_id> -r <ipaddr>
   or:	$0 -d <dev_id> -s
EOF
	exit 1
}

function help() {
	cat <<EOF

NAME
	$0 - Create a nodelocked Cr50 firmware image

SYNOPSIS
	$0 -d <dev_id> [-r <ipaddr>|-s]

DESCRIPTION
	Creates a full Cr50 firmware binary, combining nodelocked RO images
	with the RW images built from the local source.

OPTIONS
	-d <dev_id>
		Specify the DEV_ID of your Cr50, as reported by the 'sysinfo'
		Cr50 console command.  Supported formats include:
		-d aaaaaaaa_bbbbbbbb
		-d AAAAAAAA_BBBBBBBB
		-d "0xAAAAAAAA_0xBBBBBBBBB"
		-d "0xaaaaaaaa 0xbbbbbbbb"
		-d "0xAAAAAAAA 0xBBBBBBBB"

	-r <ipaddr>
		Specifies the IP address of the target.  If this option is
		used, after successful of the Cr50 nodelocked binary, the
		binary is copied, using rsync, onto the target under the
		/opt/google/cr50/firmware directory.  Cannot be used with
		the -s option.

	-s
		Program the Cr50 nodelocked binary to the target using a
		SuzyQ cable.  Requires that the target supports CCD.  Cannot
		be used with the -r option.
EOF

	exit 1
}

DEV_ID=
RSYNC_IPADDR=
SUZYQ=

while [[ $# -gt 0 ]] ; do
	case "$1" in
	-d) DEV_ID="$2"; shift;;
	-r) RSYNC_IPADDR="$2"; shift;;
	-s) SUZYQ=1;;
	-h) help;;
	--help) help;;
	*) break;;
	esac
	shift
done

if [[ -n "${RSYNC_IPADDR}" ]] && [[ -n "${SUZYQ}" ]]; then
	echo "ERROR only one of the -r and -s options may be specified"
	usage
fi

if [[ -z "$DEV_ID" ]]; then
	echo "ERROR \"-d <dev_id>\" parameter missing"
	usage
fi

# Convert Device ID to uppercase.
deviceid="${DEV_ID^^}"

# Remove "0X" and replace spaces with underscore.
deviceid="${deviceid//0X}"
deviceid="${deviceid// /_}"

# Working directory is expected to be platform/ec, all the nodelocked RO
# images are expected to found under 'private-cr50-nodelocked'.
nodelocked_dir=private-cr50-nodelocked

CR50_TEMP_TEMPLATE=cr50.XXXXXX
T=$(mktemp -d --tmpdir "${CR50_TEMP_TEMPLATE}")

cr50_bin_basename="cr50.bin.${deviceid}"
cr50_bin_tmp="${T}/${cr50_bin_basename}"
cr50_bin_final="build/cr50/${cr50_bin_basename}"
cr50_target_dir="/opt/google/cr50/firmware"
cr50_bin_target="${cr50_target_dir}/${cr50_bin_basename}"

# Create a 512 KiB binary filled with 0xFF.
echo "Fill Cr50 binary with 0xFF"
dd if=/dev/zero bs=1k count=512 | tr \\000 \\377 > "${cr50_bin_tmp}"

# Convert nodelocked RO images from hex to binary format.
ro_a_hex="${nodelocked_dir}/B2-dev-cros_loader-A-0-0-selfsigned-${deviceid}.hex"
ro_b_hex="${nodelocked_dir}/B2-dev-cros_loader-B-0-0-selfsigned-${deviceid}.hex"

if [[ -e "${ro_a_hex}" ]] ; then
	echo "Copy nodelocked RO A binary"
	objcopy -I ihex "${ro_a_hex}" -O binary "${T}/ro.A.bin" || die
	dd if="${T}/ro.A.bin" of="${cr50_bin_tmp}" conv=notrunc || die
else
	# Node locked image not found
	die "Nodelocked Cr50 image ${ro_a_hex} does not exist"
fi

if [[ -e "${ro_b_hex}" ]] ; then
	echo "Copy nodelocked RO B binary"
	objcopy -I ihex "${ro_b_hex}" -O binary "${T}/ro.B.bin" || die
	dd if="${T}/ro.B.bin" of="${cr50_bin_tmp}" \
		conv=notrunc bs=1 seek=262144 || die
else
	# Node locked image not found.
	die "Nodelocked Cr50 image ${ro_b_hex} does not exist"
fi

# Copy the Cr50 RW images built locally directly into the unified
# binary image.
echo "Copy RW A binary"
dd if=build/cr50/RW/ec.RW.flat of="${cr50_bin_tmp}" \
	conv=notrunc bs=1 seek=16384 || die
echo "Copy RW B binary"
dd if=build/cr50/RW/ec.RW_B.flat of="${cr50_bin_tmp}" \
	conv=notrunc bs=1 seek=278528 || die

# Copy the final binary from a the temporary directory to the build directory.
echo
echo "Creating final binary ${cr50_bin_final}"
cp "${cr50_bin_tmp}" "${cr50_bin_final}"

# Delete the temporary files.
rm -rf ${T}

if [[ -n "${RSYNC_IPADDR}" ]]; then
	# Perform an rsync operation to push the nodelocked binary onto the
	# target.
	echo
	echo "Transferring ${cr50_bin_final} to target at IP ${RSYNC_IPADDR}"
	rsync "${cr50_bin_final}" root@"${RSYNC_IPADDR}":"${cr50_target_dir}"

	if [[ "$?" -eq 0 ]]; then
		echo "Transfer to target ${RSYNC_IPADDR} successful."
		echo "On the target, run this command to flash the image to" \
			"the Cr50."
		echo "gsctool -a ${cr50_bin_target}"
	else
		echo "Transfer failed with exit code $?."
		exit 1
	fi
fi

if [[ -n "${SUZYQ}" ]]; then
	# Run the gsctool from the chroot to flash the Cr50 over the SuzyQ
	# cable.
	echo
	echo "Flashing ${cr50_bin_final} to target using SuzyQ cable"
	sudo extra/usb_updater/gsctool "${cr50_bin_final}"

	# Note that gsctool completes with exit code on both pass and failure.
	# However, gsctool provides sufficient status information on the
	# console to determine pass/fail.
fi
