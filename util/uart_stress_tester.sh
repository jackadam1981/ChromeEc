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
# Use: Run uart_stress_test.h --help.
#
# Output: At the end of test, character loss rates are displayed on screen.
#
# How it works:
#          1. Run a console command on each UART, and capture the output for
#             a base text in comparison.
#          2. Run a console command on a UART or more multiple times,
#             and capture the output.
#          3. Compare the captured output against the base text, and check
#             if the base pattern is repeated or if the character is lost.
#          4. Print the result
#
# Prerequisite:
#     Turn off all uart output channels with the console command 'chan 0'
#     If servod is running, turn uart_timestamp off before running this test.
#     e.g. dut-control cr50_uart_timestamp:off
#

SCRIPT="$(readlink -f "$0")"

# Load chromeOS common bash library
. "/mnt/host/source/src/scripts/common.sh" || exit 1

# Loads script libraries.
. "/usr/share/misc/shflags" || exit 1

SAVEIFS=${IFS}

#Flags
DEFINE_string pty1 "" "UART device path to test"
DEFINE_string pty2 "" "UART device path to test"
DEFINE_string pty3 "" "UART device path to test"
DEFINE_integer min_char  "40000" "Minimum number of characters to generate."

FLAGS_HELP="usage: $0 [flags]"
FLAGS "$@" || exit 1
eval set -- "${FLAGS_ARGV}"
if [[ $# -gt 0 ]]; then
	die "invalid arguments: \"$*\""
fi

set -e

info "ChromeOS UART stress test starts."

IFS="
"
UART_COUNT=0
if [[ -n "${FLAGS_pty1}" ]]; then
	[[ -e "${FLAGS_pty1}" ]] || die "Device '${FLAGS_pty1}' does not exist."
	PTY1=${FLAGS_pty1}
	info "\tpty1\t= ${PTY1}"
	UART_COUNT=$(( UART_COUNT+1 ))
fi
if [[ -n "${FLAGS_pty2}" ]]; then
	[[ -e "${FLAGS_pty3}" ]] || die "Device '${FLAGS_pty3}' does not exist."
	PTY2=${FLAGS_pty2}
	info "\tpty1\t= ${PTY2}"
	UART_COUNT=$(( UART_COUNT+1 ))
fi
if [[ -n "${FLAGS_pty3}" ]]; then
	[[ -e "${FLAGS_pty3}" ]] || die "Device '${FLAGS_pty3}' does not exist."
	PTY3=${FLAGS_pty3}
	info "\tpty1\t= ${PTY3}"
	UART_COUNT=$(( UART_COUNT+1 ))
fi
[[ ${UART_COUNT} -gt 0 ]] || \
	die "Rerun the script at least one among --pty1, --pty2, or --pty3."

DIR_TMP="$(mktemp -d --suffix=.$(basename ${SCRIPT}))"

declare -A SAMPLE_TXT
[[ -n "${FLAGS_pty1}" ]] && SAMPLE_TXT["${PTY1}"]="${DIR_TMP}/sample1.cap"
[[ -n "${FLAGS_pty2}" ]] && SAMPLE_TXT["${PTY2}"]="${DIR_TMP}/sample2.cap"
[[ -n "${FLAGS_pty3}" ]] && SAMPLE_TXT["${PTY3}"]="${DIR_TMP}/sample3.cap"
RESULT_FILE="${DIR_TMP}/result.txt"

# Trap function on EXIT
cleanup() {
	# Delete all temp files if requested to do so
	local LINK_LATEST="/tmp/$(basename ${SCRIPT})_latest"
	unlink "${LINK_LATEST}" &>/dev/null || :
	ln -s "${DIR_TMP}" "${LINK_LATEST}"
	info "Test files are in ${LINK_LATEST}"

	[[ -n "${SAVEIFS}" ]] && IFS=${SAVEIFS}
}
trap cleanup EXIT

declare -A CMD_FOR_SAMPLE
#######################################
# Issue a console command to device.
# Arguments:
#   $1: Device path
#   $2: (optional) UART console command
#######################################
generate_traffic() {
	case $# in
		1) echo "${CMD_FOR_SAMPLE["$1"]}" > "$1"
		   ;;
		2) echo "$2" > "$1"
		   ;;
		*) die "${FUNCNAME[0]}: argument error: $*"
		   ;;
	esac
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

declare -A CHAR_EXPC        # Number of expected characters without loss.
declare -A CHAR_LOST        # Number of characters lost.

