#!/bin/bash
#
# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

flat_section_start='fsf'
flat_section_mid='fsm'
flat_section_end='fse'
flat_section_mid_sorted='fsms'

# $1 - string to search for
# $2 - file containing the string
function line_num_of_str()
{
	local ret=''
	ret=( $(awk '/'$1'/{ print NR }' $2) )
	echo ${ret[0]}
}

# $1 - line number to read
# $2 - file to read line from
function get_line()
{
	local ret=''
	ret="`sed -n ''$1'p' $2`"
	echo "$ret"
}

# $1 - value to swap
function swap32()
{
	local ret=''
	local val=$1
	ret=`echo $val | grep -o .. | tac | echo "$(tr -d '\n')"`
	echo "$ret"
}

# $1 - start of array
# $2 - end of array
function quicksort()
{
	left=$1
	right=$2

	if [[ $1 -lt $2 ]]
	then
		pivot=${cmd_array[$1]}

		while (( $left < $right ))
		do
			while ((${cmd_array[$left]} <= $pivot && $left < $2))
			do
				left=$(($left + 1))
			done

			while ((${cmd_array[$right]} > $pivot))
			do
				right=$(($right-1))
			done

			if [[ $left -lt $right ]]
			then
				temp=${cmd_array[$left]}
				cmd_array[$left]=${cmd_array[$right]}
				cmd_array[$right]=$temp

				temp2=${str_array[$left]}
				str_array[$left]=${str_array[$right]}
				str_array[$right]=$temp2
			fi
		done

		temp=${cmd_array[$right]}
		cmd_array[$right]=${cmd_array[$1]}
		cmd_array[$1]=$temp
		temp=$right

		temp2=${str_array[$right]}
		str_array[$right]=${str_array[$1]}
		str_array[$1]=$temp2
		temp2=$right

		# pivot used in each case are printed for evaluation

		quicksort $1 $((right-1)) cmd_array
		quicksort $((temp+1)) $2 cmd_array
	fi
}

# $1 *.flat
# $2 *.elf
# $3 *.smap
main() {
	declare -a off_array
	declare -a cmd_array
	declare -a str_array
	declare -a line_array
	local ln=''
	local line=''
	local arr=''

	local flat_file=$1
	local smap_file=$3
	local base_address=''
	local hc_start=''
	local hc_end=''
	local hc_len=''
	local hc_size=''
	local hc_num=''
	local header=0

	# make sure the files exist
	if [ ! -e "$flat_file" ]; then
		exit 0
	fi

        if [ ! -e "$smap_file" ]; then
                exit 0
        fi

        # set base_address
        ln=$( line_num_of_str "vectors" $smap_file )
        line=$( get_line $ln $smap_file )
        arr=(`echo ${line}`)
        base_address=${arr[0]}

	if [ -z "$base_address" ]; then
		exit 0
	fi

        # set hc_start and hc_end
        hc_start=$(($( line_num_of_str "__hcmds" $smap_file )+1))
        hc_end=$(($( line_num_of_str "__hcmds_end" $smap_file )-1))

	if [ -z "$hc_start" ]; then
		 exit 0
	fi

        if [ -z "$hc_end" ]; then
                 exit 0
        fi

	if [ "$hc_start" -eq "$hc_end" ]; then
		exit 0
	fi

        if [ "$hc_start" -eq "$(($hc_end - 1))" ]; then
                exit 0
        fi

        if [ "$hc_start" -gt "$hc_end" ]; then
                exit 0
        fi

	# ec.RO file has a header
	header=$( line_num_of_str "fw_header" $smap_file )
	if [ -z "$header" ]; then
		header=0
	else
		header=64
	fi

	# read all host commands into array
	let i=0
	for itr in `seq $hc_start $hc_end`;
	do
		line=(`echo $( get_line $itr $smap_file )`)
		d1=$((16#${line[0]}))
		d2=$((16#$base_address))
		off_array[$i]=$((d1-d2))
		str_array[$i]=${line[2]}

		let i=i+1
	done
	let i-=1

	# Exit if no host commands are found
	zero=0
	if [ "$i" -eq "$zero" ]; then
		exit 0
	fi

	# Exit if only one host commands is found
	one=1
	if [ "$i" -eq "$one" ]; then
		exit 0
	fi

	# Find the size of a single host command
	counter=0
	while [ $counter -lt $i ];
	do
		if [ "${off_array[$((counter+1))]}" \
			-gt "${off_array[$counter]}" ];
		then
			hc_size=$((${off_array[$((counter+1))]}
				- ${off_array[$counter]}))
			let counter=$i
		fi
		let counter=counter+1
	done

	hc_end=${off_array[$i]}
	hc_start=${off_array[0]}
	hc_len=$(($hc_size + $hc_end - $hc_start))
	hc_num=$(($hc_len / $hc_size))

	# create temp dir to hold all the hc fragments
	dir=`mktemp -d`
	cp $flat_file $dir

	# break flat file into three sections
	dd if=$flat_file of=$dir/$flat_section_start bs=1 \
		count=$(($hc_start + $header)) 2>/dev/null
	dd if=$flat_file of=$dir/$flat_section_mid bs=1 \
		count=$hc_len skip=$(($hc_start + $header)) 2>/dev/null
	dd if=$flat_file of=$dir/$flat_section_end bs=1 \
		skip=$(($hc_start + $header + $hc_len)) 2>/dev/null

	# create a file for each host command containing its bytes
	offset=0
	for ((i=0; i < ${#str_array[@]}; i++))
	do
		dd if=$dir/$flat_section_mid of=$dir/${str_array[$i]} bs=1 \
			count=$hc_size skip=$offset 2>/dev/null
		cmd_array[$i]=$((16#$( swap32 "$(xxd -p -l 4 -s 4 \
			$dir/${str_array[$i]})" )))
		let offset=offset+$hc_size
	done

	# sort host command
	quicksort 0 $((hc_num-1)) cmd_array

	# put host commands back together in sorted oreder
	cp $dir/${str_array[0]} $dir/$flat_section_mid_sorted
	for ((i=1; i < ${#cmd_array[@]}; i++))
	do
		cat $dir/${str_array[$i]} >> $dir/$flat_section_mid_sorted
	done

	# update flat file
	cp $dir/$flat_section_start $flat_file
	cat $dir/$flat_section_mid_sorted >> $flat_file
	cat $dir/$flat_section_end >> $flat_file

	# remove temp directory
	rm -r $dir

	exit 0
}

main "$@"
