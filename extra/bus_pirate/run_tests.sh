#!/bin/bash
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

source ./env/bin/activate

pip install -r requirements.txt

python3.6 -m unittest discover

pycodestyle --show-source --show-pep8 --exclude=env .
pylint `pwd`
