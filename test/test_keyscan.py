#!/usr/bin/python

import os
import sys

# run tests
def test():
	for upto in range(500, 5000, 250):
		print upto
		cmd = ['./ectool', 'keyscan', '%d' % upto, 'key_sequence.txt']
		os.system(' '.join(cmd))

def make_bounce_test(fd, start):
    print >>fd, """
test keybounce
expect ab
seq 0
    """

    # press a
    print >>fd, """seq %d a
seq %d
seq %d a
seq %d
seq %d a""" % tuple(range(start, start + 5))

    # press b also
    start += 100
    print >>fd, """seq %d ab
seq %d a
seq %d ab
seq %d a
seq %d ab""" % tuple(range(start, start + 5))

    # release a
    start += 10
    print >>fd, """seq %d b
seq %d ab
seq %d b
seq %d ab
seq %d b""" % tuple(range(start, start + 5))

    # release b
    start += 100
    print >>fd, """seq %d
seq %d b
seq %d
seq %d b
seq %d""" % tuple(range(start, start + 5))
    print >>fd, 'endtest'

ectool = '../../../chroot/build/daisy/usr/bin/ectool'
beat = 400

ok, fail = 0, 0
for start in range(18, 100):
    fd = open("seq.txt", "w")
    make_bounce_test(fd, start)
    fd.close()

    cmd = [ectool, 'keyscan', '%d' % beat, 'seq.txt', '>/dev/null']
    ret = os.system(' '.join(cmd))
    #print ret
    if ret:
        print '[fail %d]  ' % start,
        sys.stdout.flush()
        fail += 1
    else:
        ok += 1

print
print 'pass %d, fail %d\n' % (ok, fail)
