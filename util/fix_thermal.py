#!/usr/bin/env python3

# Copyright 2021 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import logging
import re
import sys

"""
To fix all instances:
git grep --name-only 'struct ec_thermal_config' board | xargs -I {} ./util/fix_thermal.py -f {}
"""

DEFINE_STR = '''/*
 * TODO(b/202062363): Remove when clang is fixed.
 */
#define {var_upper} \\
	{{ \\
{body}	}}
'''

IFDEF_STR = '''__maybe_unused static const struct ec_thermal_config {var_lower} = {var_upper};
'''

THERMAL_PARAMS_STR = '''struct ec_thermal_config thermal_params[] = {{
	{params}}};
'''

THERMAL_CONFIG_PATTERN = re.compile(
    r'^(?:static|const) (?:static|const) struct ec_thermal_config ([\w_]+) = {(.*?)};',
    re.DOTALL)

THERMAL_PARAMS_PATTERN = re.compile(r'^struct ec_thermal_config ([\w_]+)\[] = {(.*?)};', re.DOTALL)
THERMAL_PARAMS_ARRAY_PATTERN = re.compile(r'^\s*(\[[\w_]+])\s*=\s*([\w_]+),')

def fix_thermal(text: list) -> str:
    remaining_text_lines = text
    ret = ''

    while len(remaining_text_lines) > 0:
        match = THERMAL_PARAMS_PATTERN.match(''.join(remaining_text_lines))
        if match:
            matched_line_count = len(match.group(0).splitlines())
            remaining_text_lines = remaining_text_lines[matched_line_count:]

            params = ''
            for line in match.group(2).splitlines():
                m = THERMAL_PARAMS_ARRAY_PATTERN.match(line)
                if m:
                    params = params + m.group(1) + ' = ' + m.group(2).upper() + ",\n"
            if params:
                ret = ret + THERMAL_PARAMS_STR.format(params=params)
            continue

        match = THERMAL_CONFIG_PATTERN.match(''.join(remaining_text_lines))
        if match:
            matched_line_count = len(match.group(0).splitlines())
            remaining_text_lines = remaining_text_lines[matched_line_count:]

            var_upper = match.group(1).upper()
            body = ''
            for line in match.group(2).splitlines():
                if line == '':
                    continue
                body = body + "\t" + line + " \\\n"

            ret = ret + DEFINE_STR.format(var_upper=var_upper, body=body)

            ret = ret + IFDEF_STR.format(var_lower=var_upper.lower(),
                                         var_upper=var_upper)
            continue

        ret = ret + remaining_text_lines[0]
        remaining_text_lines = remaining_text_lines[1:]

    return ret


def main() -> int:
    parser = argparse.ArgumentParser()

    log_level_choices = ['DEBUG', 'INFO', 'WARNING', 'ERROR', 'CRITICAL']
    parser.add_argument(
        '--log_level', '-l',
        choices=log_level_choices,
        default='DEBUG'
    )

    parser.add_argument(
        '--file', '-f',
    )

    args = parser.parse_args()
    logging.basicConfig(level=args.log_level)

    logging.debug('Modifying file: %s' % args.file)
    with open(args.file, 'r') as f:
        lines = f.readlines()
        ret = fix_thermal(lines)

    with open(args.file, 'w') as f:
        f.write(ret)

    return 0


if __name__ == '__main__':
  sys.exit(main())
