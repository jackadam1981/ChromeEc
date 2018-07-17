# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Calculates statistics for lists of data and pretty print them."""

from __future__ import print_function
import collections
import json
import logging
import math
import numpy
import os

STATS_PREFIX = '@@'

LONG_UNIT = {
    'mW': 'milliwatt',
    'uW': 'microwatt',
    'mV': 'millivolt',
    'uA': 'microamp',
    'uV': 'microvolt'
}

class StatsManagerError(Exception):
  """Errors in StatsManager class"""
  pass

class StatsManager(object):
  """Calculates statistics for several lists of data(float).i

  Example usage:

    >>> stats = StatsManager(order=['sample_msecs'])
    >>> stats.AddSample(TIME_KEY, 50.0)
    >>> stats.AddSample(TIME_KEY, 25.0)
    >>> stats.AddSample(TIME_KEY, 40.0)
    >>> stats.AddSample(TIME_KEY, 10.0)
    >>> stats.AddSample(TIME_KEY, 10.0)
    >>> stats.AddSample(frobnicate, 11.5)
    >>> stats.AddSample(frobnicate, 9.0)
    >>> stats.AddSample(foobar, 11111.0)
    >>> stats.AddSample(foobar, 22222.0)
    >>> stats.CalculateStats()
    >>> print(stats.SummaryToString())
    @@            NAME  COUNT      MEAN   STDDEV       MAX       MIN
    @@   sample_msecs      4     31.25    15.16     50.00     10.00
    @@         foobar      2  16666.50  5555.50  22222.00  11111.00
    @@     frobnicate      2     10.25     1.25     11.50      9.00

  Attributes:
    _data: dict of list of samples for each domain (key)
    _unit: dict of unit for each domain (key)
    _title: title to add as banner to formatted summary. If no title,
            no banner gets added
    _summarytag: tag to prepend to output file names to distinguish between
                 multiple StatsManager instances saving data to the same
                 location.
    _order: list of formatting order for domains. Domains not listed are
            displayed in sorted order
    _hide_domains: collection of domains to hide when formatting summary string
    _summary: dict of stats per domain (key): min, max, count, mean, stddev
    _logger = StatsManager logger

  Note:
    _summary is empty until CalculateStats() is called
  """

  def __init__(self, title='', summarytag='', hide_domains=[], order=[],
               accept_nan=True):
    """Initialize infrastructure for data and their statistics."""
    self._title = title
    self._summarytag = '%s_' % summarytag if summarytag else summarytag
    self._data = collections.defaultdict(list)
    self._unit = collections.defaultdict(str)
    self._order = order
    self._hide_domains = hide_domains
    self._summary = {}
    self._accept_nan = accept_nan
    self._logger = logging.getLogger('StatsManager')

  def AddSample(self, domain, value):
    """Add one value for a domain.

    Args:
      domain: the domain name for the value.
      value: one time sample for domain, expect type float.
    """
    try:
      value = float(value)
    except ValueError:
      #if we don't accept nan this will be caught below
      value = float('NaN')
      self._logger.debug('value %s for domain %s is not a number. Making NaN'
                         % (value, domain))
    if not self._accept_nan and math.isnan(value):
      raise StatsManagerError('accept_nan is false. Cannot add NaN value %s.' %
                              str(value))
    self._data[domain].append(value)

  def RecordReadings(self, readings):
    """Record multiple readings at once.

    Args:
      readings: list of (domain, reading) tuples to record
    """
    for domain, reading in readings:
      self.RecordReading(domain, reading)

  def SetUnit(self, domain, unit):
    """Set the unit for a domain.

    There can be only one unit for each domain. Setting unit twice will
    overwrite the original unit.

    Args:
      domain: the domain name.
      unit: unit of the domain.
    """
    if domain in self._unit:
      self._logger.warn('overwriting the unit of %s, old unit is %s, new unit '
                        'is %s.' % (domain, self._unit[domain], unit))
    self._unit[domain] = unit

  def CalculateStats(self):
    """Calculate stats for all domain-data pairs.

    First erases all previous stats, then calculate stats for all data.
    """
    self._summary = {}
    for domain, data in self._data.iteritems():
      data_np = numpy.array(data)
      self._summary[domain] = {
          'mean': numpy.nanmean(data_np),
          'min': numpy.nanmin(data_np),
          'max': numpy.nanmax(data_np),
          'stddev': numpy.nanstd(data_np),
          'count': data_np.size,
      }

  def _RetrieveUnitSuffix(self, domain):
    """Helper to get a unit suffix.

    Args:
      domain: domain to create unit suffix for

    Returns:
      _%s with the unit if there is a unit for the domain
      '' otherwise
    """
    suffix = self._unit[domain]
    if suffix:
      suffix = '_%s' % suffix
    return suffix

  def SummaryToString(self, prefix=STATS_PREFIX):
    """Format summary into a string, ready for pretty print.

   See class description for format example.

    Args:
      prefix: start every row in summary string with prefix, for easier reading.
    """
    headers = ('NAME', 'COUNT', 'MEAN', 'STDDEV', 'MAX', 'MIN')
    table = [headers]
    #determine what domains to display & and the order
    domains_to_display = set(self._summary.keys()) - set(self._hide_domains)
    display_order = [key for key in self._order if key in domains_to_display]
    domains_to_display -= set(display_order)
    display_order.extend(list(sorted(domains_to_display)))
    for domain in display_order:
      stats = self._summary[domain]
      unit_suffix = self._RetrieveUnitSuffix(domain)
      if not domain.endswith(unit_suffix):
        domain = '%s%s' % (domain, unit_suffix)
      row = [domain]
      row.append(str(stats['count']))
      for entry in headers[2:]:
        row.append('%.2f' % stats[entry.lower()])
      table.append(row)

    max_col_width = []
    for col_idx in range(len(table[0])):
      col_item_widths = [len(row[col_idx]) for row in table]
      max_col_width.append(max(col_item_widths))

    formatted_lines = []
    for row in table:
      formatted_row = prefix + ' '
      for i in range(len(row)):
        formatted_row += row[i].rjust(max_col_width[i] + 2)
      formatted_lines.append(formatted_row)

    formatted_output = '\n'.join(formatted_lines)
    title = self._title
    if title:
      dec_len = len(prefix)
      line_length = len(formatted_lines[0])
      line = "%s%s" % (prefix, ''.join(['-'] * (line_length - dec_len)))
      if len(title) > line_length:
        title = title[:line_length]
      padded_title = "%s%s" % (prefix, title.center(line_length)[dec_len:])
      formatted_output = '\n'.join([line, padded_title, line, formatted_output,
                                    line])
    return formatted_output

  def GetSummary(self):
    """Getter for summary."""
    return self._summary

  def _rotate_fname(self, fname):
    """Rotate filename to ensure no data gets clobbered.

    Before saving a file through the StatsManager, make sure that the filename
    is unique, and otherwise keep trying increasing integers until the fname is
    unique.

    /path/to/example/file.txt becomes /path/to/example/file1.txt if the first
    one already exists on the system.

    Args:
      fname: filename to ensure uniqueness.

    Returns:
      Same fname (with potentially an integer tag) that is guaranteed to be
      unique on the filesystem at the time of calling.
    """
    fdir = os.path.dirname(fname)
    base, ext = os.path.splitext(os.path.basename(fname))
    uniquefname = fname
    tag = 0
    while os.path.exists(uniquefname):
      oldfn = uniquefname
      uniquefname = os.path.join(fdir, "%s%d%s" % (base, tag, ext))
      self._logger.warn("Attempted to store stats information at %s, but file "
                        "already exists. Attempting to store at %s now.",
                        oldfn, uniquefname)
      tag += 1
    return uniquefname

  def SaveSummary(self, directory, fname='summary.txt', prefix=STATS_PREFIX):
    """Save summary to file.

    Args:
      directory: directory to save the summary in.
      fname: filename to save summary under.
      prefix: start every row in summary string with prefix, for easier reading.
    """
    summary_str = self.SummaryToString(prefix=prefix) + '\n'
    if not os.path.exists(directory):
      os.makedirs(directory)
    fname = self._rotate_fname(os.path.join(directory,
                                            '%s%s' % (self._summarytag, fname)))
    with open(fname, 'w') as f:
      f.write(summary_str)
    return fname

  def SaveSummaryJSON(self, directory, fname='summary.json'):
    """Save summary (only MEAN) into a JSON file.

    Args:
      directory: directory to save the JSON summary in.
      fname: filename to save summary under.
    """
    data = {}
    for domain in self._summary:
      unit = LONG_UNIT.get(self._unit[domain], self._unit[domain])
      if not unit:
        unit = 'N/A'
      data_entry = {'mean': self._summary[domain]['mean'], 'unit': unit}
      data[domain] = data_entry
    if not os.path.exists(directory):
      os.makedirs(directory)
    fname = self._rotate_fname(os.path.join(directory,
                                            '%s%s' % (self._summarytag, fname)))
    with open(fname, 'w') as f:
      json.dump(data, f)
    return fname

  def GetRawData(self):
    """Getter for all raw_data."""
    return self._data

  def SaveRawData(self, directory, dirname='raw_data'):
    """Save raw data to file.

    Args:
      directory: directory to create the raw data folder in.
      dirname: folder in which raw data live.
    """
    if not os.path.exists(directory):
      os.makedirs(directory)
    dirname = os.path.join(directory, dirname)
    if not os.path.exists(dirname):
      os.makedirs(dirname)
    files = []
    for domain, data in self._data.iteritems():
      fname = '%s%s%s.txt' % (self._summarytag, domain,
                              self._RetrieveUnitSuffix(domain))
      fname = self._rotate_fname(os.path.join(dirname, fname))
      with open(fname, 'w') as f:
        f.write('\n'.join('%.2f' % value for value in data) + '\n')
      files.append(fname)
    return files
