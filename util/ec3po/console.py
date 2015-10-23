#!/usr/bin/python2
# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""EC-3PO Console Interface

console provides the console interface between the user and the EC.  It handles
the presentation of the EC console including editing methods as well as
automatic command retrying if the EC drops a character in a command.
"""
from __future__ import print_function
import argparse
from chromite.lib import cros_logging as logging
import os
import pty
import select
import time

DEBUG = True
PROMPT = '> '
CONSOLE_INPUT_LINE_SIZE = 80 # Taken from the CONFIG_* with the same name.
COMMAND_RETRIES = 3


class EscState(object):
  """Class which contains an enumeration for states of ESC sequences."""
  ESC_START = 1
  ESC_BRACKET = 2
  ESC_BRACKET_1 = 3
  ESC_BRACKET_3 = 4
  ESC_BRACKET_8 = 5


class ControlKey(object):
  """Class which contains codes for various control keys."""
  BACKSPACE = 0x08
  CTRL_A = 0x01
  CTRL_B = 0x02
  CTRL_D = 0x04
  CTRL_E = 0x05
  CTRL_F = 0x06
  CTRL_K = 0x0b
  CTRL_N = 0xe
  CTRL_P = 0x10
  CARRIAGE_RETURN = 0x0d
  ESC = 0x1b


class MoveCursorError(Exception):
  """Exception class for errors when moving the cursor."""


class Console(object):
  """Class which provides the console interface between the EC and the user.

  This class essentially represents the console interface between the user and
  the EC.  It handles all of the console editing behaviour

  Attributes:
    master_pty: File descriptor to the master side of the PTY.  Used for driving
      output to the user.
    slave_pty: A File object to the slave side of the PTY.  This ends up being
      the EC UART.
    user_pty: A string representing the PTY name of the served console.
    input_buffer: A string representing the current input command.
    input_buffer_pos: An integer representing the current position in the buffer
      to insert a char.
    partial_cmd: A string representing the command entered on a line before
      pressing the up arrow keys.
    esc_state: An integer represeting the current state within an escape
      sequence.
    line_limit: An integer representing the maximum number of characters on a
      line.
    history: A list of strings containing the past entered console commands.
    history_pos: An integer representing the current history buffer position.
      This index is used to show previous commands.
    prompt: A string representing the console prompt displayed to the user.
    cmd_retries: An integer representing the number of attempts the console
      should retry commands if it receives an error.
  """

  def __init__(self, master_pty, slave_pty, user_pty=''):
    """Initalises the Console object with the arguments.

    Args:
    master_pty: File descriptor to the master side of the PTY.  Used for driving
      output to the user.
    slave_pty: File descriptor to the slave side of the PTY.  This ends up being
      the EC UART.
    user_pty: An optional string representing the PTY name of the served
      console.
    """
    self.master_pty = master_pty
    self.slave_pty = slave_pty
    self.user_pty = user_pty
    self.input_buffer = ''
    self.input_buffer_pos = 0
    self.partial_cmd = ''
    self.esc_state = 0
    self.line_limit = CONSOLE_INPUT_LINE_SIZE
    self.history = []
    self.history_pos = 0
    self.prompt = PROMPT
    self.cmd_retries = COMMAND_RETRIES

  def __str__(self):
    """Show internal state of Console object as a string."""
    string = 'master_pty: %s\n' % self.master_pty
    string += 'slave_pty: %s\n' % self.slave_pty
    string += 'user_pty: %s\n' % self.user_pty
    string += 'input_buffer: %s\n' % self.input_buffer
    string += 'input_buffer_pos: %d\n' % self.input_buffer_pos
    string += 'esc_state: %d\n' % self.esc_state
    string += 'line_limit: %d\n' % self.line_limit
    string += 'history: [\'' + '\', \''.join(self.history) + '\']\n'
    string += 'history_pos: %d\n' % self.history_pos
    string += 'prompt: \'%s\'\n' % self.prompt
    string += 'cmd_retries: %d\n' % self.cmd_retries
    string += 'partial_cmd: \'%s\'\n'% self.partial_cmd
    return string

  def PrintHistory(self):
    """Print the history of entered commands."""
    fd = self.master_pty
    # Make it pretty by figuring out how wide to pad the numbers.
    wide = (len(self.history) / 10) + 1
    for i in range(len(self.history)):
      line = ' %*d %s\r\n' % (wide, i, self.history[i])
      os.write(fd, line)

  def ShowPreviousCommand(self):
    """Shows the previous command from the history list."""
    # There's nothing to do if there's no history at all.
    if not self.history:
      logging.debug('No history to print.')
      return

    # Don't do anything if there's no more history to show.
    if self.history_pos == 0:
      logging.debug('No more history to show.')
      return

    logging.debug('current history position: %d.', self.history_pos)

    # Decrement the history buffer position.
    self.history_pos -= 1
    logging.debug('new history position.: %d', self.history_pos)

    # Save the text entered on the console if any.
    if self.history_pos == len(self.history)-1:
      logging.debug('saving partial_cmd: \'%s\'', self.input_buffer)
      self.partial_cmd = self.input_buffer

    # Backspace the line.
    for _ in range(self.input_buffer_pos):
      self.SendBackspace()

    # Print the last entry in the history buffer.
    logging.debug('printing previous entry %d - %s', self.history_pos,
                  self.history[self.history_pos])
    fd = self.master_pty
    prev_cmd = self.history[self.history_pos]
    os.write(fd, prev_cmd)
    # Update the input buffer.
    self.input_buffer = prev_cmd
    self.input_buffer_pos = len(prev_cmd)

  def ShowNextCommand(self):
    """Shows the next command from the history list."""
    # Don't do anything if there's no history at all.
    if not self.history:
      logging.debug('History buffer is empty.')
      return

    fd = self.master_pty

    logging.debug('current history position: %d', self.history_pos)
    # Increment the history position.
    self.history_pos += 1

    # Restore the partial cmd.
    if self.history_pos == len(self.history):
      logging.debug('Restoring partial command of \'%s\'', self.partial_cmd)
      # Backspace the line.
      for _ in range(self.input_buffer_pos):
        self.SendBackspace()
      # Print the partially entered command if any.
      os.write(fd, self.partial_cmd)
      self.input_buffer = self.partial_cmd
      self.input_buffer_pos = len(self.input_buffer)
      # Now that we've printed it, clear the partial cmd storage.
      self.partial_cmd = ''
      # Reset history position.
      self.history_pos = len(self.history)
      return

    logging.debug('new history position: %d', self.history_pos)
    if self.history_pos > len(self.history)-1:
      logging.debug('No more history to show.')
      self.history_pos -= 1
      logging.debug('Reset history position to %d', self.history_pos)
      return

    # Backspace the line.
    for _ in range(self.input_buffer_pos):
      self.SendBackspace()

    # Print the newer entry from the history buffer.
    logging.debug('printing next entry %d - %s', self.history_pos,
                  self.history[self.history_pos])
    next_cmd = self.history[self.history_pos]
    os.write(fd, next_cmd)
    # Update the input buffer.
    self.input_buffer = next_cmd
    self.input_buffer_pos = len(next_cmd)
    logging.debug('new history position: %d.', self.history_pos)

  def SliceOutChar(self):
    """Remove a char from the line and shift everything over 1 column."""
    fd = self.master_pty
    # Remove the character at the input_buffer_pos by slicing it out.
    self.input_buffer = self.input_buffer[0:self.input_buffer_pos] + \
                        self.input_buffer[self.input_buffer_pos+1:]
    # Write the rest of the line
    moved_col = os.write(fd, self.input_buffer[self.input_buffer_pos:])
    # Write a space to clear out the last char
    moved_col += os.write(fd, ' ')
    # Update the input buffer position.
    self.input_buffer_pos += moved_col
    # Reset the cursor
    self.MoveCursor('left', moved_col)

  def HandleEsc(self, byte):
    """HandleEsc processes escape sequences.

    Args:
      byte: An integer representing the current byte in the sequence.
    """
    # We shouldn't be handling an escape sequence if we haven't seen one.
    assert self.esc_state != 0

    if self.esc_state is EscState.ESC_START:
      logging.debug('ESC_START')
      if byte == ord('['):
        self.esc_state = EscState.ESC_BRACKET
        return

      else:
        logging.error('Unexpected sequence. %c' % byte)
        self.esc_state = 0

    elif self.esc_state is EscState.ESC_BRACKET:
      logging.debug('ESC_BRACKET')
      # Left Arrow key was pressed.
      if byte == ord('D'):
        logging.debug('Left arrow key pressed.')
        self.MoveCursor('left', 1)
        self.esc_state = 0 # Reset the state.
        return

      # Right Arrow key.
      elif byte == ord('C'):
        logging.debug('Right arrow key pressed.')
        self.MoveCursor('right', 1)
        self.esc_state = 0 # Reset the state.
        return

      # Up Arrow key.
      elif byte == ord('A'):
        logging.debug('Up arrow key pressed.')
        self.ShowPreviousCommand()
        # Reset the state.
        self.esc_state = 0 # Reset the state.
        return

      # Down Arrow key.
      elif byte == ord('B'):
        logging.debug('Down arrow key pressed.')
        self.ShowNextCommand()
        # Reset the state.
        self.esc_state = 0 # Reset the state.
        return

      # For some reason, minicom sends a 1 instead of 7. /shrug
      # TODO(aaboagye): Figure out why this happens.
      elif byte == ord('1') or byte == ord('7'):
        self.esc_state = EscState.ESC_BRACKET_1

      elif byte == ord('3'):
        self.esc_state = EscState.ESC_BRACKET_3

      elif byte == ord('8'):
        self.esc_state = EscState.ESC_BRACKET_8

      else:
        logging.error(r'Bad or unhandled escape sequence. got ^[%c\(%d)'
                      % (chr(byte), byte))
        self.esc_state = 0
        return

    elif self.esc_state is EscState.ESC_BRACKET_1:
      logging.debug('ESC_BRACKET_1')
      # HOME key.
      if byte == ord('~'):
        logging.debug('Home key pressed.')
        self.MoveCursor('left', self.input_buffer_pos)
        self.esc_state = 0 # Reset the state.
        logging.debug('ESC sequence complete.')
        return

    elif self.esc_state is EscState.ESC_BRACKET_3:
      logging.debug('ESC_BRACKET_3')
      # DEL key.
      if byte == ord('~'):
        logging.debug('Delete key pressed.')
        if self.input_buffer_pos != len(self.input_buffer):
          self.SliceOutChar()
        self.esc_state = 0 # Reset the state.

    elif self.esc_state is EscState.ESC_BRACKET_8:
      logging.debug('ESC_BRACKET_8')
      # END key.
      if byte == ord('~'):
        logging.debug('End key pressed.')
        self.MoveCursor('right',
                        len(self.input_buffer) - self.input_buffer_pos)
        self.esc_state = 0 # Reset the state.
        logging.debug('ESC sequence complete.')
        return

      else:
        logging.error('Unexpected sequence. %c' % byte)
        self.esc_state = 0

    else:
      logging.error('Unexpected sequence. %c' % byte)
      self.esc_state = 0

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
      w = select.select([], [self.slave_pty], [], 0.1)[1]
      r = select.select([self.slave_pty], [], [], 0.1)[0]
      if r and cmd_in_progress:
        # Check the response.
        response = os.read(r[0].fileno(), 4)
        logging.debug('Checking response...')
        got_err = response.find('&E', 0, 4)
        if got_err == -1:
          # No error received.  Dump the line and return.
          logging.debug('EC Response didn\'t contain an error.')
          logging.debug('EC->%s', response)
          os.write(self.master_pty, response)
          return
        else:
          logging.warning('EC responded with error.  Retrying...')
          logging.warning('response: %s', response)
      if w:
        # TODO(aaboagye): Handle "oneshot" commands properly so we don't retry.
        logging.debug('Sending command...')
        w[0].write(cmd + '\n')
        w[0].flush()
        cmd_in_progress = True
        retries -= 1
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

  def ProcessInput(self):
    """Captures the input determines what actions to take."""
    # There's nothing to do if the input buffer is empty.
    if len(self.input_buffer) == 0:
      return

    # Don't store 2 consecutive identical commands in the history.
    if len(self.history) > 0:
      if self.history[len(self.history)-1] != self.input_buffer:
        # Add the input to the history buffer.
        self.history.append(self.input_buffer)
    else:
      # Add the input to the history buffer.
      self.history.append(self.input_buffer)

    # Split the command up by spaces.
    line = self.input_buffer.lower().split(' ')
    logging.debug('cmd: %s' % (self.input_buffer))
    cmd = line[0]

    # The 'history' command is a special case that we handle locally.
    if cmd == 'history':
      self.PrintHistory()
      return

    # All other command need to be packed first before they go to the EC.
    packed_cmd = self.PackCommand(self.input_buffer)
    logging.debug('packed cmd: ' + packed_cmd)
    self.SendCmdToEC(packed_cmd)
    # TODO(aaboagye): Make a dict of commands and keys.

  def HandleChar(self, byte):
    """HandleChar does a certain action when it receives a character.

    Args:
      byte: An integer representing the character received from the user.
    """
    # Keep handling the ESC sequence if we're in the middle of it.
    if self.esc_state != 0:
      self.HandleEsc(byte)
      return

    # When we're at the end of the line, we should only allow going backwards,
    # backspace, carriage return, up, or down.  The arrow keys are escape
    # sequences, so we let the escape...escape.
    if self.input_buffer_pos >= self.line_limit and \
       byte not in [ControlKey.CTRL_B, ControlKey.ESC, ControlKey.BACKSPACE,
                    ControlKey.CTRL_A, ControlKey.CARRIAGE_RETURN,
                    ControlKey.CTRL_P, ControlKey.CTRL_N]:
      return

    # If the input buffer is full we can't accept new chars.
    if len(self.input_buffer) >= self.line_limit:
      buffer_full = True
    else:
      buffer_full = False

    fd = self.master_pty

    # Carriage_Return/Enter
    if byte == ControlKey.CARRIAGE_RETURN:
      logging.debug('Enter key pressed.')
      # Put a carriage return/newline and the print the prompt.
      os.write(fd, '\r\n')

      # TODO(aaboagye): When we control the printing of all output, print the
      # prompt AFTER printing all the output.  We can't do it yet because we
      # don't know how much is coming from the EC.

      #Print the prompt.
      os.write(fd, self.prompt)
      # Process the input.
      self.ProcessInput()
      # Now, clear the buffer.
      self.input_buffer = ""
      self.input_buffer_pos = 0
      # Reset history buffer pos.
      self.history_pos = len(self.history)
      # Clear partial command.
      self.partial_cmd = ''

    # Backspace
    elif byte == ControlKey.BACKSPACE:
      logging.debug('Backspace pressed.')
      if self.input_buffer_pos > 0:
        # Move left 1 column.
        self.MoveCursor('left', 1)
        # Remove the character at the input_buffer_pos by slicing it out.
        self.SliceOutChar()

      logging.debug('input_buffer_pos: %d' % (self.input_buffer_pos))

    # Ctrl+A. Move cursor to beginning of the line
    elif byte == ControlKey.CTRL_A:
      logging.debug('Control+A pressed.')
      self.MoveCursor('left', self.input_buffer_pos)

    # Ctrl+B. Move cursor left 1 column.
    elif byte == ControlKey.CTRL_B:
      logging.debug('Control+B pressed.')
      self.MoveCursor('left', 1)

    # Ctrl+D. Delete a character.
    elif byte == ControlKey.CTRL_D:
      logging.debug('Control+D pressed.')
      if self.input_buffer_pos != len(self.input_buffer):
        # Remove the character by slicing it out.
        self.SliceOutChar()

    # Ctrl+E. Move cursor to end of the line.
    elif byte == ControlKey.CTRL_E:
      logging.debug('Control+E pressed.')
      self.MoveCursor('right',
                      len(self.input_buffer) - self.input_buffer_pos)

    # Ctrl+F. Move cursor right 1 column.
    elif byte == ControlKey.CTRL_F:
      logging.debug('Control+F pressed.')
      self.MoveCursor('right', 1)

    # Ctrl+K. Kill line.
    elif byte == ControlKey.CTRL_K:
      logging.debug('Control+K pressed.')
      self.KillLine()

    # Ctrl+N. Next line.
    elif byte == ControlKey.CTRL_N:
      logging.debug('Control+N pressed.')
      self.ShowNextCommand()

    # Ctrl+P. Previous line.
    elif byte == ControlKey.CTRL_P:
      logging.debug('Control+P pressed.')
      self.ShowPreviousCommand()

    # ESC sequence
    elif byte == ControlKey.ESC:
      # Starting an ESC sequence
      self.esc_state = EscState.ESC_START

    # Only print printable chars.
    elif byte >= ord(' ') and byte <= ord('~'):
      # Drop the character if we're full.
      if buffer_full:
        logging.debug('Dropped char: %c(%d)', byte, byte)
        return
      # Print the character.
      os.write(fd, chr(byte))
      # Print the rest of the line (if any).
      extra_bytes_written = os.write(fd,
                                     self.input_buffer[self.input_buffer_pos:])

      # Recreate the input buffer.
      self.input_buffer = (self.input_buffer[0:self.input_buffer_pos] +
                           ('%c' % byte) +
                           self.input_buffer[self.input_buffer_pos:])
      # Update the input buffer position.
      self.input_buffer_pos += 1
      self.input_buffer_pos += extra_bytes_written

      # Reset the cursor if we wrote any extra bytes.
      if extra_bytes_written != 0:
        self.MoveCursor('left', extra_bytes_written)

      logging.debug('input_buffer_pos: %d' % (self.input_buffer_pos))

  def MoveCursor(self, direction, count):
    """MoveCursor moves the cursor left or right by count columns.

    Args:
      direction: A string that should be either 'left' or 'right' representing
        the direction to move the cursor on the console.
      count: An integer representing how many columns the cursor should be
        moved.

    Raises:
      ValueError: If the direction is not equal to 'left' or 'right'.
    """
    # If there's nothing to move, we're done.
    if count == 0:
      return
    fd = self.master_pty
    seq = '\033[' + str(count)
    if direction is 'left':
      # Bind the movement.
      if count > self.input_buffer_pos:
        count = self.input_buffer_pos
      seq += 'D'
      logging.debug('move cursor left %d', count)
      self.input_buffer_pos -= count

    elif direction is 'right':
      # Bind the movement.
      if (count + self.input_buffer_pos) > len(self.input_buffer):
        count = 0
      seq += 'C'
      logging.debug('move cursor right %d', count)
      self.input_buffer_pos += count

    else:
      raise MoveCursorError(('The only valid directions are \'left\' and '
                             '\'right\''))

    logging.debug('input_buffer_pos: %d' % self.input_buffer_pos)
    # Move the cursor.
    if count != 0:
      os.write(fd, seq)

  def KillLine(self):
    """Kill the rest of the line based on the input buffer position."""
    # Killing the line is killing all the text to the right.
    diff = abs(len(self.input_buffer) - self.input_buffer_pos)
    logging.debug('diff: %d' % diff)
    if diff != 0:
      self.MoveCursor('right', diff)
      for _ in range(diff):
        self.SendBackspace()
      self.input_buffer_pos -= diff
      self.input_buffer = self.input_buffer[0:self.input_buffer_pos]

  def SendBackspace(self):
    """Backspace a character on the console."""
    fd = self.master_pty
    os.write(fd, '\033[1D')
    os.write(fd, ' ')
    os.write(fd, '\033[1D')


def StartLoop(console):
  """Starts the infinite loop of console processing.

  Args:
    console: A Console object that has been initialzed with the appropriate
      PTYs.
  """
  # Need to be able to read the console input and read it to the screen.  All
  # the while, reading console input and handling appropriately.
  logging.info('EC Console is being redirected from %s and served on %s.',
               console.slave_pty.name, console.user_pty)
  logging.debug(console)
  rlist = [console.master_pty, console.slave_pty]
  while rlist:
    # Check to see if any of the rlist members are ready for reading.
    ready = select.select(rlist, [], [], 0)[0]
    if ready:
      for f in ready:
        # Read from the master side of the PTY.
        if f is console.master_pty:
          logging.debug('Input from user')
          # Convert to bytes so we can look for non-printable chars.
          line = bytearray(os.read(console.master_pty, 100))
          for i in line:
            # Handle each character as it arrives.
            console.HandleChar(i)

        else: # It must be from the EC console.
          # For now, we just pass whatever the EC sends, forward.
          line = os.read(f.fileno(), 1024)
          logging.debug('EC->%s', line)
          # Write it to the user console.
          os.write(console.master_pty, line)
    # Microsleep for just a bit.
    time.sleep(0.001)

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

def main():
  """Kicks off the EC-3PO console intepreter.

  We open a PTY pair and connect to the EC UART PTY.  Then create a Console
  object and begin serving as the interpreter.
  """
  # Set up argument parser.
  parser = argparse.ArgumentParser(description='Start EC console interpreter.')
  # TODO(aaboagye): Eventually get this from servod.
  parser.add_argument('ec_uart_pty',
                      help=('The full PTY name that the EC UART'
                            ' is present on. eg: /dev/pts/12'))
  parser.add_argument('--log-level',
                      default='info',
                      help=('info, debug, warning, error, or critical'))

  # Parse arguments.
  args = parser.parse_args()

  # Can't do much without an EC to talk to.
  if not args.ec_uart_pty:
    parser.print_help()
    quit()

  ec_uart_pty = open(args.ec_uart_pty, 'a+')

  # Set logging level.
  args.log_level = args.log_level.lower()
  if args.log_level == 'info':
    level = logging.INFO
  elif args.log_level == 'debug':
    level = logging.DEBUG
  elif args.log_level == 'warning':
    level = logging.WARNING
  elif args.log_level == 'error':
    level = logging.ERROR
  elif args.log_level == 'critical':
    level = logging.CRITICAL
  else:
    print('Error: Invalid log level.')
    parser.print_help()
    quit()

  # Start logging with a timestamp and log level shown in each log entry.
  logging.basicConfig(level=level, format=('%(asctime)s - %(levelname)s'
                                           ' - %(message)s'))

  # Open a new pseudo-terminal pair
  (master_pty, slave_pty) = pty.openpty()
  # The slave_pty is what our user should connect to.
  console = Console(master_pty, ec_uart_pty, os.ttyname(slave_pty))
  StartLoop(console)

if __name__ == '__main__':
  main()
