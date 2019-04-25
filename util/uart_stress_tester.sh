#!/bin/bash
#
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# ChromeOS UART Stress Test
#
# This script compares the UART output on a console command against the expected
# output, and checks if there are any lost characters.
#
# Use: Run uart_stress_tester.h --help.
#      The current version of this script runs either with or without servod.
#
# Output: At the end of test, character loss rates are displayed on screen.
#
# How it works:
#          1. Get UART device file path(s)
#          2. Run a console command on each UART, and capture the output for
#             a base text in comparison.
#          3. Run a console command on a UART or more multiple times,
#             and capture the output.
#          4. Compare the captured output against the base text, and check
#             if the base pattern is repeated or if the character is lost.
#          5. Print the result

SCRIPT="$(readlink -f "$0")"
SCRIPT_DIR="$(dirname "$SCRIPT")"

# Load chromeOS common bash library
if [[ "${SCRIPT_DIR}" =~ "ec/util" ]]; then
	COMMON_SH_DIR="${SCRIPT_DIR}/../../../scripts"
else
	COMMON_SH_DIR="~/trunk/src/scripts"
fi
. "${COMMON_SH_DIR}/common.sh" || exit 1

# Loads script libraries.
. "/usr/share/misc/shflags" || exit 1

DEFAULT_PORT="${SERVOD_PORT:-9999}"
CR50_VID="18d1"
CR50_PID="5014"
CR50_VPID="${CR50_VID}:${CR50_PID}"
SAVEIFS=${IFS}

#Flags
DEFINE_boolean cr50  "${FLAGS_FALSE}" "Test cr50 console"
DEFINE_boolean cpu   "${FLAGS_FALSE}" "Test cpu (AP) console"
DEFINE_boolean ec    "${FLAGS_FALSE}" "Test ec console"
DEFINE_boolean clean "${FLAGS_FALSE}" "Delete all log data after test"
DEFINE_string serial "" "CR50 serial number, e.g. XXXXXXXX-XXXXXXXX"
DEFINE_integer port  "${DEFAULT_PORT}" "Port to communicate to servo on."
DEFINE_integer min_char  "40000" "Minimum number of characters to generate."

