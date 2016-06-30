#!/usr/bin/python2
# This file is a utility to quickly flash boards. Called by calling make BOARD=<boardname> flash_cts

# -*- makefile -*-
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import subprocess as sp
import sys

# example of call this method will make
# make BOARD=nucleo-f072rb CTS_MODULE=gpio flash -j

def flash(module, dut_board, ecDirectory, dut_hla_serial=None, th_hla_serial=None):

    if (th_hla_serial == '' or th_hla_serial == None):
        sp.call(['make', '--directory=' + str(ecDirectory),
                  'BOARD=stm32l476g-eval', 'CTS_MODULE=' + module, '-j', 'flash'])

    if (dut_hla_serial == '' or dut_hla_serial == None):
        print 'Flashing dut'
        sp.call(['make', '--directory=' + str(ecDirectory),
                  'BOARD=' + dut_board, 'CTS_MODULE=' + module, '-j', 'flash'])

def main():
    if (len(sys.argv) == 1): #flash nucleo and eval board by default
        flash('gpio', 'nucleo-f072rb', '~/chromiumos/src/platform/ec')
    elif(sys.argv[1] == '-h' or sys.argv[1] == '--help' or sys.argv[1] == 'help' or len(sys.argv) != 3):
            print '\n\nUsage:'
            print './flash_boards.py [cts_module] [dut_board]'
    else:
        flash(sys.argv[1], sys.argv[2])

if __name__ == "__main__":
    main()