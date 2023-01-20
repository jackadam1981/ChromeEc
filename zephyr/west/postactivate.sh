#!/bin/bash
# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# This is ran after the virtual environment is activated

_postactivate_abspath () {
  $(command -v python3 || command -v python2 || command -v python) \
  -c "import os.path; print(os.path.abspath('$*'))"
}

# shellcheck disable=SC2154
# shellcheck disable=SC2296
# Shell: bash.
if test -n "${BASH}"; then
  _POSTACTIVATE_PATH="$(_postactivate_abspath "${BASH_SOURCE[0]}")"
# Shell: zsh.
elif test -n "${ZSH_NAME}"; then
  _POSTACTIVATE_PATH="$(_postactivate_abspath "${(%):-%N}")"
# Shell: dash.
elif test "${0##*/}" = dash; then
  _POSTACTIVATE_PATH="$(_postactivate_abspath \
    "$(lsof -p $$ -Fn0 | tail -1 | sed 's#^[^/]*##;')")"
# If everything else fails, try $0. It could work.
else
  _POSTACTIVATE_PATH="$(_postactivate_abspath "$0")"
fi

_POSTACTIVATE_ROOT="$(dirname "${_POSTACTIVATE_PATH}")"

export EC_DIR="${_POSTACTIVATE_ROOT}/../../modules/ec"
export ZEPHYR_BASE="${_POSTACTIVATE_ROOT}/../../zephyr"
export MODULES_DIR="${_POSTACTIVATE_ROOT}/../../modules"
