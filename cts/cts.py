#!/usr/bin/python2
# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file is a utility to quickly flash boards

import select
import time
import fcntl
import os
import subprocess as sp
import sys
import argparse
import collections

# example of call this method will make
# make BOARD=nucleo-f072rb CTS_MODULE=gpio -j

ocd_script_dir = '/usr/local/share/openocd/scripts'
th_board = 'stm32l476g-eval'
th_ser_file = 'th_hla_serial'
results_dir = '/tmp/results'
ec_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')

def make(module, dut_board, ecDirectory):
  sp.call(['make', '--directory=' + str(ecDirectory),
        'BOARD=stm32l476g-eval', 'CTS_MODULE=' + module, '-j'])

  sp.call(['make', '--directory=' + str(ecDirectory),
        'BOARD=' + dut_board, 'CTS_MODULE=' + module, '-j'])

def openocd_cmd(command_list, board_cfg):
  args = ['openocd', '-s', ocd_script_dir,
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
      st_serials.append(line.split()[2])
  return st_serials

# This function is necessary because the dut might be using an st-link debugger
# params: th_hla_serial is your personal th board's serial
def identify_dut(th_hla_serial):
  stlink_serials = get_stlink_serial_numbers()
  if len(stlink_serials) == 1:
    return ''
  # If 2 st-link devices connected, find dut's serial number
  elif len(stlink_serials) == 2:
    dut = [s for s in stlink_serials if th_hla_serial not in s]
    if len(dut) != 1:
      raise RuntimeError('Incorrect TH hla_serial')
      return None
    else:
      return dut[0] # Found your other st-link device serial!
  else:
    msg = 'Please connect TH and your DUT ' + \
        'and remove all other st-link devices'
    raise RuntimeError(msg)
    return None

def update_th_serial(dest_dir):
  serial = get_stlink_serial_numbers()
  if len(serial) != 1:
    msg = 'TH could not be identified.'
    msg += '\nConnect your TH and remove other st-link devices'
    raise RuntimeError(msg)
  else:
    ser = serial[0]
    dest = os.path.join(dest_dir, th_ser_file)
    if not os.path.exists(os.path.dirname(dest)):
      os.makedirs(os.path.dirname(dest))
    f = open(dest, mode='w')
    f.write(ser)
    f.close()
    return ser

def get_board_config_name(board):
  board_config_locs = {
    'stm32l476g-eval' : 'board/stm32l4discovery.cfg',
    'nucleo-f072rb' : 'board/st_nucleo_f0.cfg'
  }
  return board_config_locs[board]

def flash_boards(dut_board, th_hla, dut_hla=''):
  th_cfg = get_board_config_name(th_board)
  dut_cfg = get_board_config_name(dut_board)

  if(th_cfg == None or dut_cfg == None):
    msg = 'Board cfg files not found'
    raise RuntimeError(msg)

  th_flash_cmds = ['hla_serial ' + th_hla,
           'reset_config connect_assert_srst',
           'init',
           'reset init',
           'flash write_image erase build/' + th_board + '/ec.bin 0x08000000',
           'reset halt']

  dut_flash_cmds = ['hla_serial ' + dut_hla,
            'reset_config connect_assert_srst',
            'init',
            'reset init',
            'flash write_image erase build/' + dut_board + '/ec.bin 0x08000000',
            'reset halt']

  openocd_cmd(th_flash_cmds, th_cfg)
  openocd_cmd(dut_flash_cmds, dut_cfg)
  openocd_cmd(['hla_serial ' + th_hla,
               'init',
               'reset init',
               'resume'],
               th_cfg)
  openocd_cmd(['hla_serial ' + dut_hla,
               'init',
               'reset init',
               'resume'],
               dut_cfg)

def get_serials(th_serial_loc):
  try:
    th_hla = open(th_serial_loc).read()
  except:
    msg = ('Your th hla_serial may not have been saved. \n'
         'Connect only your th and run ./cts --th, then try again.')
    raise RuntimeError(msg)
  dut_hla = identify_dut(th_hla)
  return th_hla, dut_hla

def reset_boards(dut_board, th_hla, dut_hla=''):
  th_cfg = get_board_config_name(th_board)
  dut_cfg = get_board_config_name(dut_board)

  if(th_cfg == None or dut_cfg == None):
    msg = 'Board cfg files not found'
    raise RuntimeError(msg)

  openocd_cmd(['hla_serial ' + dut_hla, 'init', 'reset init'], dut_cfg)
  openocd_cmd(['hla_serial ' + th_hla, 'init', 'reset init'], th_cfg)
  openocd_cmd(['hla_serial ' + th_hla, 'init', 'resume'], th_cfg)
  openocd_cmd(['hla_serial ' + dut_hla, 'init', 'resume'], dut_cfg)

# Read one byte at a time while file is available for reading
def select_read_serial(fd):
  buf = []
  while(True):
    if select.select([fd],[],[],1)[0]:
      buf.append(os.read(fd,1))
    else:
      break
  result = ''.join(buf)
  return result

# Reads available bytes from path
def os_setup_serial(path):
  fd = os.open(path, os.O_RDONLY)
  flag = fcntl.fcntl(fd, fcntl.F_GETFL)
  fcntl.fcntl(fd, fcntl.F_SETFL, flag | os.O_NONBLOCK)
  return fd

def get_dev_filenames():
  com_files = [f for f in os.listdir('/dev/') if f.startswith('ttyACM')]
  if len(com_files) < 2:
    raise RuntimeError('The device dev paths could not be found')
  elif len(com_files) > 2:
    raise RuntimeError('Too many serial devices connected to host')
  else:
    return ('/dev/' + com_files[0], '/dev/' + com_files[1])

# Returns arguments for single arg macro in a file, in order
def get_macro_args(filepath, macro):
  args = []
  f = open(filepath, 'r')
  for l in [l for l in f.readlines() if l.strip().startswith(macro)]:
    l = l.strip()[len(macro):]
    args.append(l.strip('()'))
  return args

# Returns OrderedDict where key is test name and value is error code string
def parse_output(r1, r2, module):
  testlist_path = os.path.join(ec_dir, 'cts', module, 'cts.testlist')
  test_names = get_macro_args(testlist_path, 'CTS_TEST')
  error_codes_path = os.path.join(ec_dir, 'cts', 'common', 'cts.error_codes')
  error_codes = get_macro_args(error_codes_path, 'CTS_ERROR_CODE')

  # Keys are test names, values are error codes (integers)
  results = collections.OrderedDict()

  for t in test_names:
    results[t] = 0 # default results are "Unknown"

  for output_str in [r1, r2]:
    for l in [l.strip() for l in output_str.split('\n')]:
      tokens = l.split()
      if len(tokens) != 2:
        continue
      elif tokens[0].strip() not in test_names:
        continue
      elif int(tokens[1]) == 0:
        continue
      elif results[tokens[0]] != 0:
        if int(tokens[1]) == results[tokens[0]]:
          continue
        else:
          raise RuntimeError('Conflicting Test Results')
      else:
        results[tokens[0]] = int(tokens[1])

  # Convert codes to their strings
  for test in results.keys():
    results[test] = error_codes[results[test]]

  return results

def stringify_results(results, module):
  t_long = 0
  e_max_len = 0

  for test, code in results.items():
    if len(test) >  t_long:
      t_long = len(test)
    if len(code) > e_max_len:
      e_max_len = len(code)

  pretty_results = 'CTS Test Results for ' + module + ' module:\n'

  for test, code in results.items():
    align_str = '\n{0:<' + str(t_long) + '} {1:>' + str(e_max_len) + '}'
    pretty_results += align_str.format(test, code)

  return pretty_results

def reset_and_record(dut_board, module, th_hla, dut_hla):
  dest = os.path.join(results_dir, dut_board, module + '.txt')
  if not os.path.exists(os.path.dirname(dest)):
    os.makedirs(os.path.dirname(dest))
  d1, d2 = get_dev_filenames()

  try:
    fd1 = os_setup_serial(d1)
    fd2 = os_setup_serial(d2)
  except: # If board was just connected, must be reset to be read from
    for i in range(3):
      reset_boards(dut_board, th_hla, dut_hla)
      time.sleep(10)
      try:
        fd1 = os_setup_serial(d1)
        fd2 = os_setup_serial(d2)
        break
      except:
        continue

  select_read_serial(fd1) # clear any junk from buffer
  select_read_serial(fd2)
  reset_boards(dut_board, th_hla, dut_hla)
  time.sleep(3)
  r1 = select_read_serial(fd1)
  r2 = select_read_serial(fd2)
  results = parse_output(r1, r2, module)
  pretty_results = stringify_results(results, module)
  f = open(dest, 'w')
  f.write(pretty_results)
  f.close()
  print pretty_results

def main():
  global ocd_script_dir
  global ec_dir
  os.chdir(ec_dir)
  th_ser_dir = os.path.join(ec_dir, 'build', th_board)

  th_hla = None
  dut_hla = None
  dut_board = 'nucleo-f072rb' #nucleo by default
  module = 'gpio' #gpio by default

  parser = argparse.ArgumentParser(description='Used to build/flash boards')
  parser.add_argument('-d',
            '--dut',
            help='Specify DUT you want to build/flash')
  parser.add_argument('-m',
            '--module',
            help='Specify module you want to build/flash')
  parser.add_argument('-t',
            '--th',
            action='store_true',
            help='Connect only the th to save its serial')
  parser.add_argument('-b',
            '--build',
            action='store_true',
            help='Build test suite (no flashing)')
  parser.add_argument('-f',
            '--flash',
            action='store_true',
            help='Flash boards with last image built for them')
  parser.add_argument('-e',
            '--reset',
            action='store_true',
            help='Reset boards')
  parser.add_argument('-r ',
            '--run',
            action='store_true',
            help='Run and record test results (doesn\'t flash)')

  args = parser.parse_args()
  args = parser.parse_args()

  if args.th:
    serial = update_th_serial(th_ser_dir)
    if(serial != None):
      print 'Your th hla_serial # has been saved as: ' + serial
    else:
      raise RuntimeError('Unable to save serial')
    return

  if args.module:
    module = args.module

  if args.dut:
    dut_board = args.dut

  if args.reset:
    th_hla, dut_hla = get_serials(os.path.join(th_ser_dir, th_ser_file))
    reset_boards(dut_board, th_hla, dut_hla)

  elif args.build:
    make(module, dut_board, ec_dir)

  elif args.flash:
    th_hla, dut_hla = get_serials(os.path.join(th_ser_dir, th_ser_file))
    flash_boards(dut_board, th_hla, dut_hla)

  elif args.run:
    th_hla, dut_hla = get_serials(os.path.join(th_ser_dir, th_ser_file))
    reset_and_record(dut_board, module, th_hla, dut_hla)

  else:
    make(module, dut_board, ec_dir)
    th_hla, dut_hla = get_serials(os.path.join(th_ser_dir, th_ser_file))
    flash_boards(dut_board, th_hla, dut_hla)

if __name__ == "__main__":
  main()