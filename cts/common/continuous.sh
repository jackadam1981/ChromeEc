#!/bin/bash

CHROMIUMOS_SRC_DIR="$HOME/chromiumos/src"
EC_DIR="$CHROMIUMOS_SRC_DIR/platform/ec"
TESTS="meta interrupt gpio task timer"

error()
{
	# To avoid conflict with other redirections, use subshell
	(echo -e "[eCTS] $*" >&2)
}

usage()
{
	echo "Run eCTS continuously and publish results"
}

sync()
{
	cd $EC_DIR
	if ! repo sync .; then
		error "Failed to sync source"
		exit 1
	fi
}

run_test()
{
	cros_sdk -- \$HOME/trunk/src/platform/ec/cts/cts.py -m $1
}

run()
{
	for test in $TESTS; do
		run_test "$test"
	done
}

upload_results()
{
	echo "Uploading results..."
}

main()
{
	sync
	run
	upload_results
}

while getopts ":rsvh" opt; do
	case "$opt" in
	h)
		usage
		exit 0
		;;
	r)
		run
		exit 0
		;;
	s)
		sync
		exit 0
		;;
	v)
		dbash_verbose=y
		;;
	\?)
		echo "invalid option: -${OPTARG}"
		exit 1
		;;
	esac
done
shift $((OPTIND-1))

main