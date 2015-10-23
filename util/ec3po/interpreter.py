#!/usr/bin/python2
# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""EC-3PO EC Interpreter

interpreter provides the interpretation layer between the EC UART and the user.
It recives commands through its command pipe, formats the commands for the EC,
and sends the command to the EC.  It also presents data from the EC to either be
displayed via the interactive console interface, or some other consumer.  It
additionally supports automatic command retrying if the EC drops a character in
a command.
"""
from __future__ import print_function
from chromite.lib import cros_logging as logging
import os
import select
import time


COMMAND_RETRIES = 3 # Number of attempts to retry a command.
PIPE_POLL_TIMEOUT = 0.001 # Seconds to poll for data in the pipe.
EC_POLL_TIMEOUT = 0.001 # Seconds to poll for data from the EC.
EC_MAX_READ = 1024 # Max bytes to read at a time from the EC.

class Interpreter(object):
  """Class which provides the interpretation layer between the EC and user.

  This class essentially performs all of the intepretation for the EC and the
  user.  It handles all of the automatic command retrying as well as the
  formation of commands.

  Attributes:
    ec_uart_pty: A string representing the EC UART to connect to.
    cmd_pipe: A multiprocessing.Connection object which represents the
      Interpreter side of the command pipe.  This must be a bidirectional pipe.
      Commands and responses will utilize this pipe.
    dbg_pipe: A multiprocessing.Connection object which represents the
      Interpreter side of the debug pipe. This must be a bidirectional pipe with
      write capabilities.  EC debug output will utilize this pipe.
    cmd_retries: An integer representing the number of attempts the console
      should retry commands if it receives an error.
    log_level: An integer representing the numeric value of the log level.
  """

  def __init__(self, ec_uart_pty, cmd_pipe, dbg_pipe, log_level=logging.INFO):
    """Intializes an Interpreter object with the provided args.

    Args:
      ec_uart_pty: A string representing the EC UART to connect to.
      cmd_pipe: A multiprocessing.Connection object which represents the
        Interpreter side of the command pipe.  This must be a bidirectional
        pipe.  Commands and responses will utilize this pipe.
      dbg_pipe: A multiprocessing.Connection object which represents the
        Interpreter side of the debug pipe. This must be a bidirectional pipe
        with write capabilities.  EC debug output will utilize this pipe.
      cmd_retries: An integer representing the number of attempts the console
        should retry commands if it receives an error.
      log_level: An optional integer representing the numeric value of the log
        level.  By default, the log level will be INFO.
    """
    self.ec_uart_pty = open(ec_uart_pty, 'a+')
    self.cmd_pipe = cmd_pipe
    self.dbg_pipe = dbg_pipe
    self.cmd_retries = COMMAND_RETRIES
    self.log_level = log_level

  def SendCmdToEC(self, cmd):
    """Send a console command to the EC UART.

    We send the command once and check the response to see if the EC encountered
    an error when receiving the command.  An error condition is reported to the
    console by a string with at least one '&' and 'E'.  The full string is
    '&&EE'.  If an error is encountered, the console will retry up to the amount
    configured.

    Args:
      cmd: A string which contains the command to be sent.
    """
    cmd_in_progress = False
    retries = self.cmd_retries
    while retries > 0:
      w = select.select([], [self.ec_uart_pty], [], EC_POLL_TIMEOUT)[1]
      r = select.select([self.ec_uart_pty], [], [], EC_POLL_TIMEOUT)[0]
      if r and cmd_in_progress:
        # Check the response.
        response = os.read(r[0].fileno(), 4)
        logging.debug('Checking response...')
        got_err = response.find('&E', 0, 4)
        if got_err == -1:
          # No error received.  Dump the line and return.
          logging.debug('EC Response didn\'t contain an error.')
          logging.debug('EC->%s', response)
          # Send the line to the user.
          self.cmd_pipe.send(response)
          return
        else:
          logging.warning('EC responded with error.  Retrying...')
          logging.warning('response: %s', response)
          retries -= 1
          cmd_in_progress = False
      if w and not cmd_in_progress:
        # TODO(aaboagye): Handle "oneshot" commands properly so we don't retry.
        logging.debug('Sending command...')
        w[0].write(cmd + '\n')
        w[0].flush()
        cmd_in_progress = True
        continue

  def PackCommand(self, cmd):
    r"""Packs a command for use with error checking.

    For error checking, we pack console commands in a particular format.  The
    format is as follows:

      &&[x][x][x][x]&{cmd}\n\n
      ^ ^    ^^    ^^  ^  ^-- 2 newlines.
      | |    ||    ||  |-- the raw console command.
      | |    ||    ||-- 1 ampersand.
      | |    ||____|--- 2 hex digits representing the CRC8 of cmd.
      | |____|-- 2 hex digits reprsenting the length of cmd.
      |-- 2 ampersands

    Args:
      cmd: A pre-packed string which contains the raw command.

    Returns:
      packed_cmd: A string which contains the packed command.
    """
    # The command format is as follows.
    # &&[x][x][x][x]&{cmd}\n\n
    packed_cmd = '&&'
    # The first pair of hex digits are the length of the command.
    packed_cmd += '%02x' % len(cmd)
    # Then the CRC8 of cmd.
    packed_cmd += '%02x' % Crc8(cmd)
    packed_cmd += '&'
    # Now, the raw command followed by 2 newlines.
    packed_cmd += cmd + 2*'\n'
    return packed_cmd

# TODO(aaboagye): Needs to be rewritten entirely to use pipes instead.
  def ProcessCommand(self, command):
    """Captures the input determines what actions to take."""
    command = command.strip()
    # There's nothing to do if the command is empty.
    if len(command) == 0:
      return

    # All other commands need to be packed first before they go to the EC.
    packed_cmd = self.PackCommand(command)
    logging.debug('packed cmd: ' + packed_cmd)
    self.SendCmdToEC(packed_cmd)
    # TODO(aaboagye): Make a dict of commands and keys and eventually, handle
    # partial matching based on unique prefixes.

def Crc8(data):
  """Calculates the CRC8 of data.

  The generator polynomial used is: x^8 + x^2 + x + 1.
  This is the same implementation that is used in the EC.

  Args:
    data: A string of data that we wish to calculate the CRC8 on.

  Returns:
    crc >> 8: An integer representing the CRC8 value.
  """
  crc = 0
  for byte in data:
    crc ^= (ord(byte) << 8)
    for _ in range(8):
      if crc & 0x8000:
        crc ^= (0x1070 << 3)
      crc <<= 1
  return crc >> 8

def StartLoop(interp):
  """Starts an infinite loop of servicing the user and the EC.

  It checks to see if there are any commands to process, processing them if any,
  and forwards EC output to the user.

  Args:
    interp: An Interpreter object that has been properly initialised.
  """
  # Check to see if there's anything to read.
  while True:
    # Check to if we have received any commands.
    cmd_pipe_data_available = interp.cmd_pipe.poll(PIPE_POLL_TIMEOUT)
    # Check to see if the EC has sent us any data.
    ec_ready = select.select([interp.ec_uart_pty], [], [], EC_POLL_TIMEOUT)[0]
    if ec_ready:
      logging.debug('EC has data.')
      # Read what the EC sent.
      data = os.read(interp.ec_uart_pty.fileno(), EC_MAX_READ)
      logging.debug('got: \'%s\'' % data)
      # For now, just forward everything the EC sends us.
      logging.debug('Forwarding to user...')
      interp.dbg_pipe.send(data)

    if cmd_pipe_data_available:
      logging.debug('Command data available.  Begin processing.')
      data = interp.cmd_pipe.recv()
      # Process the command.
      interp.ProcessCommand(data)

    # Take a nap.
    time.sleep(0.001)
