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
# Use: Run uart_stress_tester.sh --help.
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
#     Turn off CR50 and EC uart output channels with the console command
#     'chan 0'
#     If servod is running, turn uart_timestamp off before running this test.
#     e.g. dut-control cr50_uart_timestamp:off
#

SCRIPT_NAME="$(basename "$0")"

# Load chromeOS common bash library
. "/mnt/host/source/src/scripts/common.sh" || exit 1

# Loads script libraries.
. "/usr/share/misc/shflags" || exit 1

SAVEIFS=${IFS}

#Flags
DEFINE_string pty "" "List of UART device path(s) to test"
DEFINE_integer min_char  "40000" "Minimum number of characters to generate."

FLAGS_HELP="$(cat <<EOF
usage: ${SCRIPT_NAME} [flags]
example:
  ${SCRIPT_NAME} --pty /dev/ttyUSB0 --min_char 100000
  ${SCRIPT_NAME} --pty="/dev/ttyUSB0 /dev/ttyUSB1,/dev/ttyUSB2"
EOF
)"
FLAGS "$@" || exit 1
eval set -- "${FLAGS_ARGV}"

if [[ $# -gt 0 ]]; then
	die "invalid arguments: \"$*\""
fi

set -e

# Trap function on EXIT
cleanup() {
	# Delete all temp files if requested to do so
	local LINK_LATEST="/tmp/${SCRIPT_NAME}_latest"
	unlink "${LINK_LATEST}" &>/dev/null || :

	[[ -e "${DIR_TMP}" ]] && ln -s "${DIR_TMP}" "${LINK_LATEST}"

	info "Test files are in ${LINK_LATEST}"
	${TEST_PASS} && info "PASS" || error "FAIL"

	[[ -n "${SAVEIFS}" ]] && IFS=${SAVEIFS}

	# Kill any background processes
	kill SIGTERM $(jobs -p) &>/dev/null
	wait
}
trap cleanup EXIT

#######################################
# Calculate the number of characters
# Arguments:
#   $1: Input text file
# Returns:
#   The number of characters from the input file
#######################################
get_num_char() {
	echo $( wc -c < "$1" )
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

#######################################
# Calculate the character loss rate based on the given test files.
# Arguments:
#   $2: Base sample text file to compare with
#   $3: Test text file to compare against
#   $4: Count that a console command repeated to get a test file
#######################################
calc_char_loss_rate() {
	[[ $# -eq 4 ]] || die "${FUNCNAME[0]}: argument error: $*"

	local PTY="$1"
	local FILE_SPL="$2"
	local FILE_COMP="$3"
	local REPEATS=$4
	local FILE_BASE="${FILE_SPL%.*}_${REPEATS}.cap"

	# Create a base text with the sample output, if not exists.
	if [[ ! -e "${FILE_BASE}" ]]; then
		for (( i=1; i<=${REPEATS}; i++ )) do
			cat "${FILE_SPL}"
		done > "${FILE_BASE}"
	fi

	# Count the characters in captured data files, and get the difference
	# between them.
	local CH_EXPC=$( get_num_char "${FILE_BASE}" )
	local CH_RESL=$( get_num_char "${FILE_COMP}" )
	local CH_LOST=$(( ${CH_EXPC} - ${CH_RESL} ))

	# Calculate the character loss rate
	local STR="$1: ${CH_LOST} lost / ${CH_EXPC}"

	# Check if test output is not smaller than expected.
	# If so, it must contain some other pattern.
	if [[ ${CH_LOST} -eq 0 ]]; then
		# If the sizes are same each other, then compare the text.
		if diff --brief "${FILE_BASE}" "${FILE_COMP}"; then
			info "${STR} : 0 %"
			return
		else
			error "${FILE_COMP} does not match to ${FILE_BASE}"
		fi
	elif [[ ${CH_LOST} -gt 0 ]]; then
		local RATE=$( calc_percent ${CH_LOST} ${CH_EXPC} )
		error "${STR} : ${RATE} %"
	else
		error "${FILE_COMP} corrupted: $(( -${CH_LOST} )) more found"
	fi

	TOTAL_CH_LOST=$(( ${TOTAL_CH_LOST} + ${CH_LOST} ))
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

	local PTY="$1"
	local FILE_CAP="$2"

	local STTY_ARGS=( "cs8" "ignbrk" "igncr" "noflsh" "-brkint" "-clocal"
		    "-echo" "-echoe" "-echok" "-echoctl" "-echoke"
		    "-icanon" "-icrnl" "-iexten" "-imaxbel" "-isig" "-ixon"
		    "-onlcr" "-opost" )
	stty -F "${PTY}" "${STTY_ARGS[@]}" || die "stty failed: ${STTY_ARGS[*]}"

	# Drain output
	cat "${PTY}" &>/dev/null &
	local PID=$!

	sleep 2
	kill ${PID} &>/dev/null
	wait

	# Start to capture
	cat "${PTY}" > "${FILE_CAP}" 2>/dev/null &
	echo $!
}

#######################################
# Run a UART stress test on target device(s)
# Arguments:
#   $1: Number of times to run a console command
#######################################
stress_test() {
	# Check the number of arguments.
	[[ $# -gt 1 ]] || die "${FUNCNAME[0]}: wrong number of arguments: $*"

	local ITER=$1
	shift
	local TEST_PTYS=( "$@" )
	local PIDS=()

	# Start to capture
	for pd in "${TEST_PTYS[@]}"; do
		FILE_RES["${pd}"]="${DIR_TMP}/$(basename ${pd})_res_${ITER}.cap"
		PIDS+=( $( start_capture "${pd}" "${FILE_RES["${pd}"]}" ) )
	done

	# Generate traffic
	for (( i=1; i<=${ITER}; i++ )) do
		for pd in "${TEST_PTYS[@]}"; do
			echo "${CONSOLE_CMDS["${pd}"]}" > "${pd}"
		done

		[[ $(( $i % 10 )) -eq 0 ]] || continue

		echo -n "."
		sleep 2
	done
	echo

	# Stop capturing
	sleep 5
	for pid in ${PIDS[@]}; do
		kill ${pid} &>/dev/null
	done
	wait
}

MIN_CHAR_SMPL=99999999
#######################################
# Run a UART stress test on target device(s)
# Global Variables:
#       FILE_SAMPLE
#       CONSOLE_CMDS
# Arguments:
#       @: Device paths
#######################################
get_sample_txt() {
	local FILE_CAP
	local PID
	local NUM_CH
	local CMDS=( "" "help" )

	for pd in "$@"; do
		FILE_CAP="${DIR_TMP}/$(basename ${pd})_sample.cap"

		for cmd in "${CMDS[@]}"; do
			# Start to capture
			PID=$( start_capture "${pd}" "${FILE_CAP}" )

			echo "${cmd}" >  "${pd}"

			# Stop capturing
			sleep 1
			kill ${PID} &>/dev/null
			wait

			# Calculate the number of characters from the captured.
			NUM_CH=$( get_num_char "${FILE_CAP}" )

			if [[ ${NUM_CH} -gt 50 ]]; then
				CONSOLE_CMDS["${pd}"]="${cmd}"
				break
			fi
		done

		[[ ${NUM_CH} -gt 50 ]] || die "${pd} does not seem to respond"

		if [[ ${NUM_CH} -lt ${MIN_CHAR_SMPL} ]]; then
			MIN_CHAR_SMPL=${NUM_CH}
		fi

		FILE_SAMPLE["${pd}"]="${FILE_CAP}"
	done
}

info "ChromeOS UART stress test starts."

declare -A CONSOLE_CMDS
declare -A FILE_SAMPLE
declare -A FILE_RES
TOTAL_CH_LOST=0
TEST_PASS=false

# Check whether the given devices are available.
IFS=" ,|"
read -a PTYS <<< "${FLAGS_pty}"
IFS=${SAVEIFS}

[[ ${#PTYS[@]} -gt 0 ]] || \
	die "Flag '--pty' value, ${FLAGS_pty} is not correct."

for pd in "${PTYS[@]}"; do
	[[ -e "${pd}" ]] || die "Device '${pd}' does not exist."
	if lsof -V "${pd}" &>/dev/null; then
		die "${pd} is already opened"
	fi
done

DIR_TMP="$( mktemp -d --suffix=.${SCRIPT_NAME} )"

# Get sample output as base for comparison
get_sample_txt "${PTYS[@]}"

# Calculate the iteration to run console command for traffic.
REPEATS=$(( (${FLAGS_min_char} + ${MIN_CHAR_SMPL} - 1) / ${MIN_CHAR_SMPL} ))

# Start the stress test
info "UART devices: ${PTYS[*]}"
stress_test ${REPEATS} "${PTYS[@]}"

# Calculate average rate calculation
for pd in "${PTYS[@]}"; do
	calc_char_loss_rate "${pd}" "${FILE_SAMPLE["${pd}"]}" \
			"${FILE_RES["${pd}"]}" ${REPEATS}
done

[[ ${TOTAL_CH_LOST} -eq 0 ]] && TEST_PASS=true
