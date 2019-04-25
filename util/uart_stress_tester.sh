#!/bin/bash
#
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# ChromeOS UART Stress Test
#

SCRIPT="$(readlink -f "$0")"

# Loads script libraries.
. "/usr/share/misc/shflags" || exit 1

#Flags
DEFINE_boolean cr50 "${FLAGS_TRUE}"  "Test cr50 console"
DEFINE_boolean ec   "${FLAGS_TRUE}"  "Test ec console"
DEFINE_boolean cpu  "${FLAGS_FALSE}" "Test cpu(ap) console"
DEFINE_boolean verbose  "${FLAGS_FALSE}" "Print more messages"
DEFINE_boolean clean  "${FLAGS_FALSE}" "Delete all log data after test"

FLAGS_HELP="usage: $0 [flags]"
FLAGS "$@" || exit 1
eval set -- "${FLAGS_ARGV}"
if [[ $# -gt 0 ]]; then
	die "invalid arguments: \"$*\""
fi

set -e
set +x

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

info "ChromeOS UART stress test starts."

if dut-control -i &>/dev/null ; then
	CAPTURE_TOOL="dutctrl"

	info
	info $( dut-control servo_type )
	info $( dut-control ec_board )
	info $( dut-control ec_chip )
else
	CAPTURE_TOOL="cat"
fi

if dut-control -i cr50_version &>/dev/null ; then
	info $( dut-control cr50_version )
fi

# Get a terminal path.
PTY_CR50="/dev/ttyUSB0"
if dut-control -i cr50_uart_pty &>/dev/null ; then
	PTY_CR50=$( dut-control cr50_uart_pty )
	PTY_CR50="${PTY_CR50#*:}"
fi

PTY_EC="/dev/ttyUSB2"
if dut-control -i ec_uart_pty &>/dev/null ; then
	PTY_EC=$( dut-control ec_uart_pty )
	PTY_EC="${PTY_EC#*:}"
fi

PTY_CPU="/dev/ttyUSB1"
if dut-control -i cpu_uart_pty &>/dev/null ; then
	PTY_CPU=$( dut-control cpu_uart_pty )
	PTY_CPU="${PTY_CPU#*:}"
fi

info
if [[ "${FLAGS_cr50}" -eq "${FLAGS_TRUE}" ]]; then
	info "CR50 PTY= ${PTY_CR50}"
fi
if [[ "${FLAGS_ec}" -eq "${FLAGS_TRUE}" ]]; then
	info "  EC PTY= ${PTY_EC}"
fi
if [[ "${FLAGS_cpu}" -eq "${FLAGS_TRUE}" ]]; then
	info " CPU PTY= ${PTY_CPU}"
fi

# Suppress console output on cr50 and ec
if [[ "${FLAGS_cr50}" -eq "${FLAGS_TRUE}" ]]; then
	echo 'chan save' > "${PTY_CR50}"
	echo 'chan 0'    > "${PTY_CR50}"
fi

if [[ "${FLAGS_ec}" -eq "${FLAGS_TRUE}" ]]; then
	echo 'chan save' > "${PTY_EC}"
	echo 'chan 0'    > "${PTY_EC}"
fi

cleanup() {
	# Restore channel setting
	if [[ "${FLAGS_cr50}" -eq "${FLAGS_TRUE}" ]]; then
		echo 'chan restore' > "${PTY_CR50}"
	fi
	if [[ "${FLAGS_ec}" -eq "${FLAGS_TRUE}" ]]; then
		echo 'chan restore' > "${PTY_EC}"
	fi

	if [[ "${CAPTURE_TOOL}" == "dutctrl" ]] ; then
		dut-control cr50_uart_stream &>/dev/null
		dut-control ec_uart_stream &>/dev/null
		dut-control cpu_uart_stream &>/dev/null

		dut-control cr50_uart_capture:off
		dut-control ec_uart_capture:off
		dut-control cpu_uart_capture:off
	fi

	if [[ "${FLAGS_clean}" == "${FLAGS_TRUE}" ]]; then
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
	if [ $# -lt 1 ]; then
		error "${FUNCNAME[0]} failed: no argument passed"
		return 1
	fi

	local TARGET="$1"
	local PTY="PTY_${TARGET^^}"

	if [ $# -gt 1 ] ; then
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

declare -A CHAR_EXPC=( ["CR50"]=0 ["EC"]=0 ["CPU"]=0 )
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
	if [ $# -lt 4 ]; then
		error "${FUNCNAME[0]} failed: wrong number of arguments: $*"
		return 1
	fi

	local TARGET="$1"
	local FILE_SAMPLE="$2"
	local FILE_RESL="$3"
	local REPEATS="$4"

	local FILE_BASE="${FILE_SAMPLE}"."${REPEATS}"

	# Create a base text with the sample output, if not exists.
	if [[ ! -e ${FILE_BASE} ]]; then
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
	if [ ${CH_LOST} -lt 0 ] ; then
		error "Test output data seem corrupted:" \
			"${FILE_RESL} against ${FILE_BASE}"
		return 1
	fi

	# Calculate the character loss rate
	local LOSS_RATE=$( calc_percent ${CH_LOST} ${CH_EXPC} )

	# If the sizes are same each other, then compare the text.
	if [ ${CH_LOST} -eq 0 ] ; then
		if ! diff --brief ${FILE_BASE} ${FILE_RESL} ; then
			error "Test output data seem corrupted:" \
				"${FILE_RESL} against ${FILE_BASE}"
			return 1
		fi
		info "char loss rate ${TARGET} = ${LOSS_RATE} % :" \
			"$(( ${CH_LOST} )) lost among ${CH_EXPC}"
	else
		warn "char loss rate ${TARGET} = ${LOSS_RATE} % :" \
			"$(( ${CH_LOST} )) lost among ${CH_EXPC}"
	fi

	# Accumulate the data for average rate calcuation at the end.
	CHAR_EXPC["${TARGET}"]=$(( ${CHAR_EXPC["${TARGET}"]} + ${CH_EXPC} ))
	CHAR_LOST["${TARGET}"]=$(( CHAR_LOST["${TARGET}"] + ${CH_LOST} ))
}

get_sample_txt_cat() {
	die "Not implemented yet. Run servod and retry."
}

#######################################
# Enable servod to capture UART output
# Arguments:
#   $1: Target UART. Should be either "CR50", "EC", or "CPU"
#######################################
enable_uart_capture() {
	if [ $# -lt 1 ]; then
		error "${FUNCNAME[0]} failed: no argument passed."
	fi

	local TARGET="$1"
	TARGET="${TARGET,,}"

	dut-control "${TARGET}_uart_capture:on"

	# Drain all output first to clean up the buffer
	generate_uart_traffic "${TARGET}"
	sleep 2
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
	if [ $# -lt 1 ]; then
		error "${FUNCNAME[0]} failed: no argument passed."
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
	if [ $# -lt 2 ]; then
		error "${FUNCNAME[0]} failed: not enough arguments passed: $*"
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
	    -e 's/[0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2} //g' \
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

stress_test1_cat() {
	die "Not implemented yet"
}

stress_test2_cat() {
	die "Not implemented yet"
}

stress_test3_cat() {
	die "Not implemented yet"
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
	if [ $# -le 1 -o $# -gt 4 ] ; then
		error "${FUNCNAME[0]} failed: wrong number of arguments: $*"
		return 1
	else
		ITER="$1"

		TARGET1="$2"
		FILE_BASE1="SAMPLE_${TARGET1^^}"
	fi

	if [ $# -gt 2 ] ; then
		TARGET2="$3"
		FILE_BASE2="SAMPLE_${TARGET2^^}"
	fi

	if [ $# -gt 3 ] ; then
		TARGET3="$4"
		FILE_BASE3="SAMPLE_${TARGET3^^}"
	fi

	FILE_RESL1="${DIR_TMP}/result_${TARGET1}_with_${TARGET2}_${TARGET3}.cap"
	FILE_RESL2="${DIR_TMP}/result_${TARGET2}_with_${TARGET1}_${TARGET3}.cap"
	FILE_RESL3="${DIR_TMP}/result_${TARGET3}_with_${TARGET1}_${TARGET2}.cap"

	info
	info "Stress test on $(($# - 1)) UART(s), ${*:2} starts"

	[[ -n "${TARGET1}" ]] && enable_uart_capture "${TARGET1}"
	[[ -n "${TARGET2}" ]] && enable_uart_capture "${TARGET2}"
	[[ -n "${TARGET3}" ]] && enable_uart_capture "${TARGET3}"

	for (( i=1; i<=${ITER}; i++ )) do
		[[ -n "${TARGET1}" ]] && generate_uart_traffic "${TARGET1}"
		[[ -n "${TARGET2}" ]] && generate_uart_traffic "${TARGET2}"
		[[ -n "${TARGET3}" ]] && generate_uart_traffic "${TARGET3}"

		if [ $(( $i % 10 )) == 0 ]; then
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
		*) error "${FUNCNAME[0]} failed: wrong number of arguments: $*"
		   return 1
		   ;;
	esac
}

# Get sample output as base for comparison
if [[ "${FLAGS_cr50}" -eq "${FLAGS_TRUE}" ]]; then
	get_sample_txt_${CAPTURE_TOOL} "CR50"
fi

if [[ "${FLAGS_ec}" -eq "${FLAGS_TRUE}" ]]; then
	get_sample_txt_${CAPTURE_TOOL} "EC"
fi

if [[ "${FLAGS_cpu}" -eq "${FLAGS_TRUE}" ]]; then
	get_sample_txt_${CAPTURE_TOOL} "CPU"
fi

# Test on CR50 console
ITERATIONS=40
if [[ "${FLAGS_cr50}" -eq "${FLAGS_TRUE}" ]]; then
	sleep 2
	stress_test ${ITERATIONS} "CR50"
fi
# Test on EC console
if [[ "${FLAGS_ec}" -eq "${FLAGS_TRUE}" ]]; then
	sleep 2
	stress_test ${ITERATIONS} "EC"
fi
# Test on CPU console
if [[ "${FLAGS_cpu}" -eq "${FLAGS_TRUE}" ]]; then
	sleep 2
	stress_test ${ITERATIONS} "CPU"
fi
# Test on CR50 + EC console
if [[ "${FLAGS_cr50}" -eq "${FLAGS_TRUE}" ]] &&
   [[ "${FLAGS_ec}" -eq "${FLAGS_TRUE}" ]]; then
	sleep 2
	stress_test ${ITERATIONS} "CR50" "EC"
fi
# Test on EC + CPU console
if [[ "${FLAGS_ec}" -eq "${FLAGS_TRUE}" ]] &&
   [[ "${FLAGS_cpu}" -eq "${FLAGS_TRUE}" ]] ; then
	sleep 2
	stress_test ${ITERATIONS} "EC" "CPU"
fi
# Test on CR50 + CPU console
if [[ "${FLAGS_cr50}" -eq "${FLAGS_TRUE}" ]] &&
   [[ "${FLAGS_cpu}" -eq "${FLAGS_TRUE}" ]] ; then
	sleep 2
	stress_test ${ITERATIONS} "CR50" "CPU"
fi
# Test on CR50 + EC + CPU console
if [[ "${FLAGS_cr50}" -eq "${FLAGS_TRUE}" ]] &&
   [[ "${FLAGS_ec}" -eq "${FLAGS_TRUE}" ]] &&
   [[ "${FLAGS_cpu}" -eq "${FLAGS_TRUE}" ]] ; then
	sleep 2
	stress_test ${ITERATIONS} "CR50" "EC" "CPU"
fi

# Calculate average rate calcuation
info
if [[ "${FLAGS_cr50}" -eq "${FLAGS_TRUE}" ]] ; then
	RATE=$( calc_percent ${CHAR_LOST["CR50"]} ${CHAR_EXPC["CR50"]} )
	STR="aver. char loss rate, CR50 = ${RATE} %"

	if [ ${CHAR_LOST["CR50"]} -eq 0 ]; then
		info "${STR}"
	else
		warn "${STR}"
	fi
fi
if [[ "${FLAGS_ec}" -eq "${FLAGS_TRUE}" ]] ; then
	RATE=$( calc_percent ${CHAR_LOST["EC"]} ${CHAR_EXPC["EC"]} )
	STR="aver. char loss rate, EC   = ${RATE} %"

	if [ ${CHAR_LOST["EC"]} -eq 0 ]; then
		info "${STR}"
	else
		warn "${STR}"
	fi
fi
if [[ "${FLAGS_cpu}" -eq "${FLAGS_TRUE}" ]] ; then
	RATE=$( calc_percent ${CHAR_LOST["CPU"]} ${CHAR_EXPC["CPU"]} )
	STR="aver. char loss rate, CPU  = ${RATE} %"

	if [ ${CHAR_LOST["CPU"]} -eq 0 ]; then
		info "${STR}"
	else
		warn "${STR}"
	fi
fi

info "All tests done"
