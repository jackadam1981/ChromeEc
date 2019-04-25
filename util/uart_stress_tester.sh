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
#      The current version of this script runs with servod.
#      Run servod ahead.
#
# Output: At the end of test, there will be character loss results on screen.
#         "INFO: aver. char loss rate, CR50 = 0 % : ...
#          WARNING: aver. char loss rate, EC   = xx.x % : ...
#          WARNING: aver. char loss rate, CPU  = xx.x % : ..."
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

# Loads script libraries.
. "/usr/share/misc/shflags" || exit 1

#Flags
DEFINE_boolean cr50 "${FLAGS_FALSE}"  "Test cr50 console"
DEFINE_boolean ec   "${FLAGS_FALSE}"  "Test ec console"
DEFINE_boolean cpu  "${FLAGS_FALSE}" "Test cpu (AP) console"
DEFINE_boolean verbose  "${FLAGS_FALSE}" "Print more messages"
DEFINE_boolean clean  "${FLAGS_FALSE}" "Delete all log data after test"
DEFINE_string cr50_pty "" "CR50 UART device, e.g. /dev/ttyUSB0"
DEFINE_string ec_pty "" "EC UART device, e.g. /dev/ttyUSB1"
DEFINE_string cpu_pty "" "CPU UART device, e.g. /dev/ttyUSB2"