#######################################
# Calculate the character loss rate based on the given test files.
# Arguments:
#   $1: Device Path
#   $2: Base sample text file to compare with
#   $3: Test text file to compare against
#   $4: Number that a console command repeated get a test file
#######################################
calc_char_loss_rate() {
	[[ $# -eq 4 ]] || die "${FUNCNAME[0]}: argument error: $*"

	local PTY="$1"
	local FILE_SAMPLE="$2"
	local FILE_RES="$3"
	local REPEATS="$4"
	local FILE_BASE="${FILE_SAMPLE%.*}.${REPEATS}.cap"

	# Create a base text with the sample output, if not exists.
	if [[ ! -e ${FILE_BASE} ]]; then
		for (( i=1; i<=${REPEATS}; i++ )) do
			cat ${FILE_SAMPLE}
		done > ${FILE_BASE}
	fi

	# Count the characters in captured data files, and get the difference
	# between them.
	local CH_EXPC=$( get_num_char "${FILE_BASE}" )
	local CH_RESL=$( get_num_char "${FILE_RES}" )
	local CH_LOST=$(( ${CH_EXPC} - ${CH_RESL} ))

	# Check if test output is not smaller than expected.
	# If so, it must contain some other pattern.
	if [[ ${CH_LOST} -lt 0 ]]; then
		die "${FUNCNAME[0]}: ${FILE_RES} seems corrupted"
	fi

	# Calculate the character loss rate
	local STR="\t${PTY}: ${CH_LOST} lost / ${CH_EXPC}"
	if [[ ${CH_LOST} -eq 0 ]]; then
		# If the sizes are same each other, then compare the text.
		if ! diff --brief ${FILE_BASE} ${FILE_RES} ; then
			die "${FUNCNAME[0]}: ${FILE_RES} does not match to" \
				"${FILE_BASE}"
		fi

		info "${STR}"
	else
		local LOSS_RATE=$( calc_percent ${CH_LOST} ${CH_EXPC} )
		error "${STR} : ${LOSS_RATE} %"
	fi

	# Accumulate the data for average rate calcuation at the end.
	CHAR_EXPC["${PTY}"]=$(( ${CHAR_EXPC["${PTY}"]} + ${CH_EXPC} ))
	CHAR_LOST["${PTY}"]=$(( ${CHAR_LOST["${PTY}"]} + ${CH_LOST} ))
}

#######################################
# Start to capture UART output. Call this function in background.
# Arguments:
#   $1: Device path.
#   $2: File path to save the capture
# Returns:
#   Process ID capturing the UART output in background
#######################################
start_capture() {
	[[ $# -eq 2 ]] || die "${FUNCNAME[0]}: argument error: $*"

	# Change IFS to null
	IFS=''
	local STTY_ARGS=( "cs8" "ignbrk" "noflsh" "-brkint" "-clocal"
		    "-echo" "-echoe" "-echok" "-echoctl" "-echoke"
		    "-icanon" "-icrnl" "-iexten" "-imaxbel" "-isig" "-ixon"
		    "-onlcr" "-opost" )
	stty -F "$1" "${STTY_ARGS[@]}" || die "stty failure: ${STTY_ARGS[*]}"

	# Drain output
	cat "$1" &> /dev/null &
	local PID=$!
	sleep 2
	kill ${PID} &>/dev/null

	# Start to capture
	exec 'cat "$1" > "$2" 2>/dev/null' &
	echo $!
}

#######################################
# Run a UART stress test on target device(s)
# Arguments:
#   $1: Number of times to run a console command
#   $2: The first target device
#   $3: (optional) The second target device
#   $4: (optional) The third target device
#######################################
stress_test() {
	# Check the number of arguments.
	if [ $# -ge 2 && $# -le 4 ]]; then
		die "${FUNCNAME[0]}: wrong number of arguments: $*"
	fi

	local ITER=$1

	if [[ $# -ge 2 ]]; then
		local P1="$2"
		local DNAME1=${P1%%*/}
	fi
	if [[ $# -ge 3 ]]; then
		local P2="$3"
		local DNAME2=${P2%%*/}
	fi
	if [[ $# -ge 4 ]]; then
		local P3="$4"
		local DNAME3=${P3%%*/}
	fi

	FILE_RES1="${DIR_TMP}/res_${DNAME1}_w_${DNAME2}_${DNAME3}.cap"
	FILE_RES2="${DIR_TMP}/res_${DNAME2}_w_${DNAME1}_${DNAME3}.cap"
	FILE_RES3="${DIR_TMP}/res_${DNAME3}_w_${DNAME1}_${DNAME2}.cap"

	local PID1
	local PID2
	local PID3
	# Start to capture
	[[ -n "${P1}" ]] && PID1=$( start_capture "${P1}" "${FILE_RES1}" )
	[[ -n "${P2}" ]] && PID2=$( start_capture "${P2}" "${FILE_RES2}" )
	[[ -n "${P3}" ]] && PID3=$( start_capture "${P3}" "${FILE_RES3}" )

	# Generate traffic
	for (( i=1; i<=${ITER}; i++ )) do
		[[ -n "${P1}" ]] && generate_traffic "${P1}"
		[[ -n "${P2}" ]] && generate_traffic "${P2}"
		[[ -n "${P3}" ]] && generate_traffic "${P3}"

		[[ $(( $i % 10 )) == 0 ]] && sleep 2
	done

	# Stop capturing
	sleep 5
	[[ -n "${P1}" ]] && kill ${PID1} &>/dev/null
	[[ -n "${P2}" ]] && kill ${PID2} &>/dev/null
	[[ -n "${P3}" ]] && kill ${PID3} &>/dev/null

	# Calculate the character loss
	if [[ -n "${P1}" ]]; then
		calc_char_loss_rate "$2" "${SAMPLE_TXT["${P1}"]}" \
					"${FILE_RES1}" "${ITER}"
	fi
	if [[ -n "${P2}" ]]; then
		calc_char_loss_rate "$3" "${SAMPLE_TXT["${P2}"]}" \
					"${FILE_RES2}" "${ITER}"
	fi
	if [[ -n "${P3}" ]]; then
		calc_char_loss_rate "$4" "${SAMPLE_TXT["${P3}"]}" \
					"${FILE_RES3}" "${ITER}"
	fi
}

MIN_CHAR_SMPL=99999999
get_sample_txt() {
	local PTY="$1"
	local SAMPLE_FILE="$SAMPLE_TXT["${PTY}"]"
	local CMD=""

	# Start to capture
	local PID=$( start_capture "${PTY}" "${SAMPLE_FILE}" )

	generate_traffic "${PTY}" "${CMD}"

	# Stop capturing
	sleep 1
	kill ${PID} &>/dev/null

	# Calculate the number of characters from the captured.
	local NUM_CH=$( get_num_char "${SAMPLE_FILE}" )

	if [[ ${NUM_CH} -le 50 ]]; then
		CMD="help"
		PID=$( start_capture "${PTY}" "${SAMPLE_FILE}" )
		generate_traffic "${PTY}" "${CMD}"

		# Stop capturing
		sleep 1
		kill ${PID} &>/dev/null

		# Calculate the number of characters from the captured.
		NUM_CH=$( get_num_char "${SAMPLE_FILE}" )

		[[ ${NUM_CH} -gt 50 ]] || die "Device UART is not responding"
	fi

	if [[ ${NUM_CH} -lt ${MIN_CHAR_SMPL} ]]; then
		MIN_CHAR_SMPL=${NUM_CH}
	fi
}

# Get sample output as base for comparison
[[ -n "${PTY1}" ]] && get_sample_txt "${PTY1}"
[[ -n "${PTY2}" ]] && get_sample_txt "${PTY2}"
[[ -n "${PTY3}" ]] && get_sample_txt "${PTY3}"

# Calculate the iteration to run console command for traffic.
REPEATS=$(( (${FLAGS_min_char} + ${MIN_CHAR_SMPL} - 1) / ${MIN_CHAR_SMPL} ))

# Start the stress test
info "Stress test on single UART"
[[ -n "${PTY1}" ]] && stress_test ${REPEATS} "${PTY1}"
[[ -n "${PTY2}" ]] && stress_test ${REPEATS} "${PTY2}"
[[ -n "${PTY3}" ]] && stress_test ${REPEATS} "${PTY3}"

if [[ ${UART_COUNT} -ge 2 ]]; then
	info "Stress test on two UARTs"
	[[ -n "${PTY1}" && -n "${PTY2}" ]] && \
		stress_test ${REPEATS} "${PTY1}" "${PTY2}"
	[[ -n "${PTY2}" && -n "${PTY3}" ]] && \
		stress_test ${REPEATS} "${PTY2}" "${PTY3}"
	[[ -n "${PTY1}" && -n "${PTY3}" ]] && \
		stress_test ${REPEATS} "${PTY1}" "${PTY3}"
fi

if [[ ${UART_COUNT} -ge 3 ]]; then
	info "Stress test on three UARTs"
	stress_test ${REPEATS} "${PTY1}" "${PTY2}" "${PTY3}"
fi

# Calculate average rate calculation
info "All tests done"

#######################################
# Print the average character loss rate
# Arguments:
#   $1: Device path
#######################################
print_char_loss_rate() {
	local CH_LOST=${CHAR_LOST["$1"]}
	local CH_EXPC=${CHAR_EXPC["$1"]}
	local STR="\t$1: ${CH_LOST} lost / ${CH_EXPC}"

	if [[ ${CH_LOST} -eq 0 ]]; then
		info "${STR} : PASS" | tee -a ${RESULT_FILE}
	else
		local RATE=$( calc_percent ${CH_LOST} ${CH_EXPC} )
		error "${STR} : ${RATE} %" | tee -a ${RESULT_FILE}
	fi
}

[[ -n "${PTY1}" ]] && print_char_loss_rate "${PTY1}"
[[ -n "${PTY2}" ]] && print_char_loss_rate "${PTY2}"
[[ -n "${PTY3}" ]] && print_char_loss_rate "${PTY3}"
