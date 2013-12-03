#!/usr/bin/env python

# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script for simulating keyboard presses."""

import os
import select
import StringIO
import sys
import termios
import time
import tty

def DataAvail(fd):
  """Return True if there's data ready for reading on the file descriptor."""
  avail, _, _ = select.select([fd], [], [], 0)
  return fd in avail

def Clear(fd):
  """Throw away any existing data on the file descriptor."""
  while DataAvail(fd):
    os.read(fd, 1)

def ReadTill(fd, till_ch):
  """Read until the given character arrives; return all data except till_ch."""
  sio = StringIO.StringIO()
  while True:
    ch = os.read(fd, 1)
    if ch == till_ch:
      break
    sio.write(ch)

  return sio.getvalue()

# for line in s.splitlines(): h, k = line.split(',', 1); h = int(h, 0); print "{ %-10s (%d, %d) }," % ("'%s':" % k.strip()[2:].lower(), h >> 24, (h >> 16) & 15)
KEYMAP = {
  'l_meta': (0, 1),
  'f1':     (0, 2),
  'b':      (0, 3),
  'f10':    (0, 4),
  'n':      (0, 6),
  '=':      (0, 8),
  'r_alt':  (0, 10),

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
  # '\\':      (4, 10), UK keyboard key

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
  '\x1b[A': 'up',
  '\x1b[B': 'down',
  '\x1b[C': 'right',
  '\x1b[D': 'left',

  '&amp;':  "<l_shift>7</l_shift>",
  '&gt;':   "<l_shift>,</l_shift>",
  '&lt;':   "<l_shift>.</l_shift>",
}

# Map a single letter to a sequence of inputs (as if the user typed these
# things).  There must be no loops here.  That is: the result must always get
# processed before we make it back to conversions...
CONVERSIONS = {
  "~": "<l_shift>`</l_shift>",
  "!": "<l_shift>1</l_shift>",
  "@": "<l_shift>2</l_shift>",
  "#": "<l_shift>3</l_shift>",
  "$": "<l_shift>4</l_shift>",
  "%": "<l_shift>5</l_shift>",
  "^": "<l_shift>6</l_shift>",
  "&": "<l_shift>7</l_shift>",
  "*": "<l_shift>8</l_shift>",
  "(": "<l_shift>9</l_shift>",
  ")": "<l_shift>0</l_shift>",
  "_": "<l_shift>-</l_shift>",
  "+": "<l_shift>=</l_shift>",

  "{": "<l_shift>=[/l_shift>",
  "}": "<l_shift>=]/l_shift>",
  "|": "<l_shift>=\\/l_shift>",

  ":":  "<l_shift>;</l_shift>",
  "\"": "<l_shift>'</l_shift>",

  "<": "<l_shift>,</l_shift>",
  ">": "<l_shift>.</l_shift>",
  "?": "<l_shift>/</l_shift>",
}
CONVERSIONS.update(
  dict((chr(c).upper(), "<l_shift>%s</l_shift>" % chr(c))
  for c in xrange(ord('a'), ord('z')+1))
)
CONVERSIONS.update(
  dict((chr(c - 96), "<l_ctrl>%s</l_ctrl>" % chr(c))
  for c in xrange(ord('a'), ord('z')+1))
)

buffer = ""
def convert_input(new_chars):
  global buffer

  buffer += new_chars

  to_return = []
  while buffer:
    # Handle <press> and </release>
    if buffer.startswith('<'):
      # If we've got a starting char but no ending, we're done for now...
      if '>' not in buffer:
        break

      to_lookup, _, buffer = buffer.partition('>')
      to_lookup = to_lookup[1:].lower()

      # If it starts with a /, it's a release...
      if to_lookup[0] == '/':
        to_lookup = to_lookup[1:]
        val = 0
        print >>sys.stdout, "</%s>" % to_lookup
      else:
        val = 1
        print >>sys.stdout, "<%s>" % to_lookup

      if to_lookup == "quit":
        sys.exit(0)

      if to_lookup not in KEYMAP:
        print >>sys.stderr, "Unknown key: %s" % to_lookup
        continue

      row, col = KEYMAP[to_lookup]
      to_return.append((row, col, val))
      continue

    found = False

    # Exact match on special
    for seq, key in SPECIAL_SEQ_MAP.iteritems():
      if buffer.startswith(seq):
        buffer = key + buffer[len(seq):]
        found = True
        break
    if found:
      continue

    # Possible matches of special.  If we have this we're done for now...
    for seq in SPECIAL_SEQ_MAP.iterkeys():
      if seq.startswith(buffer):
        found = True
        break
    if found:
      break

    # Process one character...
    c = buffer[0]
    buffer = buffer[1:]
    if c in KEYMAP:
      row, col = KEYMAP[c]
      to_return.extend([(row, col, 1), (row, col, 0)])
      print >>sys.stdout, c
      continue

    if c in CONVERSIONS:
      buffer = CONVERSIONS[c] + buffer
      continue

    print >>sys.stderr, "Can't handle character %#04x" % ord(c)

  sys.stdout.flush()

  return to_return

def process_data(fd, data):
  to_press = convert_input(data)
  Clear(fd)
  for (row, col, val) in to_press:
    os.write(fd, "kbpress %d %d %d\n" % (col, row, val))
    ReadTill(fd, ">")

def get_ch():
  stdin_fd = sys.stdin.fileno()
  old_settings = termios.tcgetattr(stdin_fd)
  try:
    tty.setraw(stdin_fd)
    return os.read(stdin_fd, 1)
  finally:
    termios.tcsetattr(stdin_fd, termios.TCSADRAIN, old_settings)

def main(port, to_send=""):
  print "Opening serial port: %s" % port

  fd = os.open(port, os.O_RDWR)

  if to_send:
    process_data(fd, to_send)
    return

  while True:
    process_data(fd, get_ch())

if __name__ == '__main__':
  # TODO: option parse!
  main(*sys.argv[1:])
