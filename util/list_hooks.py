# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Dumps the all the hooks in an EC ELF file.
Currently only supports Zephyr EC images.
Extending to CrOS EC should be trivial.

Depends on addr2line and readelf being in PATH.
"""

import sys
import subprocess
import re
import struct

def read_bytes_from_file(elf_path, offset, size, endian='little'):
    with open(elf_path, 'rb') as f:
        f.seek(offset)
        data = f.read(size)

    if endian == 'little':
        fmt = f'<'
    elif endian == 'big':
        fmt = f'>'
    else:
        raise Exception('Unknown endian')
    if size == 1:
        fmt += 'B'
    elif size == 2:
        fmt += 'H'
    elif size == 4:
        fmt += 'I'
    else:
        raise Exception('Unhandled size')
    return struct.unpack(fmt, data)[0]

def addr2line(elf_path, addr):
    result = subprocess.run(['addr2line', '-fs', '-e', elf_path, hex(addr)], stdout=subprocess.PIPE, check=False)
    if result.returncode != 0:
        raise Exception('Error reading elf sections')
    match = re.search(r'(?P<func>\S+)\n(?P<file>\S+):(?P<line>[0-9]+)', result.stdout.decode('utf-8'), re.MULTILINE)
    if not match:
        raise Exception('Error getting addr2line')
    return match.groupdict()

def read_elf_sections(elf_path):
    result = subprocess.run(['readelf', '-t', elf_path], stdout=subprocess.PIPE, check=False)
    if result.returncode != 0:
        raise Exception('Error reading elf sections')

    for match in re.finditer(r'\[\d+\] (?P<name>\S+)\n +(?P<type>\S+)\s+(?P<addr>[0-9a-f]+)\s+(?P<offset>[0-9a-f]+)\s+(?P<size>[0-9a-f]+)', result.stdout.decode('utf-8'), re.MULTILINE):
        yield match.groupdict()

def print_header():
    print('{:<30}{:<10}{:<15}{:<30}{:<8}{:<30}'.format('hook_type', 'priority', 'hook_address', 'file', 'line', 'func'))
    print('{:<30}{:<10}{:<15}{:<30}{:<8}{:<30}'.format('---------', '--------', '------------', '----', '----', '----'))

def print_entry(hook_type, priority, hook_address, file, line, func):
    print('{:<30}{:<10}{:<15}{:<30}{:<8}{:<30}'.format(hook_type, priority, hex(hook_address), file, line, func))

def main(argv):
    elf_path = argv[1]

    print_header()

    for section in read_elf_sections(elf_path):
        hook_type = re.match(r'zephyr_shim_hook_HOOK_(\S+)_area', section['name'])
        if not hook_type:
            continue
        hook_type = hook_type.group(1)
        entry_count = int(int(section['size'],16)/8)
        offset = int(section['offset'], 16)
        for i in range(entry_count):
            hook_address = read_bytes_from_file(elf_path, offset + i*8, 4, 'little')
            priority = read_bytes_from_file(elf_path, offset + i*8 + 4, 4, 'little')
            line_info = addr2line(elf_path, hook_address)
            print_entry(hook_type, priority, hook_address, line_info['file'], line_info['line'], line_info['func'])

if __name__ == "__main__":
    sys.exit(main(sys.argv))
