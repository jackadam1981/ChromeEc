# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Utility functions for flash related test
#

import random
import re

# Fixed random seed.
random.seed(1234)

def reverse_byte(x):
    r = 0
    for i in xrange(4):
        r = r * 256 + (x & 255)
        x = x / 256
    return r

def enc(x):
    return "%08x" % reverse_byte(x)

def get_flash_info(helper):
    helper.ec_command("hostcmd 0x10 0 00")
    resp = helper.wait_output("Response: (?P<r>.{32,32})", use_re=True)["r"]
    return (reverse_byte(int(resp[0:8], 16)),
            reverse_byte(int(resp[8:16], 16)),
            reverse_byte(int(resp[16:24], 16)),
            reverse_byte(int(resp[24:32], 16)))

def get_flash_size(helper):
    return get_flash_info(helper)[0]

def get_ro_size(helper):
    helper.ec_command("rosize")
    return int(helper.wait_output(
        "RO image size = (?P<ro>0x[0-9a-f]+)", use_re=True)["ro"], 16)

def xor_sum(size, seed, mult, add):
    ret = 0
    for i in xrange(size):
        ret ^= (seed & 0xff)
        seed = seed * mult + add
    return ret

def test_erase(helper, offset, size):
    helper.ec_command("hostcmd 0x13 0 %s%s" % (enc(offset), enc(size)))
    helper.wait_output("Flash erase at %x size %x" % (offset, size))

def _get_read_ref(helper, offset, size):
    ret = []
    retsub = []
    assert size % 4 == 0
    while size > 0:
        helper.ec_command("rw %d" % offset)
        h = helper.wait_output("read.*=\s+0x(?P<h>[0-9a-f]+)", use_re=True)["h"]
	# Change endianess here
        retsub.append(re.sub('(..)(..)(..)(..)', r'\4\3\2\1', h))
        if len(retsub) == 8:
            ret.append(''.join(retsub))
            retsub = []
        size = size - 4
        offset = offset + 4
    if retsub:
        ret.append(''.join(retsub))
    return ret

def test_read(helper, offset, size):
    ref = _get_read_ref(helper, offset, size)
    helper.ec_command("hostcmd 0x11 0 %s%s" % (enc(offset), enc(size)))
    #helper.ec_command("hcflashread %d %d" % (offset, size))
    for line in ref:
        helper.wait_output(line)

def _gen_data(size, seed, mult, add):
    data = []
    for i in xrange(size):
        data.append("%02x" % (seed & 255))
        seed = (seed * mult + add) & 4294967295;
    return ''.join(data)

def test_write(helper, offset, size, expect_fail=False):
    assert size <= 16
    seed = random.randint(2, 10000)
    mult = random.randint(2, 10000)
    add  = random.randint(2, 10000)
    data = _gen_data(size, seed, mult, add)
    helper.ec_command("hostcmd 0x12 0 %s%s%s" % (enc(offset), enc(size), data))
    if expect_fail:
        helper.wait_output("Command returned \d+", use_re=True)
    else:
        expected_sum = xor_sum(size, seed, mult, add)
        helper.wait_output("Flash write at %x size %x XOR %x" %
                           (offset, size, expected_sum))
