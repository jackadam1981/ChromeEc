#!/usr/bin/python2
# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file is a utility to quickly flash boards
# This file should be run outside of chroot. If it is run inside chroot, you may need to
# call with sudo, which is not officially supported.

import os
import subprocess as sp
import sys

# example of call this method will make
# make BOARD=nucleo-f072rb CTS_MODULE=gpio -j

th_board = 'stm32l476g-eval'

def make(module, dut_board, ecDirectory):
    sp.call(['make', '--directory=' + str(ecDirectory),
              'BOARD=stm32l476g-eval', 'CTS_MODULE=' + module, '-j'])

    sp.call(['make', '--directory=' + str(ecDirectory),
              'BOARD=' + dut_board, 'CTS_MODULE=' + module, '-j'])

def openocd_cmd(command_list, board_cfg):
    args = ['openocd', '-s', '/usr/local/share/openocd/scripts',
            '-f', board_cfg]
    for c in command_list:
        args.append('-c')
        args.append(c)
    args.append('-c')
    args.append('shutdown')
    sp.call(args)

def get_stlink_serial_numbers():
    usb_args = ['lsusb', '-v', '-d', '0x0483:0x374b']
    usb_process = sp.Popen(usb_args, stdout=sp.PIPE, shell=False)
    st_link_info = usb_process.communicate()[0]
    st_serials = []
    for line in st_link_info.split('\n'):
        if 'iSerial' in line:
            st_serials.append(line)
    serials = []
    for l in st_serials:
        serials.append(l.split()[2])
    return serials

# This function is necessary because the dut might be using an st-link debugger
# params: th_hla_serial is your personal th board's serial
def identify_dut(th_hla_serial):
    stlink_serials = get_stlink_serial_numbers()
    if len(stlink_serials) == 1:
        return None
    # If 2 st-link devices connected, find the one that doesn't have the TH serial number
    elif len(stlink_serials) == 2:
        dut = [s for s in stlink_serials if th_hla_serial not in s]
        if len(dut) != 1:
            return 'ERROR: Check your TH hla_serial'
        else:
            return dut[0] # Found your other st-link device serial!
    else:
        print 'ERROR: Please connect TH and your DUT and remove all other st-link devices'
        return None

def update_th_serial(dest_dir):
    serial = get_stlink_serial_numbers()
    if len(serial) != 1:
        print 'Connect your TH and remove other st-link devices'
    else:
        ser = serial[0]
        f = open(os.path.join(dest_dir, 'th_hla_serial'), mode='w')
        f.write(ser)
        f.close()
        return ser

def get_board_config_name(board):
    board_config_locs = {
        'stm32l476g-eval' : 'board/stm32l4discovery.cfg',
        'nucleo-f072rb' : 'board/st_nucleo_f0.cfg'
    }
    return board_config_locs[board]

def flash_boards(th_board, dut_board, th_hla_serial, dut_hla_serial=None):
    th_cfg = get_board_config_name(th_board)
    dut_cfg = get_board_config_name(dut_board)

    if(th_cfg == None or dut_cfg == None):
        return 'Board cfg files not found'

    th_flash_cmds = ['hla_serial ' + th_hla_serial,
                     'reset_config connect_assert_srst',
                     'init',
                     'reset init',
                     'flash write_image erase build/' + th_board + '/ec.bin 0x08000000',
                     'reset halt']

    dut_flash_cmds = ['reset_config connect_assert_srst',
                      'init',
                      'reset init',
                      'flash write_image erase build/' + dut_board + '/ec.bin 0x08000000',
                      'reset halt']

    if dut_hla_serial != None:
        dut_flash_cmds.insert(0, 'hla_serial ' + th_hla_serial)

    openocd_cmd(th_flash_cmds, th_cfg)
    openocd_cmd(dut_flash_cmds, dut_cfg)
    openocd_cmd(['init', 'reset init', 'resume'], th_cfg)
    openocd_cmd(['init', 'reset init', 'resume'], dut_cfg)

def main():
    path = os.path.abspath(__file__)
    ec_dir = os.path.join(os.path.dirname(path), '..')
    os.chdir(ec_dir)
    th_serial_dir = os.path.join(ec_dir, 'build', th_board)
    dut_board = ''
    module = ''

    if (len(sys.argv) == 1): #flash nucleo and eval board w/ gpio tests by default
        dut_board = 'nucleo-f072rb'
        module = 'gpio'

    elif(sys.argv[1] == '--th'):
        serial = update_th_serial(th_serial_dir)
        if(serial != None):
            print 'Your th hla_serial # has been saved as: ' + serial
        return

    elif(sys.argv[1] == '-m'):
        make('gpio', 'nucleo-f072rb', ec_dir)
        return

    elif(sys.argv[1] == '-h' or sys.argv[1] == '--help' or sys.argv[1] == 'help' or len(sys.argv) != 3):
        print '\n\nUsage:'
        print './flash_boards.py [cts_module] [dut_board]'
        print 'Options:\n=========='
        print '--th -- Connect only your th to get its serial number'
        return

    else:
        module = sys.argv[1]
        dut_board = sys.argv[2]

    make(module, dut_board, ec_dir)
    th_hla = open(os.path.join(th_serial_dir, 'th_hla_serial')).read()
    dut_hla = identify_dut(th_hla)
    flash_boards(th_board, dut_board, th_hla, dut_hla)

if __name__ == "__main__":
    main()