#!/usr/bin/env python3

# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import argparse
import logging
import subprocess
import sys
import time

DEFAULT_NUM_TEST_ITERATIONS = 10
DEFAULT_SSH_IP = 'dragonair'
DEFAULT_FIRMWARE_FILE = \
    '/opt/google/biod/fw/bloonchipper_v2.0.4277-9f652bb3.bin'


def run_dut_control(args):
    cmd = ['dut-control'] + args
    subprocess.run(cmd).check_returncode()


def run_ectool(ip, args):
    cmd = ['ssh', ip, 'ectool', '--name=cros_fp'] + args
    subprocess.run(cmd)


def run_flash_fp_mcu(ip, fw_file):
    cmd = ['ssh', ip, 'flash_fp_mcu', fw_file]
    completed_process = subprocess.run(cmd)
    return completed_process.returncode == 0


def run_test(ip, fw_file):
    logging.debug('Enable hardware write protect')
    run_dut_control(['fw_wp_state:force_on'])
    logging.debug('Initial flash protection settings:')
    run_ectool(ip, ['flashprotect'])
    logging.debug('Enable software write protect')
    run_ectool(ip, ['flashprotect', 'enable'])
    # "flashprotect enable" can be slow, so wait for it to complete
    time.sleep(2)
    logging.debug('Flash protection settings after enable:')
    run_ectool(ip, ['flashprotect'])
    logging.debug('sysinfo after enable:')
    run_ectool(ip, ['sysinfo'])
    logging.debug('Rebooting FPMCU')
    run_ectool(ip, ['reboot_ec'])
    logging.debug('Flash protection settings after reboot:')
    run_ectool(ip, ['flashprotect'])
    logging.debug('Disabling hardware write protect')
    run_dut_control(['fw_wp_state:force_off'])

    logging.debug('Running flash_fp_mcu')
    return run_flash_fp_mcu(ip, fw_file)


def main():
    parser = argparse.ArgumentParser()

    parser.add_argument(
        '--num_iterations', '-n',
        help='Number of iterations to run (default: ' + str(
            DEFAULT_NUM_TEST_ITERATIONS) + ')',
        type=int,
        default=DEFAULT_NUM_TEST_ITERATIONS)

    parser.add_argument(
        '--ip', '-i',
        help='IP address of DUT (default: ' + DEFAULT_SSH_IP + ')',
        default=DEFAULT_SSH_IP
    )

    parser.add_argument(
        '--firmware_file', '-f',
        help='Default: ' + DEFAULT_FIRMWARE_FILE,
        default=DEFAULT_FIRMWARE_FILE
    )

    log_level_choices = ['DEBUG', 'INFO', 'WARNING', 'ERROR', 'CRITICAL']
    parser.add_argument(
        '--log_level', '-l',
        choices=log_level_choices,
        default='DEBUG'
    )

    args = parser.parse_args()
    logging.basicConfig(level=args.log_level)

    num_attempts = 0
    while num_attempts < args.num_iterations:
        num_attempts += 1
        logging.info('Running attempt: %d', num_attempts)
        if not run_test(args.ip, args.firmware_file):
            logging.error('Error running flash_fp_mcu')
            return 1

    return 0


if __name__ == '__main__':
    sys.exit(main())
