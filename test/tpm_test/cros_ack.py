#!/usr/bin/python
# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module for initializing and driving a SPI TPM."""

from __future__ import print_function

from collections import namedtuple
import os
import struct
import sys
import traceback

from tpmtest import TPM
import utils
import subcmd

# Suppressing pylint warning about an import not at the top of the file. The
# path needs to be set *before* the last import.
# pylint: disable=C6204
root_dir = os.path.dirname(os.path.abspath(sys.argv[0]))
sys.path.append(os.path.join(root_dir, '..', '..', 'build', 'tpm_test'))

CrosTPMPersoResponseHeader_v0 = namedtuple(
    'CrosTPMPersoResponseHeader_v0',
    ['tag',
     'size',
     'command_code',
     'subcommand_code'])

CrosAckResponse_v0 = namedtuple(
    'CrosAckResponse_v0',
        ['magic',
         'payload_version',
         'n_keys',
         'names',
         'filler',
         'checksum'])
CrosAckResponse_v0_FMT = '< I H H 960s 12s 32s'
assert struct.Struct(CrosAckResponse_v0_FMT).size == 1012

PersoResponse_v0_FMT = '<H'
assert struct.Struct(PersoResponse_v0_FMT).size == 2

_PERSO_FILENAME_LEN = 31

if __name__ == '__main__':
  try:
    debug_needed = len(sys.argv) == 2 and sys.argv[1] == '-d'
    t = TPM(debug_mode=debug_needed, try_startup=False)
    wrapped_response = t.command(t.wrap_ext_command(subcmd.MACK, ''))

    # Unpack ignoring TPM header
    ack = CrosAckResponse_v0._make(struct.unpack(CrosAckResponse_v0_FMT,
                                                 wrapped_response[12:]))
    chip_id = ack.names[:32]

    perso_data = ''
    chip_perso_data_file = chip_id[:_PERSO_FILENAME_LEN] + '.res'
    with open(os.path.join(chip_perso_data_file)) as f:
      perso_data = f.read()
    print(utils.cursor_back() + 'CHIP ID: %s' % chip_perso_data_file)

    # Perso messages include the TPM header which is not needed.
    assert(len(perso_data) == 4096)
    half = len(perso_data)/2
    first_perso_data = perso_data[12:half]
    second_perso_data = perso_data[half + 12:]

    response = t.command(t.wrap_ext_command(subcmd.MPERSO, first_perso_data))
    ok = struct.unpack(PersoResponse_v0_FMT, response[12:])
    print(utils.cursor_back() + 'FIRST PERSO RESULT: %x' % ok)

    response = t.command(t.wrap_ext_command(subcmd.MPERSO, second_perso_data))
    ok = struct.unpack(PersoResponse_v0_FMT, response[12:])
    print(utils.cursor_back() + 'SECOND PERSO_RESULT: %x' % ok)

  except subcmd.TpmTestError as e:
    exc_file, exc_line = traceback.extract_tb(sys.exc_traceback)[-1][:2]
    print('\nError in %s:%s: ' % (os.path.basename(exc_file), exc_line), e)
    if debug_needed:
      traceback.print_exc()
    sys.exit(1)
