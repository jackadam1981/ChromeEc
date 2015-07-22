#!/usr/bin/python2
# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Configuration Option Checker.

Script to ensure that all configuration options for the Chrome EC are defined
in config.h.
"""
from __future__ import print_function
import os
import re
import subprocess


class Line(object):
  """Class for each changed line in diff output.

  Attributes:
    line_num: The line number that this line appears in the file.
    string: The literal string of this line.
  """

  def __init__(self, line_num, string):
    """Inits Line with the line number and the actual string."""
    self.line_num = line_num
    self.string = string


class Hunk(object):
  """Class for a git diff hunk.

  Attributes:
    filename: The name of the file that this hunk belongs to.
    lines: A list of Lines that are a part of this hunk.
  """

  def __init__(self, filename, lines):
    """Inits Hunk with the filename and the list of lines of the hunk."""
    self.filename = filename
    self.lines = lines


# Master file which is supposed to include all CONFIG_xxxx descriptions.
CONFIG_FILE = 'include/config.h'

# Specific files which the checker should ignore.
WHITELIST = [CONFIG_FILE, 'util/config_option_check.py']

def obtain_current_config_options():
  """Obtains current config options from include/config.h."""
  config_options = []
  config_option_re = re.compile(r'^#(define|undef)\s+(CONFIG_[A-Z0-9_]+)')
  with open(CONFIG_FILE, 'r') as config_file:
    for line in config_file:
      result = config_option_re.search(line)
      if not result:
        continue
      word = result.groups()[1]
      if word not in config_options:
        config_options.append(word)
  return config_options

def print_missing_config_options(hunks, config_options):
  """Searches thru all the changes in hunks for missing options and prints them.

  TODO(aaboagye): Improve upon this to detect when files are being removed and a
  CONFIG_* option is no longer used elsewhere in the repo.
  """
  missing_config_option = False
  print_banner = True
  # Determine longest CONFIG_* length to be used for formatting.
  max_option_length = max(len(option) for option in config_options)
  config_option_re = re.compile(r'(CONFIG_[a-zA-Z0-9_]+)')
  c_style_ext = ('.c', '.h', '.inc', '.S')
  make_style_ext = ('.mk')

  # Check each hunk's line for a missing config option.
  for h in hunks:
    for l in h.lines:
      # Check for the existence of a CONFIG_* in the line.
      match = re.findall(config_option_re, l.string)
      if not match:
        continue

      # At this point, an option was found in the string.  However, we need to
      # verify that it is not within a comment.  Assume every option is a
      # violation until proven otherwise.

      options = dict()
      for opt in match:
        options[opt] = True

      # Different files have different comment syntax;  Handle appropriately.
      extension = os.path.splitext(h.filename)[1]
      if extension in c_style_ext:
        beg_comment_idx = l.string.find('/*')
        end_comment_idx = l.string.find('*/')
        for option in match:
          option_idx = l.string.find(option)
          if beg_comment_idx == -1:
            # Check to see if this line is from a multi-line comment.
            if l.string.lstrip()[0] == '*':
              # It _seems_ like it is, therefore ignore this instance.
              options[option] = False
          else:
            # Check to see if its actually inside the comment.
            if beg_comment_idx < option_idx < end_comment_idx:
              # The config option is in the comment.  Ignore it.
              options[option] = False
      elif extension in make_style_ext or 'Makefile' in h.filename:
        beg_comment_idx = l.string.find('#')
        for option in match:
          option_idx = l.string.find(option)
          # Ignore everything to the right of the hash.
          if beg_comment_idx < option_idx and beg_comment_idx != -1:
            # The option is within a comment.  Ignore it.
            options[option] = False

      # Check to see if the CONFIG_* option is in the config file and print the
      # violations.
      for option in match:
        if option not in config_options and options[option]:
          # Print the banner once.
          if print_banner:
            print('The following config options were found to be missing '
                  'from %s.\n'
                  'Please add new config options there along with '
                  'descriptions.\n\n' % CONFIG_FILE)
            print_banner = False
            missing_config_option = True
          # Print the misssing config option.
          print('> %-*s %s:%s' % (max_option_length, option,
                                  h.filename,
                                  l.line_num))
  return missing_config_option

def get_hunks():
  """Gets the hunks of the most recent commit."""
  # Get the diff output.
  cmd = 'git diff --cached -GCONFIG_* --no-prefix --no-ext-diff HEAD~1'
  cmd = cmd.split(' ')
  diff = subprocess.check_output(cmd).split('\n')
  hunks = []

  # Nothing to do if the list is empty.
  if not diff[0]:
    return hunks

  # Regex patterns
  filename_re = re.compile(r'[-|+]{3} (.*)')
  hunk_line_num_re = re.compile(r'^@@ -[0-9]+,[0-9]+ \+([0-9]+),[0-9]+ @@.*')
  line_re = re.compile(r'^([+|-])(.*)')

  # Begin processing the diff output to create hunks.
  i = 0
  while i < range(len(diff)):
    line = diff[i]
    # TODO: Process the case when removing a file completely to catch unused
    # CONFIG_* options.
    if 'deleted' in line:
      return hunks
    # Search for a file name.  The first match should be '--- <file>'.
    result = re.search(filename_re, line)
    if not result:
      i += 1
      continue
    # Found a file name.
    still_in_file = True
    # Check to see if it's /dev/null, if it is, we're adding a file.
    if result.groups(1) == '/dev/null':
      # Need to check the next line for the filename.
      i += 1
      continue

    # Obtain the file name.
    filename = result.groups(1)[0]
    # Skip the file if it is whitelisted.
    if filename in WHITELIST:
      i += 1
      continue

    try:
      i += 1
      line = diff[i]
    except IndexError:
      # We've reached the end of diff.  Return what we have.
      return hunks

    while still_in_file:
      # Search for a hunk.  Each hunk starts with a line describing the line
      # numbers in the file.
      result = re.search(hunk_line_num_re, line)
      while not result:
        try:
          i += 1
          line = diff[i]
        except IndexError:
          # We've reached the end of diff.  Return what we have.
          return hunks
        result = re.search(hunk_line_num_re, line)

      # At this point, we've found a hunk.
      still_in_hunk = True
      # Extract the line number offset and start counting.
      line_num = int(result.groups(1)[0])
      try:
        i += 1
        line = diff[i]
      except IndexError:
        # We've reached the end of diff.  Return what we have.
        return hunks

      change = re.search(line_re, line)
      hunk_lines = []
      while still_in_hunk:
        if change:
          # We've found a changed line.  Consider it iff it's an addition.
          if change.groups(1)[0] == '+':
            hunk_lines.append(Line(line_num, change.groups(2)[1]))
            line_num += 1
          # Deletions(-) don't count towards line number count.
        else:
          # An unchanged line.
          line_num += 1

        try:
          i += 1
          line = diff[i]
        except IndexError:
          # We've reached the end of the diff.  Add the current hunk and return
          # what we have.
          hunks.append(Hunk(filename, hunk_lines))
          return hunks

        new_hunk = re.search(hunk_line_num_re, line)
        new_file = re.search(filename_re, line)
        change = re.search(line_re, line)

        if new_hunk or new_file:
          # If we've encountered another hunk or a new file, let's wrap this
          # hunk up.
          still_in_hunk = False
          hunks.append(Hunk(filename, hunk_lines))
        if new_file:
          still_in_file = False
  return hunks

def main():
  """Searches through committed changes for missing config options.

  Checks through committed changes for CONFIG_* options.  Then checks to make
  sure that all CONFIG_* options used are defined in include/config.h.  Finally,
  reports any missing config options.
  """
  # Obtain the hunks of the commit to search through.
  hunks = get_hunks()
  # Obtain config options from include/config.h.
  config_options = obtain_current_config_options()
  # Find any missing config options from the hunks and print them.
  missing_opts = print_missing_config_options(hunks, config_options)

  if missing_opts:
    print('\nIt may also be possible that you have a typo.')
    os.sys.exit(1)

if __name__ == '__main__':
  main()
