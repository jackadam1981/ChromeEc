#!/usr/bin/env python3

# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script for simulating keyboard presses."""

import argparse
import os
import sys
import termios
import time
import tty
import xmlrpc.client


KEYMAP = {
  'l_meta':  (0, 1),
  'f1':      (0, 2),
  'b':       (0, 3),
  'f10':     (0, 4),
  'n':       (0, 6),
  '=':       (0, 8),
  'r_alt':   (0, 10),

  'esc':     (1, 1),
  chr(0x1b): (1, 1),

  'f4':      (1, 2),
  'g':       (1, 3),
  'f7':      (1, 4),
  'h':       (1, 6),
  '\'':      (1, 8),
  'f9':      (1, 9),

  'bkspace': (1, 11),
  chr(0x7f): (1, 11),
  chr(0x08): (1, 11),

  'l_ctrl':  (2, 0),

  'tab':     (2, 1),
  chr(0x09): (2, 1),

  'f3':      (2, 2),
  't':       (2, 3),
  'f6':      (2, 4),
  ']':       (2, 5),
  'y':       (2, 6),
  '102nd':   (2, 7),
  '[':       (2, 8),
  'f8':      (2, 9),

  '`':       (3, 1),
  'f2':      (3, 2),
  '5':       (3, 3),
  'f5':      (3, 4),
  '6':       (3, 6),
  '-':       (3, 8),
  '\\':      (3, 11),

  'r_ctrl':  (4, 0),
  'a':       (4, 1),
  'd':       (4, 2),
  'f':       (4, 3),
  's':       (4, 4),
  'k':       (4, 5),
  'j':       (4, 6),
  ';':       (4, 8),
  'l':       (4, 9),
  # '\\':    (4, 10), UK keyboard key

  'enter':   (4, 11),
  '\r':      (4, 11),
  '\n':      (4, 11),

  'z':       (5, 1),
  'c':       (5, 2),
  'v':       (5, 3),
  'x':       (5, 4),
  ',':       (5, 5),
  'm':       (5, 6),
  'l_shift': (5, 7),
  '/':       (5, 8),
  '.':       (5, 9),
  ' ':       (5, 11),

  '1':       (6, 1),
  '3':       (6, 2),
  '4':       (6, 3),
  '2':       (6, 4),
  '8':       (6, 5),
  '7':       (6, 6),
  '0':       (6, 8),
  '9':       (6, 9),
  'l_alt':   (6, 10),
  'down':    (6, 11),
  'right':   (6, 12),

  'q':       (7, 1),
  'e':       (7, 2),
  'r':       (7, 3),
  'w':       (7, 4),
  'i':       (7, 5),
  'u':       (7, 6),
  'r_shift': (7, 7),
  'p':       (7, 8),
  'o':       (7, 9),
  'up':      (7, 11),
  'left':    (7, 12),
}


# Map sequences of letters to stuff that needs no conversions
SPECIAL_SEQ_MAP = {
  '\x1b[A':   '<up!>',
  '\x1b[B':   '<down!>',
  '\x1b[C':   '<right!>',
  '\x1b[D':   '<left!>',

  '\x1b[Z':   '<l_shift><tab!></l_shift>', # Shift tab

  '\x1b[5':   '<l_alt><up!></l_alt>',      # Page up
  '\x1b[6':   '<l_alt><down!></l_alt>',    # Page down

  '\x1bOP':   '<f1!>',
  '\x1b[11~': '<f1!>',
  '\x1bOQ':   '<f2!>',
  '\x1b[12~': '<f2!>',
  '\x1bOR':   '<f3!>',
  '\x1b[13~': '<f3!>',
  '\x1bOS':   '<f4!>',
  '\x1b[14~': '<f4!>',
  '\x1b[15~': '<f5!>',
  '\x1b[17~': '<f6!>',
  '\x1b[18~': '<f7!>',
  '\x1b[19~': '<f8!>',
  '\x1b[20~': '<f9!>',
  '\x1b[21~': '<f10!>',

  '&amp;':    '<l_shift>7</l_shift>',
  '&gt;':     '<l_shift>,</l_shift>',
  '&lt;':     '<l_shift>.</l_shift>',
}


