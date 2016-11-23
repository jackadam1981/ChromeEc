#!/usr/bin/python
# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" Adapted from Android's monsoon streamgraph."""

import matplotlib
import matplotlib.pyplot as plt
import sys
import time
from threading  import Thread
try:
    from Queue import Queue, Empty
except ImportError:
    from queue import Queue, Empty  # python 3.x

class RealTimeDrawer():
  def __init__(self):
    plt.ion()
    self.fig = plt.figure(figsize=(20, 10))
    self.ax = plt.axes()
    self.ax.set_xlim(-20,0)
    self.ax.set_yscale('log')
    self.ax.set_ylim(0.0001,10)
    self.ax.hold(True)
    plt.tight_layout(pad=1)
    self.fig.canvas.draw()
    self.background = self.fig.canvas.copy_from_bbox(self.ax.bbox) # cache the background
    self.plot = self.ax.plot([],[])[0]
    self.txt = self.ax.text(-1, 0.0005, "placeholder", horizontalalignment='right')
    self.times = [time.time()] * 2000
    self.values = [0.0] * 2000
    self.cum_values = [0.0] * 2000

  def UpdateGraph(self):
    x = [t - time.time() for t in self.times]
    y = self.values
    self.plot.set_data(x, y)                           # update the xy data
    self.fig.canvas.restore_region(self.background)    # restore background
    self.ax.draw_artist(self.plot)                     # redraw just the points
    self.txt.set_text("current: %.03fmW\n" % (self.values[-1] * 1000) +
                      "average(1sec): %.03fmW\n" % (self.Average(-1, 0) * 1000) +
                      "average(5sec): %.03fmW\n" % (self.Average(-5, 0) * 1000) +
                      "average(10sec): %.03fmW" % (self.Average(-10, 0) * 1000))
    self.ax.draw_artist(self.txt)                      # redraw just the points
    self.fig.canvas.blit(self.ax.bbox)                 # fill in the axes rectangle

  def Average(self, time_start, time_end):
    current_time = time.time()
    vals = [self.values[i] for i in range(len(self.times)) if self.times[i] > current_time + time_start]
    if vals:
      return sum(vals) / len(vals)
    return 0

  def AddValue(self, current_value):
    current_time = time.time()
    self.times = self.times[1:] + [current_time]
    self.values = self.values[1:] + [current_value]

def RunAsync():
  def enqueue_output(out, queue):
      for line in iter(out.readline, b''):
          queue.put(line)
      out.close()

  q = Queue()
  t = Thread(target=enqueue_output, args=(sys.stdin, q))
  t.daemon = True # thread dies with the program
  t.start()

  drawer = RealTimeDrawer()
  first_time=time.time()
  i = 0
  while True:
    try:
      line = q.get_nowait()
    except Empty:
      time.sleep(0.001)
      drawer.UpdateGraph()
    else:
      try:
	timest, val = line.split(',')
        current_value = float(val)
	current_value /= 1000000.
	seconds = float(timest)
      except ValueError:
        continue
      drawer.AddValue(current_value)
      print "%s\t%s\t%s" % (i, seconds, current_value)
      i += 1


if __name__ == '__main__':
  RunAsync()

