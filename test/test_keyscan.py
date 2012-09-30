#!/usr/bin/python

import os

# run tests
for upto in range(500, 5000, 250):
	print upto
	cmd = ['./ectool', 'keyscan', '%d' % upto, 'key_sequence.txt']
	os.system(' '.join(cmd))

