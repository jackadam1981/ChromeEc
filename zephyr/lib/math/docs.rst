.. Copyright 2025 The ChromiumOS Authors
   Use of this source code is governed by a BSD-style license that can be
   found in the LICENSE file.

============
Math Library
============
This library provides a collection of mathematical functions and data structures.

----------
IIR Filter
----------
The IIR (Infinite Impulse Response) filter is a type of signal processing filter that uses feedback to process signals. This implementation provides a generic IIR filter that can be configured with custom coefficients, as well as a Butterworth low-pass filter.

.. doxygenfile:: iir_filter.h

-------------
IIR Decimator
-------------
The IIR decimator is used to reduce the sampling rate of a signal. It uses an IIR filter to prevent aliasing.

.. doxygenfile:: iir_decimator.h

---------------------
Exponential Smoothing
---------------------
The exponential smoothing filter is a simple low-pass filter that can be used to
smooth out noisy data.

.. mermaid::
   :align: center

   xychart-beta
      title "Exponential Smoothing of Accelerometer Data"
      x-axis "Sample" 1 --> 30
      y-axis "Acceleration (G)" 0.5 --> 2.5
      line "Raw Data" [1.004,1.001,0.996,1.076,1.032,1.113,1.671,1.547,1.451,1.766,1.860,1.813,1.161,1.298,1.018,1.151,1.063,0.998,0.998,0.996,1.000,1.005,1.003,1.002,0.998,0.999,1.005,0.998,1.001,1.004]
      line "Smoothed" [1.004,1.002,0.999,1.038,1.035,1.074,1.373,1.460,1.455,1.611,1.736,1.774,1.467,1.383,1.200,1.175,1.119,1.059,1.028,1.012,1.006,1.005,1.004,1.003,1.001,1.000,1.002,1.000,1.001,1.002]

.. doxygenfile:: exp_smoothing.h