# Map a single letter to a sequence of inputs (as if the user typed these
# things).  There must be no loops here.  That is: the result must always get
# processed before we make it back to conversions...
CONVERSIONS = {
  '~': '<l_shift>`</l_shift>',
  '!': '<l_shift>1</l_shift>',
  '@': '<l_shift>2</l_shift>',
  '#': '<l_shift>3</l_shift>',
  '$': '<l_shift>4</l_shift>',
  '%': '<l_shift>5</l_shift>',
  '^': '<l_shift>6</l_shift>',
  '&': '<l_shift>7</l_shift>',
  '*': '<l_shift>8</l_shift>',
  '(': '<l_shift>9</l_shift>',
  ')': '<l_shift>0</l_shift>',
  '_': '<l_shift>-</l_shift>',
  '+': '<l_shift>=</l_shift>',

  '{': '<l_shift>[</l_shift>',
  '}': '<l_shift>]</l_shift>',
  '|': '<l_shift>\\</l_shift>',

  ':': '<l_shift>;</l_shift>',
  '"': "<l_shift>'</l_shift>",

  '<': '<l_shift>,</l_shift>',
  '>': '<l_shift>.</l_shift>',
  '?': '<l_shift>/</l_shift>',
}
for c in range(ord('a'), ord('z')+1):
  CONVERSIONS[chr(c).upper()] = '<l_shift>%s</l_shift>' % chr(c)
  CONVERSIONS[chr(c - 96)] = '<l_ctrl>%s</l_ctrl>' % chr(c)


class Processor:
  """This class encapsulates the state needed to process incoming
  keypresses in interactive mode."""
  def __init__(self, servo):
    """Sets up a Processor.

    Args:
      servo: xmlrpc server proxy for servod
    """
    self._servo = servo
    self._buffer = ''


  def _convert_input(self, new_chars):
    """Process incoming characters to see if there are any usable
    kbpress commands we can build out of them.

    Args:
      new_chars: string of characters or tags to process

    Returns:
      a list of kbpress commands to send to the EC console
    """
    self._buffer += new_chars

    to_return = []
    while self._buffer:
      # Handle <press> and </release>
      if self._buffer.startswith('<'):
        # If we've got a starting char but no ending, we're done for now...
        if '>' not in self._buffer:
          break

        to_lookup, _, self._buffer = self._buffer.partition('>')
        to_lookup = to_lookup[1:].lower()

        sys.stdout.write('<%s>\r\n' % to_lookup)
        if to_lookup[0] == '/':
          # If it starts with a /, it's a release...
          to_lookup = to_lookup[1:]
          val = 0
        elif to_lookup[-1] == '!':
          # If it ends with a ! it's a click
          to_lookup = to_lookup[:-1]
          val = -1
        else:
          val = 1

        if to_lookup == 'quit':
          sys.exit(0)

        if to_lookup not in KEYMAP:
          sys.stderr.write('Unknown key: %s\r\n' % to_lookup)
          continue

        row, col = KEYMAP[to_lookup]
        if val == -1:
          to_return.append('kbpress %d %d' % (col, row))
        else:
          to_return.append('kbpress %d %d %d' % (col, row, val))
        continue

      found = False

      # Exact match on special
      for seq, key in SPECIAL_SEQ_MAP.items():
        if self._buffer.startswith(seq):
          self._buffer = key + self._buffer[len(seq):]
          found = True
          break
      if found:
        continue

      # Possible matches of special. If we have this we're done for now...
      for seq in SPECIAL_SEQ_MAP.keys():
        if seq.startswith(self._buffer):
          found = True
          break
      if found:
        break

      # Process one character...
      c = self._buffer[0]
      self._buffer = self._buffer[1:]
      if c in KEYMAP:
        row, col = KEYMAP[c]
        to_return.append('kbpress %d %d' % (col, row))
        sys.stdout.write('%s\r\n' % c)
        continue

      if c in CONVERSIONS:
        self._buffer = CONVERSIONS[c] + self._buffer
        continue

      sys.stderr.write("Can't handle character %#04x\r\n" % ord(c))

    sys.stdout.flush()

    return to_return


  def process_data(self, data):
    """Processes keystrokes and sends the EC any kbpress commands
    it can make from them.

    Args:
      data:  string of characters or tags to process
    """
    to_press = self._convert_input(data)

    if not to_press:
      return

    self._servo.set('ec_uart_multicmd', ';'.join(to_press))

def main(port, keys):
  servo = xmlrpc.client.ServerProxy('http://localhost:%s' % port)
  processor = Processor(servo)

  if keys:
    processor.process_data(keys)
    return

  # keys was None, so we're in interactive mode
  stdin_fd = sys.stdin.fileno()
  old_settings = termios.tcgetattr(stdin_fd)
  tty.setraw(stdin_fd)
  try:
    while True:
      c = os.read(stdin_fd, 1).decode('utf-8')
      processor.process_data(c)
  finally:
    termios.tcsetattr(stdin_fd, termios.TCSADRAIN, old_settings)

if __name__ == '__main__':
  parser = argparse.ArgumentParser(description='Use servo to type on the DUT')
  parser.add_argument('-p', '--port', type=int, default=9999,
                      help='port servod is running on (default: 9999)')
  parser.add_argument('keys', type=str, nargs='?',
                      help='keys to type (leave out for interactive mode)')
  args = parser.parse_args()
  main(args.port, args.keys)