FLAGS_HELP="usage: $0 [flags]"
FLAGS "$@" || exit 1
eval set -- "${FLAGS_ARGV}"
if [[ $# -gt 0 ]] ; then
	die "invalid arguments: \"$*\""
fi

set -e

info "ChromeOS UART stress test starts."

dut_control() {
	"dut-control" "--port=${FLAGS_port}" "$@" 2>/dev/null
}

declare -A DEV_PATH
if dut_control -i &>/dev/null ; then
	# servod is running.
	USE_SERVOD=true
	SERVO_TYPE="$( dut_control -o "servo_type" )"

	# Get CR50 FW version
	if dut_control -i cr50_version &>/dev/null ; then
		CR50_VERSION="$( dut_control -o "cr50_version" )"
	fi
else
	# servod is not running.
	USE_SERVOD=false
	SERVO_TYPE="ccd_cr50 without servod"
	IFS="
"
	# Find a CR50 device serial number
	if [[ -n "${FLAGS_serial}" ]]; then
		SERIAL="${FLAGS_serial}"
	else
		SERIALS=( $( lsusb -vd ${CR50_VPID} | grep iSerial ) ) || :
		case ${#SERIALS[@]} in
			0) die "Found no CR50 devices"
				;;
			1) SERIAL="${SERIALS[0]##* }"        # get the last word
				;;
			*) warn "Rerun the script with one of flags below:"
			   for i in "${SERIALS[@]}"; do
				warn "    --serial ${i##* }"
			   done
			   die "Found too many CR50 devices"
				;;
		esac
	fi
	# Find a CR50 device directory
	DEVDIR="$( grep -l -r "${SERIAL}" /sys/devices 2>/dev/null )" || :
	DEVDIR="${DEVDIR%/*}"   # Trim the filename, so that dir name can remain
	[[ -n "${DEVDIR}" ]] || die "Wrong serial number: ${SERIAL}"]]
	[[ -d "${DEVDIR}" ]] || die "Cannot find a directory, ${DEVDIR}"

	# Find each UART console device paths
	DEVPATH=( $( grep "ttyUSB" -hr ${DEVDIR} 2>/dev/null | sort ) )
	if [[ ${#DEVPATH[@]} -eq 0 ]]; then
		warn "Replug the Suzy-Q."
		die "No /dev/ttyUSB devices detected"
	elif [[ ${#DEVPATH[@]} -lt 3 ]]; then
		die "Not enough console devices: ${DEVPATH[@]}"
	fi

	# Trim "^DEVNAME=" from each DEVPATH[]
	DEV_PATH["CR50"]="/dev/${DEVPATH[0]#*=}"
	DEV_PATH["CPU"]="/dev/${DEVPATH[1]#*=}"
	DEV_PATH["EC"]="/dev/${DEVPATH[2]#*=}"

	# Get CR50 FW version
	# TODO(namyoon@): after crrev.com/c/1600501 lands,
	#                 use gsctool -n ${SERIAL}
	CR50_VERSION="$( gsctool -f 2>/dev/null)"
	CR50_VERSION="${CR50_VERSION##*RW }"
	CR50_VERSION+="/$( cat ${DEVDIR}/configuration )"
fi

info "\tservo_type\t= ${SERVO_TYPE}"
info "\tCR50 version\t= ${CR50_VERSION}"

#######################################
# Get a device path.
# Arguments:
#   $1: Target UART. should be either "CR50", "EC", or "CPU"
#######################################
get_uart_pty() {
	local UART="${1,,}"

	# Device path was not given as argument.
	if ${USE_SERVOD}; then
		RETVAL=$( dut_control -o "${UART}_uart_pty" ) || \
			die "${FUNCNAME[0]}: Failed to get ${UART}_uart_pty}"
	else
		RETVAL="${DEV_PATH["${UART^^}"]}"
	fi
	echo "${RETVAL}"
}

UART_COUNT=0
if [[ "${FLAGS_cr50}" -eq "${FLAGS_TRUE}" ]] ; then
	PTY_CR50="$(get_uart_pty "CR50")"
	[[ -e "${PTY_CR50}" ]] || die "${PTY_CR50} (CR50) does not exist"
	info "\tCR50 pty\t= ${PTY_CR50}"

	(( UART_COUNT++ )) || :
fi
if [[ "${FLAGS_cpu}" -eq "${FLAGS_TRUE}" ]] ; then
	PTY_CPU="$(get_uart_pty "CPU")"
	[[ -e "${PTY_CPU}" ]] || die "${PTY_CPU} (CPU) does not exist"
	info "\tCPU pty\t= ${PTY_CPU}"

	(( UART_COUNT++ )) || :
fi
if [[ "${FLAGS_ec}" -eq "${FLAGS_TRUE}" ]] ; then
	PTY_EC="$(get_uart_pty "EC")"
	[[ -e "${PTY_EC}" ]] || die "${PTY_EC} (EC) does not exist"
	info "\tEC pty\t\t= ${PTY_EC}"

	(( UART_COUNT++ )) || :
fi

[[ ${UART_COUNT} -gt 0 ]] || \
	die "Rerun the script at least one among --cr50, --cpu, or --ec."

# Suppress console output and disable ec3po's timestamp output.
declare -a DUT_CTRL_SAVE
suppress_output() {
	local UART="${1,,}"
	local PTY="$2"

	if [[ "${UART}" != "CPU" ]]; then
		echo 'chan save' > "${PTY}"
		echo 'chan 0' > "${PTY}"
	fi

	if ${USE_SERVOD}; then
		DUT_CTRL_SAVE+=( "$( dut_control "${UART}_uart_timestamp" )" )
		dut_control "${UART}_uart_timestamp:off"
	fi
}

[[ -e "${PTY_CR50}" ]] && suppress_output "CR50" "${PTY_CR50}"
[[ -e "${PTY_CPU}" ]] && suppress_output "CPU" "${PTY_CPU}"
[[ -e "${PTY_EC}" ]] && suppress_output "EC" "${PTY_EC}"

# Get sample of text to compare for.
DIR_TMP="$(mktemp -d --suffix=.$(basename ${SCRIPT}))"
SAMPLE_CR50="${DIR_TMP}/sample_cr50.cap"
SAMPLE_CPU="${DIR_TMP}/sample_cpu.cap"
SAMPLE_EC="${DIR_TMP}/sample_ec.cap"

# Trap function on EXIT
cleanup() {
	# Restore channel setting
	[[ -e "${PTY_CR50}" ]] &&  echo 'chan restore' > "${PTY_CR50}"
	[[ -e "${PTY_EC}" ]]   &&  echo 'chan restore' > "${PTY_EC}"

	if ${USE_SERVOD}; then
		# This is to prevent uart_caputure from leaving enabled
		# accidentally.

		# Drain the buffers
		dut_control "cr50_uart_stream" &>/dev/null
		dut_control "ec_uart_stream" &>/dev/null
		dut_control "cpu_uart_stream" &>/dev/null

		# Disable UART capture
		dut_control "cr50_uart_capture:off" &>/dev/null
		dut_control "ec_uart_capture:off" &>/dev/null
		dut_control "cpu_uart_capture:off" &>/dev/null

		# Restore dut_control values
		if [[ ${#DUT_CTRL_SAVE[@]} -gt 0 ]] ; then
			dut_control "${DUT_CTRL_SAVE[@]}"
		fi
	fi

	# Delete all temp files if requested to do so
	if [[ "${FLAGS_clean}" == "${FLAGS_TRUE}" ]] ; then
		rm -rf "${DIR_TMP}"
	else
		local LINK_LATEST="/tmp/$(basename ${SCRIPT})_latest"
		unlink "${LINK_LATEST}" &>/dev/null || :
		ln -s "${DIR_TMP}" "${LINK_LATEST}"
		info "Test files are in ${LINK_LATEST}"
	fi

	[[ -n "${SAVEIFS}" ]] && IFS=${SAVEIFS}
}
trap cleanup EXIT

#######################################
# Issue a console command to UART.
# Arguments:
#   $1: Target UART. should be either "CR50", "EC", or "CPU"
#   $2: (optional) Console command to send
#######################################
generate_traffic() {
	[[ $# -le 2 ]] || die "${FUNCNAME[0]}: argument error: $@"

	local UART="${1^^}"
	local PTY="PTY_${UART}"
	declare -A CMD_FOR_SAMPLE=( ["CR50"]="help" ["CPU"]="" ["EC"]="help" )

	if [[ $# -gt 1 ]] ; then
		echo "$2" > "${!PTY}"
	else
		echo "${CMD_FOR_SAMPLE["${UART}"]}" > "${!PTY}"
	fi
}

#######################################
# Calculate the number of characters
# Arguments:
#   $1: Input text file
# Returns:
#   The number of characters from the input file
#######################################
get_num_char() {
	echo $(wc -c < "$1")
}

#######################################
# Calculate the percentage
# Arguments:
#   $1: Numerator
#   $2: Denominator
# Returns:
#   The percentage $1 over $2
#######################################
calc_percent() {
	echo $( bc <<< "scale=1;100.0 * $1 / $2" )
}

# Number of expected characters without loss.
declare -A CHAR_EXPC=( ["CR50"]=0 ["CPU"]=0 ["EC"]=0 )
# Number of characters lost.
declare -A CHAR_LOST=( ["CR50"]=0 ["CPU"]=0 ["EC"]=0 )

#######################################
# Calculate the character loss rate based on the given test files.
# Arguments:
#   $1: Target UART. Should be either "CR50", "EC", or "CPU"
#   $2: Base sample text file to compare with
#   $3: Test text file to compare against
#   $4: Number that a console command repeated get a test file
#######################################
calc_char_loss_rate() {
	[[ $# -eq 4 ]] || die "${FUNCNAME[0]}: argument error: $@"

	local UART="$1"
	local FILE_SAMPLE="$2"
	local FILE_RES="$3"
	local REPEATS="$4"
	local FILE_BASE="${FILE_SAMPLE}"."${REPEATS}"

	# Create a base text with the sample output, if not exists.
	if [[ ! -e ${FILE_BASE} ]] ; then
		for (( i=1; i<=${REPEATS}; i++ )) do
			cat ${FILE_SAMPLE}
		done > ${FILE_BASE}
	fi

	# Count the characters in captured data files, and get the difference
	# between them.
	local CH_EXPC=$( get_num_char "${FILE_BASE}" )
	local CH_RESL=$( get_num_char "${FILE_RES}" )
	local CH_LOST=$(( ${CH_EXPC} - ${CH_RESL} ))

	# Check if test output is bigger than expected.
	# If so, it must contain some other pattern.
	if [[ ${CH_LOST} -lt 0 ]] ; then
		die "${FUNCNAME[0]}: Test output data seem corrupted:" \
			"${FILE_RES} against ${FILE_BASE}"
	fi

	# Calculate the character loss rate
	local LOSS_RATE=$( calc_percent ${CH_LOST} ${CH_EXPC} )
	local STR="\t${UART}: char loss rate = ${LOSS_RATE} %"
	STR+=" : ${CH_LOST} lost / ${CH_EXPC}"

	# If the sizes are same each other, then compare the text.
	if [[ ${CH_LOST} -eq 0 ]] ; then
		if ! diff --brief ${FILE_BASE} ${FILE_RES} ; then
			die "${FUNCNAME[0]}: ${FILE_RES} does not match to" \
				"${FILE_BASE}"
		fi

		info "\t${UART}: 0 lost / ${CH_EXPC}"
	else
		error ${STR}
	fi

	# Accumulate the data for average rate calcuation at the end.
	CHAR_EXPC["${UART}"]=$(( ${CHAR_EXPC["${UART}"]} + ${CH_EXPC} ))
	CHAR_LOST["${UART}"]=$(( ${CHAR_LOST["${UART}"]} + ${CH_LOST} ))
}

#######################################
# Start to capture UART output. Call this function in background.
# Arguments:
#   $1: UART device path.
#   $2: File path to save the capture
# Returns:
#   Process ID capturing the UART output in background
#######################################
start_capture() {
	[[ $# -eq 2 ]] || die "${FUNCNAME[0]}: argument error: $@"

	# Change IFS to null
	IFS=''
	local STTY_ARGS=( "cs8" "ignbrk" "noflsh"
		    "-brkint" "-clocal"
		    "-echo" "-echoe" "-echok" "-echoctl" "-echoke"
		    "-icanon" "-icrnl" "-iexten" "-imaxbel"
		    "-onlcr" "-opost" )
	stty -F "$1" "${STTY_ARGS[@]}" || die "stty failure: ${STTY_ARGS[@]}"

	# Drain output
	cat "$1" &> /dev/null &
	local PROC_ID=$!
	sleep 1
	kill ${PROC_ID} &>/dev/null

	# Start to capture
	cat "$1" &> "$2" 2>/dev/null &
	echo $!
}

#######################################
# Get a base output for a single console command execution via servod
# Globals:
#   SAMPLE_CR50, SAMPLE_CPU, SAMPLE_EC
#   PTY_CR50, PTY_CPU, PTY_EC
# Arguments:
#   $1: Target UART. Should be either "CR50", "CPU", and "EC"
#   $2: Filename to save the UART output
#######################################
get_sample_txt_no_servod() {
	local UART="${1^^}"
	local PTY="PTY_${UART}"

	# Start to capture
	local PROC_ID=$( start_capture "${!PTY}" "$2" )

	generate_traffic "${UART}"

	# Stop capturing
	sleep 1
	kill ${PROC_ID} &>/dev/null
}

#######################################
# Enable servod to capture UART output
# Arguments:
#   $1: Target UART. Should be either "CR50", "EC", or "CPU"
#######################################
enable_capture() {
	[[ $# -eq 1 ]] || die "${FUNCNAME[0]}: argument error: $@"

	local UART="${1,,}"

	dut_control "${UART}_uart_capture:on"

	# Drain captured data multiple times for complete cleanup
	dut_control "${UART}_uart_stream" &>/dev/null
}

#######################################
# Disable servod to capture UART output
# Arguments:
#   $1: Target UART. Should be either "CR50", "EC", or "CPU"
#######################################
disable_capture(){
	[[ $# -eq 1 ]] || die "${FUNCNAME[0]}: argument error: $@"

	local UART="${1,,}"

	dut_control "${UART}_uart_capture:off"
}

#######################################
# Retrieve all captured data from servod
# Arguments:
#   $1: Target UART. Should be either "CR50", "EC", or "CPU"
#   $2: File (path) to store the captured data
#######################################
get_capture() {
	[[ $# -eq 2 ]] || die "${FUNCNAME[0]}: argument error: $@"

	local FILE_TMP="$(mktemp)"
	local UART="${1,,}"

	# dut_control might print not all output data at once
	# Should be done repeadely until nothing comes out.
	while :; do
		dut_control "${UART}_uart_stream" > "${FILE_TMP}"
		trim_capture "${FILE_TMP}"

		local CHAR_NUM=$( get_num_char "${FILE_TMP}" )

		[[ ${CHAR_NUM} -eq 0 ]] && break

		cat ${FILE_TMP}
	done >> "$2"

	rm -f ${FILE_TMP}
}

#######################################
# Refine raw (servod) captured text
# Arguments:
#   $1: File (path) that contains the raw captured data
#######################################
trim_capture(){
	local INPUT_FILE="$1"

	# Truncate the footer character by dut_control, like ' or ",
	# and a trailing new line.
	truncate -s -2 "${INPUT_FILE}"

	# stripping the header message off from dut_control,
	# like "cr50_uart_stream:'" or "cpu_uar_stream:"".
	# Also remove all timestamps, which are from ec3po.
	sed -i -r -e 's/^.{2,8}_uart_stream:.//' \
	    -e 's/\\r//g' \
	    -e 's/\\n/\'$'\n''/g' \
	    "${INPUT_FILE}"
}

#######################################
# Get a base output for a single console command execution via servod
# Globals:
#   SAMPLE_CR50
#   SAMPLE_EC
#   SAMPLE_CPU
# Arguments:
#   $1: Target UART. Should be either "CR50", "EC", or "CPU"
#   $2: Filename to save the UART output
#######################################
get_sample_txt_servod() {
	local UART="${1^^}"

	enable_capture "${UART}"

	generate_traffic "${UART}"

	# Stop capturing
	sleep 1
	get_capture "${UART}" "$2"
	disable_capture "${UART}"
}

#######################################
# Run a UART stress test on target UART(s) using read command
# Globals:
#   SAMPLE_CR50, SAMPLE_EC, SAMPLE_CPU
#   UART1, UART2, UART3
#   PTY1, PTY2, PTY3
#   FILE_RES1, FILE_RES2, FILE_RES3
#   ITER
#######################################
stress_test_no_servod() {
	local PID1=0
	local PID2=0
	local PID3=0

	# Start to capture
	[[ -n "${UART1}" ]] && PID1=$( start_capture "${!PTY1}" "${FILE_RES1}" )
	[[ -n "${UART2}" ]] && PID2=$( start_capture "${!PTY2}" "${FILE_RES2}" )
	[[ -n "${UART3}" ]] && PID3=$( start_capture "${!PTY3}" "${FILE_RES3}" )

	# Generate traffic
	for (( i=1; i<=${ITER}; i++ )) do
		[[ -n "${UART1}" ]] && generate_traffic "${UART1}"
		[[ -n "${UART2}" ]] && generate_traffic "${UART2}"
		[[ -n "${UART3}" ]] && generate_traffic "${UART3}"

		[[ $(( $i % 10 )) == 0 ]] && sleep 2
	done

	# Stop capturing
	sleep 2
	if [[ "${PID1}" -ne 0 ]]; then
		kill ${PID1} &>/dev/null
	fi
	if [[ "${PID2}" -ne 0 ]]; then
		kill ${PID2} &>/dev/null
	fi
	if [[ "${PID3}" -ne 0 ]]; then
		kill ${PID3} &>/dev/null
	fi
}

#######################################
# Run a UART stress test on target UART(s) via servod
# Globals:
#   SAMPLE_CR50, SAMPLE_EC, SAMPLE_CPU
#   UART1, UART2, UART3
#   PTY1, PTY2, PTY3
#   FILE_RES1, FILE_RES2, FILE_RES3
#   ITER
#######################################
stress_test_servod() {
	[[ -n "${UART1}" ]] && enable_capture "${UART1}"
	[[ -n "${UART2}" ]] && enable_capture "${UART2}"
	[[ -n "${UART3}" ]] && enable_capture "${UART3}"

	for (( i=1; i<=${ITER}; i++ )) do
		[[ -n "${UART1}" ]] && generate_traffic "${UART1}"
		[[ -n "${UART2}" ]] && generate_traffic "${UART2}"
		[[ -n "${UART3}" ]] && generate_traffic "${UART3}"

		[[ $(( $i % 10 )) == 0 ]] && continue

		[[ -n "${UART1}" ]] && get_capture "${UART1}" "${FILE_RES1}"
		[[ -n "${UART2}" ]] && get_capture "${UART2}" "${FILE_RES2}"
		[[ -n "${UART3}" ]] && get_capture "${UART3}" "${FILE_RES3}"
	done

	# Wait for a while to have all output captured
	sleep 2
	if [[ -n "${UART1}" ]]; then
		get_capture "${UART1}" "${FILE_RES1}"
		disable_capture "${UART1}"
	fi
	if [[ -n "${UART2}" ]]; then
		get_capture "${UART2}" "${FILE_RES2}"
		disable_capture "${UART2}"
	fi
	if [[ -n "${UART3}" ]]; then
		get_capture "${UART3}" "${FILE_RES3}"
		disable_capture "${UART3}"
	fi
}

#######################################
# Run a UART stress test on target UART
# Arguments:
#   $1: Number of times to run a console command
#   $2: The first target UART
#   $3: (optional) The second target UART
#   $4: (optional) The third target UART
#######################################
stress_test() {
	# Check the number of arguments.
	[[ $# -ge 2 && $# -le 4 ]] || \
		die "${FUNCNAME[0]}: wrong number of arguments: $*"

	ITER=$1
	UART1="${2^^}"
	FILE_BASE1="SAMPLE_${UART1}"
	PTY1="PTY_${UART1}"

	if [[ $# -gt 2 ]] ; then
		UART2="${3^^}"
		FILE_BASE2="SAMPLE_${UART2}"
		PTY2="PTY_${UART2}"
	else
		unset UART2
	fi

	if [[ $# -gt 3 ]] ; then
		UART3="${4^^}"
		FILE_BASE3="SAMPLE_${UART3}"
		PTY3="PTY_${UART3}"
	else
		unset UART3
	fi

	FILE_RES1="${DIR_TMP}/res_${UART1}_w_${UART2}_${UART3}.cap"
	FILE_RES2="${DIR_TMP}/res_${UART2}_w_${UART1}_${UART3}.cap"
	FILE_RES3="${DIR_TMP}/res_${UART3}_w_${UART1}_${UART2}.cap"

	# Run test
	if ${USE_SERVOD}; then
		stress_test_servod
	else
		stress_test_no_servod
	fi

	# Calculate the character loss
	if [[ -n "${UART1}" ]]; then
		calc_char_loss_rate "${UART1}" "${!FILE_BASE1}" "${FILE_RES1}" \
					"${ITER}"
	fi
	if [[ -n "${UART2}" ]]; then
		calc_char_loss_rate "${UART2}" "${!FILE_BASE2}" "${FILE_RES2}" \
					"${ITER}"
	fi
	if [[ -n "${UART3}" ]]; then
		calc_char_loss_rate "${UART3}" "${!FILE_BASE3}" "${FILE_RES3}" \
					"${ITER}"
	fi
}

MIN_CHAR_SMPL=99999999
get_sample_txt() {
	local UART="$1"
	local SAMPLE_FILE="SAMPLE_${UART}"

	if ${USE_SERVOD}; then
		get_sample_txt_servod "${UART}" "${!SAMPLE_FILE}"
	else
		get_sample_txt_no_servod "${UART}" "${!SAMPLE_FILE}"
	fi

	local NUM_CH=$( get_num_char "${!SAMPLE_FILE}" )

	if [[ ${NUM_CH} -eq 0 ]]; then
		die "No output on console command on ${UART}"
	fi

	if [[ ${NUM_CH} -lt ${MIN_CHAR_SMPL} ]]; then
		MIN_CHAR_SMPL=${NUM_CH}
	fi
}

# Get sample output as base for comparison
[[ -e "${PTY_CR50}" ]] && get_sample_txt "CR50"
[[ -e "${PTY_CPU}" ]] && get_sample_txt "CPU"
[[ -e "${PTY_EC}" ]] && get_sample_txt "EC"

# Calculate the iteration to run console command for traffic.
REPEATS=$(( (${FLAGS_min_char} + ${MIN_CHAR_SMPL} - 1) / ${MIN_CHAR_SMPL} ))

# Start the stress test
info "Stress test on single UART"
[[ -e "${PTY_CR50}" ]] && stress_test ${REPEATS} "CR50"
[[ -e "${PTY_CPU}" ]] && stress_test ${REPEATS} "CPU"
[[ -e "${PTY_EC}" ]] && stress_test ${REPEATS} "EC"

if [[ ${UART_COUNT} -ge 2 ]]; then
	info "Stress test on two UARTs"
	[[ -e "${PTY_CR50}" && -e "${PTY_EC}" ]]  && \
			stress_test ${REPEATS} "CR50" "EC"
	[[ -e "${PTY_CR50}" && -e "${PTY_CPU}" ]] && \
			stress_test ${REPEATS} "CR50" "CPU"
	[[ -e "${PTY_CPU}" && -e "${PTY_EC}" ]]   && \
			stress_test ${REPEATS} "CPU" "EC"
fi

if [[ ${UART_COUNT} -ge 3 ]]; then
	info "Stress test on three UARTs"
	stress_test ${REPEATS} "CR50" "CPU" "EC"
fi

# Calculate average rate calculation
info "All tests done"

#######################################
# Print the average character loss rate
# Arguments:
#   $1: Target UART. Should be either "CR50", "EC", or "CPU"
#######################################
print_char_loss_rate() {
	local UART="${1^^}"
	local CH_LOST=${CHAR_LOST["${UART}"]}
	local CH_EXPC=${CHAR_EXPC["${UART}"]}
	local RATE=$( calc_percent ${CH_LOST} ${CH_EXPC} )

	if [[ ${CHAR_LOST["${UART}"]} -eq 0 ]] ; then
		info "\tPASS:${UART}: 0 lost / ${CH_EXPC}"
	else
		local ERR_MSG="\t${UART}: aver. char loss rate,"
		ERR_MSG+="${UART} = ${RATE} % : ${CH_LOST} lost / ${CH_EXPC}"
		error ${ERR_MSG}
	fi
}

[[ -e "${PTY_CR50}" ]] && print_char_loss_rate "CR50"
[[ -e "${PTY_CPU}" ]] && print_char_loss_rate "CPU"
[[ -e "${PTY_EC}" ]] && print_char_loss_rate "EC"