FLAGS_HELP="usage: $0 [flags]"
FLAGS "$@" || exit 1
eval set -- "${FLAGS_ARGV}"
if [[ $# -gt 0 ]] ; then
	die "line ${LINENO}: invalid arguments: \"$*\""
fi

set -e

# Redirects tput to stderr, and drop any error messages.
tput2() {
	tput "$@" 1>&2 2>/dev/null || true
}

error() {
	tput2 bold && tput2 setaf 1
	echo "ERROR: $*" >&2
	tput2 sgr0
}

info() {
	tput2 bold && tput2 setaf 2
	echo "INFO: $*" >&2
	tput2 sgr0
}

warn() {
	tput2 bold && tput2 setaf 3
	echo "WARNING: $*" >&2
	tput2 sgr0
}

debug() {
	tput2 bold && tput2 setaf 5
	echo "DEBUG: $*" >&2
	tput2 sgr0
}

die() {
	[ -z "$*" ] || error "$@"
	exit 1
}

TOOL_PATH="$PATH:${SCRIPT_DIR}"
LSUSB_BIN="$( PATH="${TOOL_PATH}" which lsusb )"
UDEVADM_BIN="$( PATH="${TOOL_PATH}" which udevadm )"
CR50_VID="18d1"
CR50_PID="5014"

info "ChromeOS UART stress test starts."
info

if dut-control -i &>/dev/null ; then
	# servod is running.
	CAPTURE_TOOL="dutctrl"

	info "$( dut-control servo_type )"
	info "$( dut-control ec_chip )"
	EC_BOARD="$( dut-control ec_board 2>/dev/null )" || \
		die "line ${LINENO}: Failed to get ec_board"
	info "${EC_BOARD}"

	if dut-control -i cr50_version &>/dev/null ; then
		info $( dut-control cr50_version )
	fi
else
	# servod is not running.
	CAPTURE_TOOL="cat"

	info "$( ${LSUSB_BIN} -d ${CR50_VID}:${CR50_PID} -v | \
		grep iConfig 2>/dev/null )"

	# Find a CR50 UART device.
	DEVNUM_BASE=-1
	for (( i=0; ; i++ )) do
		DEVFILE="/dev/ttyUSB${i}"
		if [[ ! -e ${DEVFILE} ]] ; then
			break
		fi

		DEVINFO="$( ${UDEVADM_BIN} info -q property -n ${DEVFILE} )"
		if ! echo ${DEVINFO} | grep -iq "VENDOR_ID=${CR50_VID}" ; then
			continue
		fi
		if ! echo ${DEVINFO} | grep -iq "MODEL_ID=${CR50_PID}" ; then
			continue
		fi

		DEVNUM_BASE=$i
		break;
	done

	if [[ ${DEVNUM_BASE} -lt 0 ]] ; then
		die "line ${LINENO}: CR50 UART device is not detected"
	fi
fi

# Get a terminal path.
declare -A DEVNUM_OFFSET=( ["CR50"]=0 ["EC"]=2 ["CPU"]=1 )
get_uart_pty() {
	local TARGET="$1"
	TARGET="${TARGET,,}"

	local FLAG_NAME_TARGET_PTY="FLAGS_${TARGET}_pty"
	if [[ -n "${!FLAG_NAME_TARGET_PTY}" ]] ; then
		echo "${!FLAG_NAME_TARGET_PTY}"
		return
	fi

	# Device path was not given as argument.
	if [[ "${CAPTURE_TOOL}" == "dutctrl" ]] ; then
		RETVAL=$( dut-control ${TARGET}_uart_pty ) || \
			die "${FUNCNAME[0]}: Failed to get ${TARGET}_uart_pty}"
		RETVAL="${RETVAL#*:}"
	else
		local DEVNUM=$((${DEVNUM_BASE}+${DEVNUM_OFFSET["${TARGET^^}"]}))
		RETVAL="/dev/ttyUSB${DEVNUM}"
	fi

	echo "${RETVAL}"
}
if [[ "${FLAGS_cr50}" -eq "${FLAGS_TRUE}" ]] ; then
	PTY_CR50="$(get_uart_pty "CR50")"
	[[ -e "${PTY_CR50}" ]] || \
		die "line ${LINENO}: ${PTY_CR50} (CR50) does not exist"
	info "CR50 PTY= ${PTY_CR50}"
fi
if [[ "${FLAGS_ec}" -eq "${FLAGS_TRUE}" ]] ; then
	PTY_EC="$(get_uart_pty "EC")"
	[[ -e "${PTY_EC}" ]] || \
		die "line ${LINENO}: ${PTY_EC} (EC) does not exist"
	info "  EC PTY= ${PTY_EC}"
fi
if [[ "${FLAGS_cpu}" -eq "${FLAGS_TRUE}" ]] ; then
	PTY_CPU="$(get_uart_pty "CPU")"
	[[ -e "${PTY_CPU}" ]] || \
		die "line ${LINENO}: ${PTY_CPU} (CPU) does not exist"
	info " CPU PTY= ${PTY_CPU}"
fi
info

# Suppress console output and disable ec3po's timestamp output.
declare -a DUT_CTRL_SAVE
if [[ -e "${PTY_CR50}" ]] ; then
	echo 'chan save' > "${PTY_CR50}"
	echo 'chan 0'    > "${PTY_CR50}"

	if [[ "${CAPTURE_TOOL}" == "dutctrl" ]] ; then
		DUT_CTRL_SAVE+=( "$(dut-control cr50_uart_timestamp)" )
		dut-control cr50_uart_timestamp:off
	fi
fi
if [[ -e "${PTY_EC}" ]] ; then
	echo 'chan save' > "${PTY_EC}"
	echo 'chan 0'    > "${PTY_EC}"

	if [[ "${CAPTURE_TOOL}" == "dutctrl" ]] ; then
		DUT_CTRL_SAVE+=( "$(dut-control ec_uart_timestamp)" )
		dut-control ec_uart_timestamp:off
	fi
fi
if [[ -e "${PTY_CPU}" ]] ; then
	if [[ "${CAPTURE_TOOL}" == "dutctrl" ]] ; then
		DUT_CTRL_SAVE+=( "$(dut-control cpu_uart_timestamp)" )
		dut-control cpu_uart_timestamp:off
	fi
fi

# Trap function on EXIT
cleanup() {
	# Restore channel setting
	if [[ -e "${PTY_CR50}" ]] ; then
		echo 'chan restore' > "${PTY_CR50}"
	fi
	if [[ -e "${PTY_EC}" ]] ; then
		echo 'chan restore' > "${PTY_EC}"
	fi

	if [[ "${CAPTURE_TOOL}" == "dutctrl" ]] ; then
		# Disable UART capture
		dut-control cr50_uart_stream &>/dev/null
		dut-control ec_uart_stream &>/dev/null
		dut-control cpu_uart_stream &>/dev/null

		dut-control cr50_uart_capture:off &>/dev/null
		dut-control ec_uart_capture:off &>/dev/null
		dut-control cpu_uart_capture:off &>/dev/null

		# Restore dut-control values
		if [[ ${#DUT_CTRL_SAVE[@]} -gt 0 ]] ; then
			dut-control "${DUT_CTRL_SAVE[@]}"
		fi
	fi

	# Delete all temp files if requested to do so
	if [[ "${FLAGS_clean}" == "${FLAGS_TRUE}" ]] ; then
		rm -rf "${DIR_TMP}"
	else
		local LINK_LATEST="/tmp/$(basename ${SCRIPT})_latest"
		unlink "${LINK_LATEST}" &>/dev/null || :
		ln -s "${DIR_TMP}" "${LINK_LATEST}"
		debug "Test files are in ${LINK_LATEST}"
	fi
}
trap cleanup EXIT

# Get sample of text to compare for.
DIR_TMP="$(mktemp -d --suffix=.$(basename ${SCRIPT}))"
SAMPLE_CR50="${DIR_TMP}/sample_cr50.cap"
SAMPLE_EC="${DIR_TMP}/sample_ec.cap"
SAMPLE_CPU="${DIR_TMP}/sample_cpu.cap"

declare -A CMD_FOR_SAMPLE=( ["CR50"]="help" ["EC"]="help" ["CPU"]="" )

#######################################
# Issue a console command to UART.
# Arguments:
#   $1: Target UART. should be either "CR50", "EC", or "CPU"
#   $2: (optional) Console command to send
#######################################
generate_uart_traffic() {
	if [[ $# -lt 1 ]] ; then
		error "${FUNCNAME[0]}: no argument passed"
		return 1
	fi

	local TARGET="$1"
	local PTY="PTY_${TARGET^^}"

	if [[ $# -gt 1 ]] ; then
		echo "$2" > "${!PTY}"
	else
		echo "${CMD_FOR_SAMPLE[${TARGET^^}]}" > "${!PTY}"
	fi
}

#######################################
# Calculate the number of characters
# Arguments:
#   $1: Input text file
# Returns
#   The number of characters from the input file
#######################################
get_num_char() {
	echo $(wc -c < "$1")
}

#######################################
# Calculate the number of characters
# Arguments:
#   $1: Numerator
#   $2: Denominator
# Returns
#   Return the percentage $1 over $2
#######################################
calc_percent() {
	echo $( bc <<< "scale=1;100.0 * $1 / $2" )
}


# Number of expected characters without loss.
declare -A CHAR_EXPC=( ["CR50"]=0 ["EC"]=0 ["CPU"]=0 )
# Number of characters lost.
declare -A CHAR_LOST=( ["CR50"]=0 ["EC"]=0 ["CPU"]=0 )

#######################################
# Calculate the character loss rate based on the given test files.
# Arguments:
#   $1: Target UART. Should be either "CR50", "EC", or "CPU"
#   $2: Base sample text file to compare with
#   $3: Test text file to compare against
#   $4: Number that a console command repeated get a test file
#######################################
calc_char_loss_rate() {
	if [[ $# -lt 4 ]] ; then
		error "${FUNCNAME[0]}: wrong number of arguments: $*"
		return 1
	fi

	local TARGET="$1"
	local FILE_SAMPLE="$2"
	local FILE_RESL="$3"
	local REPEATS="$4"

	local FILE_BASE="${FILE_SAMPLE}"."${REPEATS}"

	# Create a base text with the sample output, if not exists.
	if [[ ! -e ${FILE_BASE} ]] ; then
		for (( i=1; i<=${REPEATS}; i++ )) do
			cat ${FILE_SAMPLE}
		done > ${FILE_BASE}
	fi

	# Create a base text with the sample output, if not exists.
	local CH_EXPC=$( get_num_char "${FILE_BASE}" )
	local CH_RESL=$( get_num_char "${FILE_RESL}" )
	local CH_LOST=$((  ${CH_EXPC} - ${CH_RESL} ))

	# Check if test output is bigger than expected.
	# If so, it must contain some other pattern.
	if [[ ${CH_LOST} -lt 0 ]] ; then
		error "${FUNCNAME[0]}: Test output data seem corrupted:" \
			"${FILE_RESL} against ${FILE_BASE}"
		return 1
	fi

	# Calculate the character loss rate
	local LOSS_RATE=$( calc_percent ${CH_LOST} ${CH_EXPC} )

	# If the sizes are same each other, then compare the text.
	if [[ ${CH_LOST} -eq 0 ]] ; then
		if ! diff --brief ${FILE_BASE} ${FILE_RESL} ; then
			error "${FUNCNAME[0]}: ${FILE_RESL} does not match to" \
				"${FILE_BASE}"
			return 1
		fi
		info "char loss rate ${TARGET} = ${LOSS_RATE} % :" \
			"${CH_LOST} lost among ${CH_EXPC}"
	else
		warn "char loss rate ${TARGET} = ${LOSS_RATE} % :" \
			"${CH_LOST} lost among ${CH_EXPC}"
	fi

	# Accumulate the data for average rate calcuation at the end.
	CHAR_EXPC["${TARGET}"]=$(( ${CHAR_EXPC["${TARGET}"]} + ${CH_EXPC} ))
	CHAR_LOST["${TARGET}"]=$(( CHAR_LOST["${TARGET}"] + ${CH_LOST} ))
}

get_sample_txt_cat() {
	die "${FUNCNAME[0]}: Not implemented yet. Run servod and retry"
}

#######################################
# Enable servod to capture UART output
# Arguments:
#   $1: Target UART. Should be either "CR50", "EC", or "CPU"
#######################################
enable_uart_capture() {
	if [[ $# -lt 1 ]] ; then
		error "${FUNCNAME[0]}: no argument passed."
	fi

	local TARGET="$1"
	TARGET="${TARGET,,}"

	dut-control "${TARGET}_uart_capture:on"

	# Drain all output first to clean up the buffer
	generate_uart_traffic "${TARGET}"
	sleep 2
	# Retrieve captured data multiple times for complete flush
	dut-control "${TARGET}_uart_stream" > /dev/null
	dut-control "${TARGET}_uart_stream" > /dev/null
	dut-control "${TARGET}_uart_stream" > /dev/null
}

#######################################
# Disable servod to capture UART output
# Arguments:
#   $1: Target UART. Should be either "CR50", "EC", or "CPU"
#######################################
disable_uart_capture(){
	if [[ $# -lt 1 ]] ; then
		error "${FUNCNAME[0]}: no argument passed."
	fi

	local TARGET="$1"
	TARGET="${TARGET,,}"

	dut-control "${TARGET}_uart_capture:off"
}

#######################################
# Retrieve all captured data from servod
# Arguments:
#   $1: Target UART. Should be either "CR50", "EC", or "CPU"
#   $2: File (path) to store the captured data
#######################################
get_uart_capture() {
	if [[ $# -lt 2 ]] ; then
		error "${FUNCNAME[0]}: not enough arguments passed: $*"
		return 1
	fi

	local FILE_TMP="$(mktemp)"
	local TARGET="$1"
	TARGET="${TARGET,,}"


	# dut-control might print not all output data at once
	# Should be done repeadely until nothing comes out.
	while :; do
		dut-control "${TARGET}_uart_stream" > "${FILE_TMP}"
		trim_uart_capture "${FILE_TMP}"

		local CHAR_NUM=$( get_num_char "${FILE_TMP}" )

		if [[ ${CHAR_NUM} -eq 0 ]] ; then
			break
		fi

		cat ${FILE_TMP}
	done >> "$2"

	rm -f ${FILE_TMP}
}

#######################################
# Refine raw (servod) captured text
# Arguments:
#   $1: File (path) that contains the raw captured data
#######################################
trim_uart_capture(){
	local INPUT_FILE="$1"

	# Truncate the footer character by dut-control, like ' or ",
	# and a trailing new line.
	truncate -s -2 "${INPUT_FILE}"

	# stripping the header message off from dut-control,
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
#######################################
get_sample_txt_dutctrl() {
	local TARGET="$1"
	local FILE_NAME="SAMPLE_${TARGET^^}"

	enable_uart_capture "${TARGET}"
	generate_uart_traffic "${TARGET}"

	# Wait for a while to have all output captured
	sleep 1

	get_uart_capture "${TARGET}" "${!FILE_NAME}"
	if [[ "${FLAGS_verbose}" -eq "${FLAGS_TRUE}" ]] ; then
		info "sample text from ${TARGET}"
		cat ${!FILE_NAME}
		echo
	fi

	disable_uart_capture "${TARGET}"
}

#######################################
# Run a UART stress test on target UART(s)
# Globals:
#   SAMPLE_CR50
#   SAMPLE_EC
#   SAMPLE_CPU
# Arguments:
#   $1: Number of times to run a console command
#   $2: The first target UART
#   $3: (optional) The second target UART
#   $4: (optional) The third target UART
#######################################
stress_test_cat() {
	die "${FUNCNAME[0]}: Not implemented yet"
}

#######################################
# Run a UART stress test on target UART(s) via servod
# Globals:
#   SAMPLE_CR50
#   SAMPLE_EC
#   SAMPLE_CPU
# Arguments:
#   $1: Number of times to run a console command
#   $2: The first target UART
#   $3: (optional) The second target UART
#   $4: (optional) The third target UART
#######################################
stress_test_dutctrl() {
	local ITER

	local TARGET1
	local TARGET2
	local TARGET3

	local FILE_BASE1
	local FILE_BASE2
	local FILE_BASE3

	local FILE_RESL1="/dev/null"
	local FILE_RESL2="/dev/null"
	local FILE_RESL3="/dev/null"

	# Check the number of arguments.
	if [[ $# -le 1 || $# -gt 4 ]] ; then
		error "${FUNCNAME[0]}: wrong number of arguments: $*"
		return 1
	else
		ITER="$1"

		TARGET1="$2"
		FILE_BASE1="SAMPLE_${TARGET1^^}"
	fi

	if [[ $# -gt 2 ]] ; then
		TARGET2="$3"
		FILE_BASE2="SAMPLE_${TARGET2^^}"
	fi

	if [[ $# -gt 3 ]] ; then
		TARGET3="$4"
		FILE_BASE3="SAMPLE_${TARGET3^^}"
	fi

	FILE_RESL1="${DIR_TMP}/result_${TARGET1}_with_${TARGET2}_${TARGET3}.cap"
	FILE_RESL2="${DIR_TMP}/result_${TARGET2}_with_${TARGET1}_${TARGET3}.cap"
	FILE_RESL3="${DIR_TMP}/result_${TARGET3}_with_${TARGET1}_${TARGET2}.cap"

	info "Stress test on $(($# - 1)) UART(s), ${*:2} starts"

	[[ -n "${TARGET1}" ]] && enable_uart_capture "${TARGET1}"
	[[ -n "${TARGET2}" ]] && enable_uart_capture "${TARGET2}"
	[[ -n "${TARGET3}" ]] && enable_uart_capture "${TARGET3}"

	for (( i=1; i<=${ITER}; i++ )) do
		[[ -n "${TARGET1}" ]] && generate_uart_traffic "${TARGET1}"
		[[ -n "${TARGET2}" ]] && generate_uart_traffic "${TARGET2}"
		[[ -n "${TARGET3}" ]] && generate_uart_traffic "${TARGET3}"

		if [[ $(( $i % 10 )) == 0 ]] ; then
			[[ -n "${TARGET1}" ]] && get_uart_capture \
					"${TARGET1}" "${FILE_RESL1}"
			[[ -n "${TARGET2}" ]] && get_uart_capture \
					"${TARGET2}" "${FILE_RESL2}"
			[[ -n "${TARGET3}" ]] && get_uart_capture \
					"${TARGET3}" "${FILE_RESL3}"
		fi
	done

	# Wait for a while to have all output captured
	sleep 5
	[[ -n "${TARGET1}" ]] && get_uart_capture "${TARGET1}" "${FILE_RESL1}"
	[[ -n "${TARGET2}" ]] && get_uart_capture "${TARGET2}" "${FILE_RESL2}"
	[[ -n "${TARGET3}" ]] && get_uart_capture "${TARGET3}" "${FILE_RESL3}"

	if [[ -n "${TARGET1}" ]] ; then
		disable_uart_capture "${TARGET1}"
		calc_char_loss_rate "${TARGET1}" \
				"${!FILE_BASE1}" "${FILE_RESL1}" "${ITER}"
	fi

	if [[ -n "${TARGET2}" ]] ; then
		disable_uart_capture "${TARGET2}"
		calc_char_loss_rate "${TARGET2}" \
				"${!FILE_BASE2}" "${FILE_RESL2}" "${ITER}"
	fi

	if [[ -n "${TARGET3}" ]] ; then
		disable_uart_capture "${TARGET3}"
		calc_char_loss_rate "${TARGET3}" \
				"${!FILE_BASE3}" "${FILE_RESL3}" "${ITER}"
	fi

	info "Stress test ends"
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
	case $# in
		2|3|4) stress_test_${CAPTURE_TOOL} "$@"
		   ;;
		*) error "${FUNCNAME[0]}: wrong number of arguments: $*"
		   return 1
		   ;;
	esac
}

# Get sample output as base for comparison
if [[ -e "${PTY_CR50}" ]] ; then
	get_sample_txt_${CAPTURE_TOOL} "CR50"
fi

if [[ -e "${PTY_EC}" ]] ; then
	get_sample_txt_${CAPTURE_TOOL} "EC"
fi

if [[ -e "${PTY_CPU}" ]] ; then
	get_sample_txt_${CAPTURE_TOOL} "CPU"
fi

# Test on CR50 console
ITERATIONS=40
if [[ -e "${PTY_CR50}" ]] ; then
	sleep 2
	stress_test ${ITERATIONS} "CR50"
fi
# Test on EC console
if [[ -e "${PTY_EC}" ]] ; then
	sleep 2
	stress_test ${ITERATIONS} "EC"
fi
# Test on CPU console
if [[ -e "${PTY_CPU}" ]] ; then
	sleep 2
	stress_test ${ITERATIONS} "CPU"
fi
# Test on CR50 + EC console
if [[ -e "${PTY_CR50}" && -e "${PTY_EC}" ]] ; then
	sleep 2
	stress_test ${ITERATIONS} "CR50" "EC"
fi
# Test on EC + CPU console
if [[ -e "${PTY_EC}" && -e "${PTY_CPU}" ]] ; then
	sleep 2
	stress_test ${ITERATIONS} "EC" "CPU"
fi
# Test on CR50 + CPU console
if [[ -e "${PTY_CR50}" && -e "${PTY_CPU}" ]] ; then
	sleep 2
	stress_test ${ITERATIONS} "CR50" "CPU"
fi
# Test on CR50 + EC + CPU console
if [[ -e "${PTY_CR50}" && -e "${PTY_EC}" && -e "${PTY_CPU}" ]] ; then
	sleep 2
	stress_test ${ITERATIONS} "CR50" "EC" "CPU"
fi

# Calculate average rate calcuation
info "All tests done"
info
print_char_loss_rate() {
	local TARGET="$1"
	local RATE=$( calc_percent ${CHAR_LOST["${TARGET}"]} \
		${CHAR_EXPC["${TARGET}"]} )
	local STR="aver. char loss rate, ${TARGET} = ${RATE} %"
	STR+=" : ${CHAR_LOST["${TARGET}"]} lost among ${CHAR_EXPC["${TARGET}"]}"

	if [[ ${CHAR_LOST["${TARGET}"]} -eq 0 ]] ; then
		info "${STR}"
	else
		warn "${STR}"
	fi
}
[[ -e "${PTY_CR50}" ]] && print_char_loss_rate "CR50"
[[ -e "${PTY_EC}" ]] && print_char_loss_rate "EC"
[[ -e "${PTY_CPU}" ]] && print_char_loss_rate "CPU"
