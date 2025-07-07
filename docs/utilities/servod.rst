.. Copyright 2025 The ChromiumOS Authors
   Use of this source code is governed by a BSD-style license that can be
   found in the LICENSE file.

.. _servod:

======
Servod
======

.. note::
    The content of this page is a summary of the more comprehensive guide
    available at https://chromium.googlesource.com/chromiumos/third_party/hdctools/+/HEAD/docs/servod_outside_chroot.md.
    That document is the source of truth and should be consulted for detailed
    instructions.

This document explains how to use ``servod`` outside of the ChromeOS SDK chroot.
This allows users to run ``servod`` from a pre-built docker image without
needing to build any code, unless they are modifying ``servod`` itself.

The document assumes the user has followed the ChromiumOS developer guide and is
running a Linux distribution that supports Docker.

Installation
------------

1.  **Install Docker:** Install the Docker engine on your Linux distribution.
2.  **User Groups:** Add your user to the ``tty`` and ``docker`` groups to avoid
    permission errors. You may need to reboot for this to take effect.

Setup
-----
The primary tools for interacting with ``servod`` are ``dut-control`` and
``servodtool``. These are used to manage and interact with ``servod``
containers, especially when multiple instances are running.

Basic Usage
-----------
The guide provides instructions on how to start, stop, and interact with
``servod`` instances using the provided tools.

Advanced Usage
--------------
For advanced use cases, such as modifying ``servod`` itself, the guide
details the necessary steps to build and run a custom Docker image.

FAQ
---
A FAQ section is available in the source document to address common issues and
questions.
