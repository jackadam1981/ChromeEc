# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for StatsManager."""

from __future__ import print_function
import json
import os
import shutil
import tempfile
import unittest
import re

from stats_manager import StatsManager

class TestStatsManager(unittest.TestCase):
  """Test to verify StatsManager methods work as expected.

  StatsManager should collect raw data, calculate their statistics, and save
  them in expected format.
  """

  def _populate_dummy_stats(self):
    """Create a populated & processed StatsManager to test data retrieval."""
    self.data.AddSample('A', 99999.5)
    self.data.AddSample('A', 100000.5)
    self.data.AddSample('A', 'ERROR')
    self.data.SetUnit('A', 'uW')
    self.data.SetUnit('A', 'mW')
    self.data.AddSample('B', 1.5)
    self.data.AddSample('B', 2.5)
    self.data.AddSample('B', 3.5)
    self.data.SetUnit('B', 'mV')
    self.data.CalculateStats()

  def setUp(self):
    """Set up data and create a temporary directory to save data and stats."""
    self.tempdir = tempfile.mkdtemp()
    self.data = StatsManager()

  def tearDown(self):
    """Delete the temporary directory and its content."""
    shutil.rmtree(self.tempdir)

  def test_AddSample(self):
    """Adding a value successfully adds a value."""
    self.data.AddSample('Test', 1000)
    self.data.SetUnit('Test', 'test')
    self.data.CalculateStats()
    summary = self.data.GetSummary()
    self.assertEqual(1, summary['Test']['count'])

  def test_AddSampleNoFloat(self):
    """Adding a non number gets ignored and doesn't raise an exception."""
    self.data.AddSample('Test', 17)
    self.data.AddSample('Test', 'fiesta')
    self.data.SetUnit('Test', 'test')
    self.data.CalculateStats()
    summary = self.data.GetSummary()
    self.assertEqual(1, summary['Test']['count'])

  def test_AddSampleNoUnit(self):
    """Not adding a unit does not cause an exception on CalculateStats()."""
    self.data.AddSample('Test', 17)
    self.data.CalculateStats()
    summary = self.data.GetSummary()
    self.assertEqual(1, summary['Test']['count'])

  def test_UnitSuffix(self):
    """Unit gets appended as a suffix in the displayed summary."""
    self.data.AddSample('test', 250)
    self.data.SetUnit('test', 'mw')
    self.data.CalculateStats()
    summary_str = self.data.SummaryToString()
    self.assertIn('test_mw', summary_str)

  def test_DoubleUnitSuffix(self):
    """If domain already ends in unit, verify that unit doesn't get appended."""
    self.data.AddSample('test_mw', 250)
    self.data.SetUnit('test_mw', 'mw')
    self.data.CalculateStats()
    summary_str = self.data.SummaryToString()
    self.assertIn('test_mw', summary_str)
    self.assertNotIn('test_mw_mw', summary_str)

  def test_GetRawData(self):
    """GetRawData returns exact same data as fed in."""
    self._populate_dummy_stats()
    raw_data = self.data.GetRawData()
    self.assertListEqual([99999.5, 100000.5], raw_data['A'])
    self.assertListEqual([1.5, 2.5, 3.5], raw_data['B'])

  def test_GetSummary(self):
    """GetSummary returns expected stats about the data fed in."""
    self._populate_dummy_stats()
    summary = self.data.GetSummary()
    self.assertEqual(2, summary['A']['count'])
    self.assertAlmostEqual(100000.5, summary['A']['max'])
    self.assertAlmostEqual(99999.5, summary['A']['min'])
    self.assertAlmostEqual(0.5, summary['A']['stddev'])
    self.assertAlmostEqual(100000.0, summary['A']['mean'])
    self.assertEqual(3, summary['B']['count'])
    self.assertAlmostEqual(3.5, summary['B']['max'])
    self.assertAlmostEqual(1.5, summary['B']['min'])
    self.assertAlmostEqual(0.81649658092773, summary['B']['stddev'])
    self.assertAlmostEqual(2.5, summary['B']['mean'])

  def test_SummaryToStringHideDomains(self):
    """Keys indicated in hide_domains are not printed in the summary."""
    data = StatsManager(hide_domains=['A-domain'])
    data.AddSample('A-domain', 17)
    data.AddSample('B-domain', 17)
    data.CalculateStats()
    summarystr = data.SummaryToString()
    self.assertIn('B-domain', summarystr)
    self.assertNotIn('A-domain', summarystr)

  def test_SummaryToStringOrder(self):
    """Order passed into StatsManager is honored when formatting summary."""
    a_before_b_regexp = re.compile('A-domain.*B-domain', re.DOTALL)
    b_before_a_regexp = re.compile('B-domain.*A-domain', re.DOTALL)
    #StatsManager that should print A before B
    a2b_data = StatsManager(order=['A-domain'])
    a2b_data.AddSample('A-domain', 17)
    a2b_data.AddSample('B-domain', 17)
    a2b_data.CalculateStats()
    a2b_summarystr = a2b_data.SummaryToString()
    self.assertRegexpMatches(a2b_summarystr, a_before_b_regexp)
    self.assertNotRegexpMatches(a2b_summarystr, b_before_a_regexp)
    #StatsManager that should print B before A
    b2a_data = StatsManager(order=['B-domain'])
    b2a_data.AddSample('A-domain', 17)
    b2a_data.AddSample('B-domain', 17)
    b2a_data.CalculateStats()
    b2a_summarystr = b2a_data.SummaryToString()
    self.assertRegexpMatches(b2a_summarystr, b_before_a_regexp)
    self.assertNotRegexpMatches(b2a_summarystr, a_before_b_regexp)

  def test_RotateFname(self):
    data = StatsManager()
    testfile = os.path.join(self.tempdir, 'testfile.txt')
    with open(testfile, 'w') as f:
      f.write('')
    expected_rotation = os.path.join(self.tempdir, 'testfile0.txt')
    self.assertEqual(expected_rotation, data._rotate_fname(testfile))

  def test_SaveRawData(self):
    """SaveRawData stores same data as fed in."""
    self._populate_dummy_stats()
    dirname = 'unittest_raw_data'
    files = self.data.SaveRawData(self.tempdir, dirname)
    for fname in files:
      with open(fname, 'r') as f:
        if 'A_mW' in fname:
          self.assertEqual('99999.50', f.readline().strip())
          self.assertEqual('100000.50', f.readline().strip())
        if 'B_mV' in fname:
          self.assertEqual('1.50', f.readline().strip())
          self.assertEqual('2.50', f.readline().strip())
          self.assertEqual('3.50', f.readline().strip())

  def test_SaveRawDataNoUnit(self):
    """SaveRawData appends no unit suffix if the unit is not specified."""
    self.data.AddSample('train', 1000)
    self.data.AddSample('car', 200)
    self.data.SetUnit('car', 'blue')
    self.data.CalculateStats()
    outdir = 'unittest_raw_data'
    files = self.data.SaveRawData(self.tempdir, outdir)
    files = [os.path.basename(f) for f in files]
    #verify expected behavior with a unit
    self.assertIn('car_blue.txt', files)
    #verify expected behavior without a unit
    self.assertIn('train.txt', files)

  def test_SaveSummary(self):
    """SaveSummary properly dumps the summary into a file."""
    self._populate_dummy_stats()
    fname = 'unittest_summary.txt'
    fname = self.data.SaveSummary(self.tempdir, fname)
    with open(fname, 'r') as f:
      self.assertEqual(
          '@@   NAME  COUNT       MEAN  STDDEV        MAX       MIN\n',
          f.readline())
      self.assertEqual(
          '@@   A_mW      2  100000.00    0.50  100000.50  99999.50\n',
          f.readline())
      self.assertEqual(
          '@@   B_mV      3       2.50    0.82       3.50      1.50\n',
          f.readline())

  def test_SaveSummaryJSON(self):
    """SaveSummaryJSON saves the same data as fed in."""
    self._populate_dummy_stats()
    fname = 'unittest_summary.json'
    fname = self.data.SaveSummaryJSON(self.tempdir, fname)
    with open(fname, 'r') as f:
      summary = json.load(f)
      self.assertAlmostEqual(100000.0, summary['A']['mean'])
      self.assertEqual('milliwatt', summary['A']['unit'])
      self.assertAlmostEqual(2.5, summary['B']['mean'])
      self.assertEqual('millivolt', summary['B']['unit'])

  def test_SaveSummaryJSONNoUnit(self):
    """SaveSummaryJSON marks unknown units properly as N/A."""
    self.data.AddSample('train', 1000)
    self.data.AddSample('car', 200)
    self.data.SetUnit('car', 'blue')
    self.data.CalculateStats()
    fname = 'unittest_summary.json'
    fname = self.data.SaveSummaryJSON(self.tempdir, fname)
    with open(fname, 'r') as f:
      summary = json.load(f)
      self.assertEqual('blue',summary['car']['unit'])
      self.assertEqual('N/A',summary['train']['unit'])

if __name__ == '__main__':
  unittest.main()
