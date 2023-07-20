#!/usr/bin/env python3
# Copyright 2020 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=line-too-long

"""Runs unit tests on device and displays the results.

This script assumes you have a ~/.servodrc config file with a line that
corresponds to the board being tested.

See https://chromium.googlesource.com/chromiumos/third_party/hdctools/+/HEAD/docs/servo.md#servodrc

In addition to running this script locally, you can also run it from a remote
machine against a board connected to a local machine. For example:

Start servod and JLink locally:

(local chroot) $ sudo servod --board dragonclaw
(local chroot) $ sudo JLinkRemoteServerCLExe -select USB

Forward the FPMCU console on a TCP port:

(local chroot) $ socat $(dut-control raw_fpmcu_console_uart_pty | cut -d: -f2) \
                 tcp4-listen:10000,fork

Forward all the ports to the remote machine:

(local outside) $ ssh -R 9999:localhost:9999 <remote> -N
(local outside) $ ssh -R 10000:localhost:10000 <remote> -N
(local outside) $ ssh -R 19020:localhost:19020 <remote> -N

Run the script on the remote machine:

(remote chroot) ./test/run_device_tests.py --remote 127.0.0.1 \
                --jlink_port 19020 --console_port 10000
"""
# pylint: enable=line-too-long
# TODO(b/267800058): refactor into multiple modules
# pylint: disable=too-many-lines

from abc import abstractmethod
import argparse
import concurrent
from concurrent.futures.thread import ThreadPoolExecutor
from dataclasses import dataclass
from dataclasses import field
from enum import Enum
import io
import logging
import os
from pathlib import Path
import pathlib
import re
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
import time
from typing import BinaryIO, Callable, Dict, List, Optional, Tuple

# pylint: disable=import-error
import colorama  # type: ignore[import]
from contextlib2 import ExitStack
import fmap


# pylint: enable=import-error

EC_DIR = Path(os.path.dirname(os.path.realpath(__file__))).parent
JTRACE_FLASH_SCRIPT = os.path.join(EC_DIR, "util/flash_jlink.py")
SERVO_MICRO_FLASH_SCRIPT = os.path.join(EC_DIR, "util/flash_ec")
FLASH_FP_MCU_FLASH_SCRIPT = os.path.join(
    EC_DIR, "util/flash_ssh_flash_fp_mcu.py"
)

ALL_TESTS_PASSED_REGEX = re.compile(r"Pass!\r\n")
ALL_TESTS_FAILED_REGEX = re.compile(r"Fail! \(\d+ tests\)\r\n")

SINGLE_CHECK_PASSED_REGEX = re.compile(r"Pass: .*")
SINGLE_CHECK_FAILED_REGEX = re.compile(r".*failed:.*")

RW_IMAGE_BOOTED_REGEX = re.compile(r"^\[Image: RW.*")

ASSERTION_FAILURE_REGEX = re.compile(r"ASSERTION FAILURE.*")

DATA_ACCESS_VIOLATION_8020000_REGEX = re.compile(
    r"Data access violation, mfar = 8020000\r\n"
)
DATA_ACCESS_VIOLATION_8040000_REGEX = re.compile(
    r"Data access violation, mfar = 8040000\r\n"
)
DATA_ACCESS_VIOLATION_80C0000_REGEX = re.compile(
    r"Data access violation, mfar = 80c0000\r\n"
)
DATA_ACCESS_VIOLATION_80E0000_REGEX = re.compile(
    r"Data access violation, mfar = 80e0000\r\n"
)
DATA_ACCESS_VIOLATION_20000000_REGEX = re.compile(
    r"Data access violation, mfar = 20000000\r\n"
)
DATA_ACCESS_VIOLATION_24000000_REGEX = re.compile(
    r"Data access violation, mfar = 24000000\r\n"
)
DATA_ACCESS_VIOLATION_64020000_REGEX = re.compile(
    r"Data access violation, mfar = 64020000\r\n"
)
DATA_ACCESS_VIOLATION_64040000_REGEX = re.compile(
    r"Data access violation, mfar = 64040000\r\n"
)

PRINTF_CALLED_REGEX = re.compile(r"printf called\r\n")

BLOONCHIPPER = "bloonchipper"
DARTMONKEY = "dartmonkey"
HELIPILOT = "helipilot"

JTRACE = "jtrace"
SERVO_MICRO = "servo_micro"
FLASH_FP_MCU = "flash_fp_mcu"

GCC = "gcc"
CLANG = "clang"

PRIVATE_YES = "yes"
PRIVATE_NO = "no"
PRIVATE_ONLY = "only"

TEST_ASSETS_BUCKET = "gs://chromiumos-test-assets-public/fpmcu/RO"
DARTMONKEY_IMAGE_PATH = os.path.join(
    TEST_ASSETS_BUCKET, "dartmonkey_v2.0.2887-311310808.bin"
)
NOCTURNE_FP_IMAGE_PATH = os.path.join(
    TEST_ASSETS_BUCKET, "nocturne_fp_v2.2.64-58cf5974e.bin"
)
NAMI_FP_IMAGE_PATH = os.path.join(
    TEST_ASSETS_BUCKET, "nami_fp_v2.2.144-7a08e07eb.bin"
)
BLOONCHIPPER_V4277_IMAGE_PATH = os.path.join(
    TEST_ASSETS_BUCKET, "bloonchipper_v2.0.4277-9f652bb3.bin"
)
BLOONCHIPPER_V5938_IMAGE_PATH = os.path.join(
    TEST_ASSETS_BUCKET, "bloonchipper_v2.0.5938-197506c1.bin"
)


class ImageType(Enum):
    """EC Image type to use for the test."""

    RO = 1
    RW = 2


class ApplicationType(Enum):
    """
    Select the application type to use (test or production)
    """

    TEST = 1
    PRODUCTION = 2


@dataclass
# pylint: disable-next=too-many-instance-attributes
class BoardConfig:
    """Board-specific configuration."""

    name: str
    servo_uart_name: str
    servo_power_enable: str
    rollback_region0_regex: object
    rollback_region1_regex: object
    mpu_regex: object
    reboot_timeout: float
    variants: Dict


@dataclass
class TestConfig:
    """Configuration for a given test."""

    # pylint: disable=too-many-instance-attributes
    test_name: str
    imagetype_to_use: ImageType = ImageType.RW
    apptype_to_use: ApplicationType = ApplicationType.TEST
    finish_regexes: List = None
    fail_regexes: List = None
    toggle_power: bool = False
    test_args: List[str] = field(default_factory=list)
    num_flash_attempts: int = 2
    timeout_secs: int = 20
    enable_hw_write_protect: bool = False
    ro_image: str = None
    build_board: str = None
    config_name: str = None
    exclude_boards: List = field(default_factory=list)
    logs: List = field(init=False, default_factory=list)
    passed: bool = field(init=False, default=False)
    num_passes: int = field(init=False, default=0)
    num_fails: int = field(init=False, default=0)

    # The callbacks below are called before and after a test is executed and
    # may be used for additional test setup, post test activities, or other tasks
    # that do not otherwise fit into the test workflow. The default behavior is
    # to simply return True and if either callback returns False then the test
    # is reported a failure.
    pre_test_callback: Callable = field(init=True, default=lambda board: True)
    post_test_callback: Callable = field(init=True, default=lambda board: True)

    def __post_init__(self):
        if self.finish_regexes is None:
            self.finish_regexes = [
                ALL_TESTS_PASSED_REGEX,
                ALL_TESTS_FAILED_REGEX,
            ]
        if self.fail_regexes is None:
            self.fail_regexes = [
                SINGLE_CHECK_FAILED_REGEX,
                ALL_TESTS_FAILED_REGEX,
                ASSERTION_FAILURE_REGEX,
            ]
        if self.config_name is None:
            self.config_name = self.test_name


# All possible tests.
class AllTests:
    """All possible tests."""

    @staticmethod
    def get(board_config: BoardConfig, with_private: str) -> List[TestConfig]:
        """Return public and private test configs for the specified board."""
        public_tests = (
            []
            if with_private == PRIVATE_ONLY
            else AllTests.get_public_tests(board_config)
        )
        private_tests = (
            [] if with_private == PRIVATE_NO else AllTests.get_private_tests()
        )

        all_tests = public_tests + private_tests
        board_tests = list(
            filter(
                lambda e: (board_config.name not in e.exclude_boards), all_tests
            )
        )
        return board_tests

    @staticmethod
    def get_public_tests(board_config: BoardConfig) -> List[TestConfig]:
        """Return public test configs for the specified board."""
        tests = [
            TestConfig(
                test_name="production_app_test",
                finish_regexes=[RW_IMAGE_BOOTED_REGEX],
                imagetype_to_use=ImageType.RW,
                apptype_to_use=ApplicationType.PRODUCTION,
            ),
            TestConfig(test_name="abort"),
            TestConfig(test_name="aes"),
            TestConfig(test_name="always_memset"),
            TestConfig(test_name="benchmark"),
            TestConfig(test_name="boringssl_crypto"),
            TestConfig(test_name="cortexm_fpu"),
            TestConfig(test_name="crc"),
            TestConfig(test_name="exception"),
            TestConfig(
                test_name="flash_physical",
                imagetype_to_use=ImageType.RO,
                toggle_power=True,
            ),
            TestConfig(
                test_name="flash_write_protect",
                imagetype_to_use=ImageType.RO,
                toggle_power=True,
                enable_hw_write_protect=True,
                timeout_secs=40,
            ),
            # TODO(b/274162810): Re-enable test on bloonchipper when LTO is re-enabled.
            TestConfig(
                test_name="fpsensor_auth_crypto_stateful",
                exclude_boards=[BLOONCHIPPER],
            ),
            TestConfig(test_name="fpsensor_auth_crypto_stateless"),
            TestConfig(test_name="fpsensor_hw"),
            TestConfig(
                config_name="fpsensor_spi_ro",
                test_name="fpsensor",
                imagetype_to_use=ImageType.RO,
                test_args=["spi"],
            ),
            TestConfig(
                config_name="fpsensor_spi_rw",
                test_name="fpsensor",
                test_args=["spi"],
            ),
            TestConfig(
                config_name="fpsensor_uart_ro",
                test_name="fpsensor",
                imagetype_to_use=ImageType.RO,
                test_args=["uart"],
            ),
            TestConfig(
                config_name="fpsensor_uart_rw",
                test_name="fpsensor",
                test_args=["uart"],
            ),
            TestConfig(test_name="ftrapv"),
            TestConfig(
                test_name="libc_printf",
                finish_regexes=[PRINTF_CALLED_REGEX],
            ),
            TestConfig(test_name="global_initialization"),
            TestConfig(test_name="libcxx"),
            TestConfig(test_name="malloc", imagetype_to_use=ImageType.RO),
            TestConfig(
                config_name="mpu_ro",
                test_name="mpu",
                imagetype_to_use=ImageType.RO,
                finish_regexes=[board_config.mpu_regex],
            ),
            TestConfig(
                config_name="mpu_rw",
                test_name="mpu",
                finish_regexes=[board_config.mpu_regex],
            ),
            TestConfig(test_name="mutex"),
            TestConfig(test_name="panic"),
            TestConfig(test_name="pingpong"),
            TestConfig(test_name="printf"),
            TestConfig(test_name="queue"),
            TestConfig(test_name="rng_benchmark"),
            TestConfig(
                config_name="rollback_region0",
                test_name="rollback",
                finish_regexes=[board_config.rollback_region0_regex],
                test_args=["region0"],
            ),
            TestConfig(
                config_name="rollback_region1",
                test_name="rollback",
                finish_regexes=[board_config.rollback_region1_regex],
                test_args=["region1"],
            ),
            TestConfig(
                test_name="rollback_entropy", imagetype_to_use=ImageType.RO
            ),
            TestConfig(test_name="rtc"),
            TestConfig(test_name="sbrk", imagetype_to_use=ImageType.RO),
            TestConfig(test_name="sha256"),
            TestConfig(test_name="sha256_unrolled"),
            TestConfig(test_name="static_if"),
            TestConfig(test_name="stdlib"),
            TestConfig(test_name="std_vector"),
            TestConfig(test_name="stm32f_rtc", exclude_boards=[DARTMONKEY]),
            TestConfig(
                config_name="system_is_locked_wp_on",
                test_name="system_is_locked",
                test_args=["wp_on"],
                toggle_power=True,
                enable_hw_write_protect=True,
            ),
            TestConfig(
                config_name="system_is_locked_wp_off",
                test_name="system_is_locked",
                test_args=["wp_off"],
                toggle_power=True,
                enable_hw_write_protect=False,
            ),
            TestConfig(test_name="timer"),
            TestConfig(test_name="timer_dos"),
            TestConfig(test_name="tpm_seed_clear"),
            TestConfig(test_name="unaligned_access"),
            TestConfig(test_name="unaligned_access_benchmark"),
            TestConfig(test_name="utils", timeout_secs=10),
            TestConfig(test_name="utils_str"),
        ]

        # Run unaligned access tests for all boards and RO versions.
        for variant_name, variant_info in board_config.variants.items():
            tests.append(
                TestConfig(
                    config_name="unaligned_access_" + variant_name,
                    test_name="unaligned_access",
                    fail_regexes=[
                        SINGLE_CHECK_FAILED_REGEX,
                        ALL_TESTS_FAILED_REGEX,
                    ],
                    ro_image=variant_info.get("ro_image_path"),
                    build_board=variant_info.get("build_board"),
                )
            )

        # Run panic data test for all boards and RO versions.
        for variant_name, variant_info in board_config.variants.items():
            tests.append(
                TestConfig(
                    config_name="panic_data_" + variant_name,
                    test_name="panic_data",
                    fail_regexes=[
                        SINGLE_CHECK_FAILED_REGEX,
                        ALL_TESTS_FAILED_REGEX,
                    ],
                    ro_image=variant_info.get("ro_image_path"),
                    build_board=variant_info.get("build_board"),
                )
            )

        return tests

    @staticmethod
    def get_private_tests() -> List[TestConfig]:
        """Return private test configs for the specified board, if available."""
        tests = []
        try:
            current_dir = os.path.dirname(__file__)
            private_dir = os.path.join(current_dir, os.pardir, "private/test")
            have_private = os.path.isdir(private_dir)
            if not have_private:
                return []
            sys.path.append(private_dir)
            import private_tests  # pylint: disable=import-error,import-outside-toplevel

            for test_args in private_tests.tests:
                tests.append(TestConfig(**test_args))
        # Catch all exceptions to avoid disruptions in public repo
        except BaseException as exception:  # pylint: disable=broad-except
            logging.debug(
                "Failed to get list of private tests: %s", str(exception)
            )
            logging.debug("Ignore error and continue.")
            return []
        return tests


BLOONCHIPPER_CONFIG = BoardConfig(
    name=BLOONCHIPPER,
    servo_uart_name="raw_fpmcu_console_uart_pty",
    servo_power_enable="fpmcu_pp3300",
    reboot_timeout=1.0,
    rollback_region0_regex=DATA_ACCESS_VIOLATION_8020000_REGEX,
    rollback_region1_regex=DATA_ACCESS_VIOLATION_8040000_REGEX,
    mpu_regex=DATA_ACCESS_VIOLATION_20000000_REGEX,
    variants={
        "bloonchipper_v2.0.4277": {
            "ro_image_path": BLOONCHIPPER_V4277_IMAGE_PATH
        },
        "bloonchipper_v2.0.5938": {
            "ro_image_path": BLOONCHIPPER_V5938_IMAGE_PATH
        },
    },
)

DARTMONKEY_CONFIG = BoardConfig(
    name=DARTMONKEY,
    servo_uart_name="raw_fpmcu_console_uart_pty",
    servo_power_enable="fpmcu_pp3300",
    reboot_timeout=1.0,
    rollback_region0_regex=DATA_ACCESS_VIOLATION_80C0000_REGEX,
    rollback_region1_regex=DATA_ACCESS_VIOLATION_80E0000_REGEX,
    mpu_regex=DATA_ACCESS_VIOLATION_24000000_REGEX,
    # For dartmonkey board, run panic data test also on nocturne_fp and
    # nami_fp boards with appropriate RO image.
    variants={
        "dartmonkey_v2.0.2887": {"ro_image_path": DARTMONKEY_IMAGE_PATH},
        "nocturne_fp_v2.2.64": {
            "ro_image_path": NOCTURNE_FP_IMAGE_PATH,
            "build_board": "nocturne_fp",
        },
        "nami_fp_v2.2.144": {
            "ro_image_path": NAMI_FP_IMAGE_PATH,
            "build_board": "nami_fp",
        },
    },
)

HELIPILOT_CONFIG = BoardConfig(
    name=HELIPILOT,
    servo_uart_name="raw_fpmcu_console_uart_pty",
    servo_power_enable="fpmcu_pp3300",
    reboot_timeout=1.5,
    rollback_region0_regex=DATA_ACCESS_VIOLATION_64020000_REGEX,
    rollback_region1_regex=DATA_ACCESS_VIOLATION_64040000_REGEX,
    mpu_regex=DATA_ACCESS_VIOLATION_20000000_REGEX,
    variants={},
)

BOARD_CONFIGS = {
    "bloonchipper": BLOONCHIPPER_CONFIG,
    "dartmonkey": DARTMONKEY_CONFIG,
    "helipilot": HELIPILOT_CONFIG,
}


def read_file_gsutil(path: str) -> bytes:
    """Get data from bucket, using gsutil tool"""
    cmd = ["gsutil", "cat", path]

    logging.debug('Running command: "%s"', " ".join(cmd))
    gsutil = subprocess.run(cmd, stdout=subprocess.PIPE, check=False)
    gsutil.check_returncode()

    return gsutil.stdout


def find_section_offset_size(section: str, image: bytes) -> Tuple[int, int]:
    """Get offset and size of the section in image"""
    areas = fmap.fmap_decode(image)["areas"]
    area = next(area for area in areas if area["name"] == section)
    return area["offset"], area["size"]


def read_section(src: bytes, section: str) -> bytes:
    """Read FMAP section content into byte array"""
    (src_start, src_size) = find_section_offset_size(section, src)
    src_end = src_start + src_size
    return src[src_start:src_end]


def write_section(data: bytes, image: bytearray, section: str):
    """Replace the specified section in image with the contents of data"""
    (section_start, section_size) = find_section_offset_size(section, image)

    if section_size < len(data):
        raise ValueError(section + " section size is not enough to store data")

    section_end = section_start + section_size
    filling = bytes([0xFF for _ in range(section_size - len(data))])

    image[section_start:section_end] = data + filling


def copy_section(src: bytes, dst: bytearray, section: str):
    """Copy section from src image to dst image"""
    (src_start, src_size) = find_section_offset_size(section, src)
    (dst_start, dst_size) = find_section_offset_size(section, dst)

    if dst_size < src_size:
        raise ValueError(
            "Section " + section + " from source image has "
            "greater size than the section in destination image"
        )

    src_end = src_start + src_size
    dst_end = dst_start + dst_size
    filling = bytes([0xFF for _ in range(dst_size - src_size)])

    dst[dst_start:dst_end] = src[src_start:src_end] + filling


def replace_ro(image: bytearray, ro_section: bytes):
    """Replace RO in image with provided one"""
    # Backup RO public key since its private part was used to sign RW.
    ro_pubkey = read_section(image, "KEY_RO")

    # Copy RO part of the firmware to the image. Please note that RO public key
    # is copied too since EC_RO area includes KEY_RO area.
    copy_section(ro_section, image, "EC_RO")

    # Restore RO public key.
    write_section(ro_pubkey, image, "KEY_RO")


def get_console(board_config: BoardConfig) -> Optional[str]:
    """Get the name of the console for a given board."""
    cmd = [
        "dut-control",
        board_config.servo_uart_name,
    ]
    logging.debug('Running command: "%s"', " ".join(cmd))

    with subprocess.Popen(cmd, stdout=subprocess.PIPE) as proc:
        for line in io.TextIOWrapper(proc.stdout):  # type: ignore[arg-type]
            logging.debug(line)
            pty = line.split(":")
            if len(pty) == 2 and pty[0] == board_config.servo_uart_name:
                return pty[1].strip()

    return None


class TestHarnes:
    @abstractmethod
    def flash(self, image: pathlib.Path) -> None:
        pass

    @abstractmethod
    def console(self) -> io.IOBase:
        pass

    @abstractmethod
    def start_test(self, args: List[str] = []) -> None:
        pass

    @abstractmethod
    def reboot_ro(self) -> None:
        pass

    @abstractmethod
    def power(self, power_on: bool) -> None:
        pass

    @abstractmethod
    def hw_write_protect(self, enable: bool) -> None:
        pass


class TestServo(TestHarnes):
    def flash(self, image: pathlib.Path) -> None:
        pass

    def console(self) -> io.IOBase:
        # pylint: disable-next=consider-using-with
        # console_file = open(get_console(board_config), "wb+", buffering=0)
        # return console
        pass

    def start_test(self, args: List[str] = []) -> None:
        pass

    def reboot_ro(self) -> None:
        pass

    def power(self, power_on: bool) -> None:
        pass

    def hw_write_protect(self, enable: bool) -> None:
        pass


class TestOnDut(TestHarnes):
    _SSH_TESTING_RSA_KEY = (
        pathlib.Path.home() / "chromiumos/chromite/ssh_keys/testing_rsa"
    )
    _REMOTE_TEST_BIN = pathlib.Path("/tmp/test.bin")

    def __init__(self, dut_host: str, dut_port: int) -> None:
        self._dut_host = dut_host
        self._dut_port = dut_port
        self._ssh_identity = tempfile.NamedTemporaryFile()

        self._console_server: Optional[subprocess.Popen] = None
        self._console_sock: Optional[socket.socket] = None
        self._console_sock_file: Optional[socket.SocketIO] = None

        # Copy testing_rsa to a place where we can change permission.
        # with self._SSH_TESTING_RSA_KEY.open('rb') as f:
        # self._ssh_identity.write(f.read())
        self._ssh_identity.write(self._SSH_TESTING_RSA_KEY.read_bytes())
        self._ssh_identity.flush()
        # self._ssh_identity.close() # will delete

        self._ssh_opts = [
            "-i",
            self._ssh_identity.name,
            "-oUserKnownHostsFile=/dev/null",
            "-oStrictHostKeyChecking=no",
            "-oNumberOfPasswordPrompts=0",
        ]

        # rsa_key_copy = pathlib.Path('/tmp/testing_rsa')
        # shutil.copy(SSH_TESTING_RSA_KEY, rsa_key_copy)
        # os.system(f'chmod 600 {self._ssh_identity.name}')
        os.system(f"ls -alh {self._SSH_TESTING_RSA_KEY}")
        os.system(f"ls -alh {self._ssh_identity.name}")
        # print('##################')
        # os.system(f'cat {self._ssh_identity.name}')
        # print('##################')

        # https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:chromite/lib/remote_access.py;l=226
        # ssh_opts = f'-i {rsa_key_copy} -oUserKnownHostsFile=/dev/null -oStrictHostKeyChecking=no -oNumberOfPasswordPrompts=0'
        # remote_test_bin = '/tmp/test.bin'

    def _start_cmd(self, cmd: List[str]) -> subprocess.Popen:
        """Start a long running command and return open context."""
        print(f'# Start {" ".join(cmd)}.')
        process = subprocess.Popen(cmd)
        return process

    def _run_cmd(self, cmd: List[str]) -> int:
        """Run a single command to completion."""
        print(f'# Run {" ".join(cmd)}.')
        sys.stdout.flush()
        p = subprocess.run(cmd)  # pylint: disable=subprocess-run-check
        return p.returncode

    def _prepare_ssh_cmd(
        self, remote_cmd: List[str], extra_ssh_opts: List[str] = []
    ) -> List[str]:
        cmd = ["ssh"]
        cmd += self._ssh_opts
        cmd += extra_ssh_opts
        cmd += ["-p", str(self._dut_port), self._dut_host]
        cmd += ["--"]
        cmd += remote_cmd
        return cmd

    def _run_ssh_cmd(
        self, remote_cmd: List[str], extra_ssh_opts: List[str] = []
    ) -> int:
        cmd = self._prepare_ssh_cmd(remote_cmd, extra_ssh_opts)
        return self._run_cmd(cmd)

    def _transfer_image(self, image: pathlib.Path) -> int:
        cmd = ["scp"]
        cmd += self._ssh_opts
        cmd += ["-P", str(self._dut_port)]
        cmd += [str(image), f"{self._dut_host}:{self._REMOTE_TEST_BIN}"]
        return self._run_cmd(cmd)

    def _console_server_start(self):
        # Stop timberslide on DUT.
        remote_cmd = [
            "stop",
            "timberslide",
            "LOG_PATH=/sys/kernel/debug/cros_fp/console_log",
        ]
        self._run_ssh_cmd(remote_cmd)

        self._run_ssh_cmd(["killall", "socat"])

        # Start socat on DUT.
        print(f"# Starting console server {self._dut_host}:{self._dut_port}")
        extra_ssh_opts = ["-L10000:localhost:10000"]
        remote_cmd = [
            "socat",
            "/sys/kernel/debug/cros_fp/console_log",
            "tcp4-listen:10000,reuseaddr",
        ]
        cmd = self._prepare_ssh_cmd(remote_cmd, extra_ssh_opts)
        self._console_server = self._start_cmd(cmd)
        # We are leaving knowing that the server may not be up and ready to
        # receive connections.

    def _console_server_stop(self):
        print(f"# Stopping console server")
        if self._console_server:
            print("# Sending signal")
            self._console_server.send_signal(signal.SIGINT)
            self._console_server.terminate()
            time.sleep(1)
            print("# Launching killall")
            self._run_ssh_cmd(["killall", "socat"])
            self._console_server = None

    def _console_connect(self):
        print(f"# Connecting to console {self._dut_host}:{10000}")

        # The server may not have started yet. We could either wait 2 seconds
        # in the server start routine or poll until it is up.
        attempts = 5
        while attempts:
            print(f"# Attempts remaining {attempts}.")
            try:
                self._console_sock = socket.socket(
                    socket.AF_INET, socket.SOCK_STREAM
                )
                self._console_sock.connect((self._dut_host, 10000))
                break
            except ConnectionRefusedError:
                self._console_sock = None
                attempts -= 1
                time.sleep(1)

        if not self._console_sock:
            raise Exception("Connection refused too many times.")
        print(f"# Connection successfull")
        self._console_sock_file = self._console_sock.makefile(
            mode="rwb", buffering=0
        )

    def _console_disconnect(self):
        print(f"# Disconnecing console socket")
        if self._console_sock_file:
            self._console_sock_file.close()
            self._console_sock_file = None
        if self._console_sock:
            self._console_sock.close()
            self._console_sock = None

    def __del__(self):
        self._console_disconnect()
        self._console_server_stop()

    def flash(self, image: pathlib.Path) -> None:
        print(f"# Flashing {image}")
        self._console_disconnect()
        self._console_server_stop()
        self.hw_write_protect(False)
        ret = self._transfer_image(image)
        if ret != 0:
            raise Exception(
                f"Error transfering image to DUT. Return code {ret}."
            )
        ret = self._run_ssh_cmd(["flash_fp_mcu", str(self._REMOTE_TEST_BIN)])
        if ret != 0:
            raise Exception(f"Error flashing image to DUT. Return code {ret}.")
        time.sleep(2)

    def console(self) -> io.IOBase:
        print("# Console")
        if not self._console_server:
            self._console_server_start()
        if not self._console_sock_file:
            self._console_connect()
        return self._console_sock_file

    def start_test(self, args: List[str] = []) -> None:
        print(f"# Starting test.")
        # self._run_ssh_cmd(["ectool", "--name=cros_fp", "fpstats"])
        self._run_ssh_cmd(["ectool", "--name=cros_fp", "hello"])

    def reboot_ro(self) -> None:
        print("# Reboot to RO")
        self._run_ssh_cmd(["ectool", "--name=cros_fp", "reboot_ec"])
        self._run_ssh_cmd(
            ["ectool", "--name=cros_fp", "rwsig", "action", "abort"]
        )
        self._run_ssh_cmd(
            ["ectool", "--name=cros_fp", "rwsig", "action", "abort"]
        )
        self._run_ssh_cmd(
            ["ectool", "--name=cros_fp", "rwsig", "action", "abort"]
        )
        time.sleep(1)
        self._run_ssh_cmd(
            ["ectool", "--name=cros_fp", "version"]
        )
        time.sleep(1)

    def power(self, power_on: bool) -> None:
        print(f"# Power {power_on}")

        # Brya
        GPIO_CHIP = "gpiochip664"
        PWN_EN_NUM = 826 - 0

        EXPORT_CMD = "/sys/class/gpio/export"
        DIRECTION_CMD = f"/sys/class/gpio/gpio{PWN_EN_NUM}/direction"
        VALUE_CMD = f"/sys/class/gpio/gpio{PWN_EN_NUM}/value"

        # Export
        self._run_ssh_cmd(["echo", str(PWN_EN_NUM), f">{EXPORT_CMD}"])
        self._run_ssh_cmd(["echo", "out", f">{DIRECTION_CMD}"])
        self._run_ssh_cmd(["echo", str(1 if power_on else 0), f">{VALUE_CMD}"])
        pass

    def hw_write_protect(self, enable: bool) -> None:
        print(f"# HW WP {enable}")
        state = "force_on" if enable else "force_off"
        self._run_cmd(
            [
                "dut-control",
                f"fw_wp_state:{state}",
            ]
        )


harness = TestOnDut("localhost", 2222)


def power(board_config: BoardConfig, power_on: bool) -> None:
    """Turn power to board on/off."""
    # if power_on:
    #     state = "pp3300"
    # else:
    #     state = "off"

    # cmd = [
    #     "dut-control",
    #     board_config.servo_power_enable + ":" + state,
    # ]
    # logging.debug('Running command: "%s"', " ".join(cmd))
    # subprocess.run(cmd, check=False).check_returncode()
    harness.power(power_on)


def power_cycle(board_config: BoardConfig) -> None:
    """power_cycle the boards."""
    logging.debug("power_cycling board")
    power(board_config, power_on=False)
    time.sleep(board_config.reboot_timeout)
    power(board_config, power_on=True)


def hw_write_protect(enable: bool) -> None:
    """Enable/disable hardware write protect."""
    # if enable:
    #     state = "force_on"
    # else:
    #     state = "force_off"

    # cmd = [
    #     "dut-control",
    #     "fw_wp_state:" + state,
    # ]
    # logging.debug('Running command: "%s"', " ".join(cmd))
    # subprocess.run(cmd, check=False).check_returncode()
    harness.hw_write_protect(enable)


def build(
    test_name: str, board_name: str, compiler: str, app_type: ApplicationType
) -> None:
    """Build specified test for specified board."""
    cmd = ["make"]

    if compiler == CLANG:
        cmd = cmd + ["CC=arm-none-eabi-clang"]

    cmd = cmd + [
        "BOARD=" + board_name,
        "-j",
    ]

    # If the image type is a test image, then apply test- prefix to the target name
    if app_type == ApplicationType.TEST:
        cmd = cmd + [
            "test-" + test_name,
        ]

    logging.debug('Running command: "%s"', " ".join(cmd))
    subprocess.run(cmd, check=False).check_returncode()


def flash(
    image_path: str, board: str, flasher: str, remote_ip: str, remote_port: int
) -> bool:
    """Flash specified test to specified board."""
    logging.info("Flashing test")

    cmd = []
    if flasher == JTRACE:
        cmd.append(JTRACE_FLASH_SCRIPT)
        if remote_ip:
            cmd.extend(["--remote", remote_ip + ":" + str(remote_port)])
    elif flasher == SERVO_MICRO:
        cmd.append(SERVO_MICRO_FLASH_SCRIPT)
    elif flasher == FLASH_FP_MCU:
        print("# Using flash_fp_mcu over ssh")
        cmd.append(FLASH_FP_MCU_FLASH_SCRIPT)
    else:
        logging.error('Unknown flasher: "%s"', flasher)
        return False
    cmd.extend(
        [
            "--board",
            board,
            "--image",
            image_path,
        ]
    )
    # logging.debug('Running command: "%s"', " ".join(cmd))
    # completed_process = subprocess.run(cmd, check=False)
    # return completed_process.returncode == 0
    harness.flash(pathlib.Path(image_path))
    return True


def patch_image(test: TestConfig, image_path: str):
    """Replace RO part of the firmware with provided one."""
    with open(image_path, "rb+") as image_file:
        image = bytearray(image_file.read())
        ro_section = read_file_gsutil(test.ro_image)
        replace_ro(image, ro_section)
        image_file.seek(0)
        image_file.write(image)
        image_file.truncate()


def readline(
    executor: ThreadPoolExecutor, file: BinaryIO, timeout_secs: int
) -> Optional[bytes]:
    """Read a line with timeout."""
    future = executor.submit(file.readline)
    try:
        return future.result(timeout_secs)
    except concurrent.futures.TimeoutError:
        return None


def readlines_until_timeout(
    executor, file: BinaryIO, timeout_secs: int
) -> List[bytes]:
    """Continuously read lines for timeout_secs."""
    lines: List[bytes] = []
    while True:
        line = readline(executor, file, timeout_secs)
        if not line:
            return lines
        lines.append(line)


def process_console_output_line(line: bytes, test: TestConfig):
    """Parse console output line and update test pass/fail counters."""
    try:
        line_str = line.decode()

        if SINGLE_CHECK_PASSED_REGEX.match(line_str):
            test.num_passes += 1

        for regex in test.fail_regexes:
            if regex.match(line_str):
                test.num_fails += 1
                break

        return line_str
    except UnicodeDecodeError:
        # Sometimes we get non-unicode from the console (e.g., when the
        # board reboots.) Not much we can do in this case, so we'll just
        # ignore it.
        return None


def run_test(
    test: TestConfig,
    build_board: str,
    console: io.FileIO,
    reboot_timeout: float,
    executor: ThreadPoolExecutor,
) -> bool:
    """Run specified test."""
    start = time.time()

    # Wait for boot to finish
    time.sleep(reboot_timeout)
    # console.write("\n".encode())
    if test.imagetype_to_use == ImageType.RO:
        # console.write("reboot ro\n".encode())
        harness.reboot_ro()
        time.sleep(reboot_timeout)

    # Skip runtest if using standard app type
    if test.apptype_to_use != ApplicationType.PRODUCTION:
        # test_cmd = "runtest " + " ".join(test.test_args) + "\n"
        # console.write(test_cmd.encode())
        harness.start_test(test.test_args)

    logging.debug("Calling pre-test callback")
    if not test.pre_test_callback(build_board):
        logging.error("pre-test callback failed, aborting")
        return False

    while True:
        # console.flush()
        line = readline(executor, console, 1)
        if not line:
            now = time.time()
            if now - start > test.timeout_secs:
                logging.debug("Test timed out")
                return False
            continue

        test.logs.append(line)
        # Look for test_print_result() output (success or failure)
        line_str = process_console_output_line(line, test)
        if line_str is None:
            # Sometimes we get non-unicode from the console (e.g., when the
            # board reboots.) Not much we can do in this case, so we'll just
            # ignore it and print log the line as is.
            logging.debug(line)
            continue

        logging.debug(line_str.rstrip())

        for finish_re in test.finish_regexes:
            if finish_re.match(line_str):
                # flush read the remaining
                lines = readlines_until_timeout(executor, console, 1)
                logging.debug(lines)
                test.logs.append(lines)

                for line in lines:
                    process_console_output_line(line, test)

                logging.debug("Calling post-test callback")
                post_cb_passed = test.post_test_callback(build_board)
                return test.num_fails == 0 and post_cb_passed


def get_test_list(
    config: BoardConfig, test_args, with_private: str
) -> List[TestConfig]:
    """Get a list of tests to run."""
    if test_args == "all":
        return AllTests.get(config, with_private)

    test_list = []
    for test in test_args:
        logging.debug("test: %s", test)
        test_regex = re.compile(test)
        tests = [
            test
            for test in AllTests.get(config, with_private)
            if test_regex.fullmatch(test.config_name)
        ]
        if not tests:
            logging.error(
                'Test "%s" is either not configured or not supported on board "%s"',
                test,
                config.name,
            )
            sys.exit(1)
        test_list += tests

    return test_list


def flash_and_run_test(
    test: TestConfig,
    board_config: BoardConfig,
    args: argparse.Namespace,
    executor,
) -> bool:
    """Run a single test using the test and board configuration specified"""
    build_board = args.board
    # If test provides this information, build image for board specified
    # by test.
    if test.build_board is not None:
        build_board = test.build_board

    # attempt to build test binary, reporting a test failure on error
    try:
        build(test.test_name, build_board, args.compiler, test.apptype_to_use)
    except Exception as exception:  # pylint: disable=broad-except
        logging.error("failed to build %s: %s", test.test_name, exception)
        return False

    if test.apptype_to_use == ApplicationType.PRODUCTION:
        image_path = os.path.join(EC_DIR, "build", build_board, "ec.bin")
    else:
        image_path = os.path.join(
            EC_DIR,
            "build",
            build_board,
            test.test_name,
            test.test_name + ".bin",
        )
    logging.debug("image_path: %s", image_path)

    if test.ro_image is not None:
        try:
            patch_image(test, image_path)
        except Exception as exception:  # pylint: disable=broad-except
            logging.warning(
                "An exception occurred while patching image: %s", exception
            )
            return False

    # flash test binary
    # TODO(b/158327221): First attempt to flash fails after
    #  flash_write_protect test is run; works after second attempt.
    flash_succeeded = False
    for i in range(0, test.num_flash_attempts):
        logging.debug("Flash attempt %d", i + 1)
        if flash(
            image_path, args.board, args.flasher, args.remote, args.jlink_port
        ):
            flash_succeeded = True
            break
        time.sleep(board_config.reboot_timeout)

    if not flash_succeeded:
        logging.debug(
            "Flashing failed after max attempts: %d", test.num_flash_attempts
        )
        return False

    if test.toggle_power:
        power_cycle(board_config)

    hw_write_protect(test.enable_hw_write_protect)

    # run the test
    logging.info('Running test: "%s"', test.config_name)

    with ExitStack() as stack:
        # if args.remote and args.console_port:
        #     console_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        #     console_socket.connect((args.remote, args.console_port))
        #     console = stack.enter_context(
        #         console_socket.makefile(mode="rwb", buffering=0)
        #     )
        # else:
        #     # pylint: disable-next=consider-using-with
        #     console_file = open(get_console(board_config), "wb+", buffering=0)
        #     console = stack.enter_context(console_file)
        console = stack.enter_context(harness.console())

        return run_test(
            test,
            build_board,
            console,
            board_config.reboot_timeout,
            executor=executor,
        )


def parse_remote_arg(remote: str) -> str:
    """Convert the 'remote' input argument to IP address, if available."""
    if not remote:
        return ""

    try:
        ip_addr = socket.gethostbyname(remote)
        return ip_addr
    except socket.gaierror:
        logging.error('Failed to resolve host "%s".', remote)
        sys.exit(1)


def validate_args_combination(args: argparse.Namespace):
    """Check that the current combination of arguments is supported.

    Not all combinations of command line arguments are valid or currently
    supported. If tests can't be executed, print and error message and exit.
    """
    if args.jlink_port and not args.flasher == JTRACE:
        logging.error("jlink_port specified, but flasher is not set to J-Link.")
        sys.exit(1)

    if args.remote and not (args.jlink_port or args.console_port):
        logging.error(
            "jlink_port or console_port must be specified when using "
            "the remote option."
        )
        sys.exit(1)

    if (args.jlink_port or args.console_port) and not args.remote:
        logging.error(
            "The remote option must be specified when using the "
            "jlink_port or console_port options."
        )
        sys.exit(1)

    if args.remote and args.flasher == SERVO_MICRO:
        logging.error(
            "The remote option is not supported when flashing with servo "
            "micro. Use J-Link instead or flash with a local servo micro."
        )
        sys.exit(1)

    if args.board not in BOARD_CONFIGS:
        logging.error('Unable to find a config for board: "%s"', args.board)
        sys.exit(1)


def main():
    """Run unit tests on device and displays the results."""
    parser = argparse.ArgumentParser()

    default_board = "bloonchipper"
    parser.add_argument(
        "--board",
        "-b",
        help="Board (default: " + default_board + ")",
        default=default_board,
    )

    default_tests = "all"
    parser.add_argument(
        "--tests",
        "-t",
        nargs="+",
        help="Tests (default: " + default_tests + ")",
        default=default_tests,
    )

    parser.add_argument(
        "--print_tests",
        action="store_true",
        help="Print selected tests and exit",
    )

    log_level_choices = ["DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL"]
    parser.add_argument(
        "--log_level", "-l", choices=log_level_choices, default="DEBUG"
    )

    flasher_choices = [SERVO_MICRO, JTRACE]
    parser.add_argument(
        "--flasher", "-f", choices=flasher_choices, default=JTRACE
    )

    compiler_options = [GCC, CLANG]
    parser.add_argument(
        "--compiler", "-c", choices=compiler_options, default=GCC
    )

    # This might be expanded to serve as a "remote" for flash_ec also, so
    # we will leave it generic.
    parser.add_argument(
        "--remote",
        "-n",
        help="The remote host connected to one or both of: J-Link and Servo.",
        type=parse_remote_arg,
    )

    parser.add_argument(
        "--jlink_port",
        "-j",
        type=int,
        help="The port to use when connecting to JLink.",
    )
    parser.add_argument(
        "--console_port",
        "-p",
        type=int,
        help="The port connected to the FPMCU console.",
    )

    with_private_choices = [PRIVATE_YES, PRIVATE_NO, PRIVATE_ONLY]
    parser.add_argument(
        "--with_private", choices=with_private_choices, default=PRIVATE_YES
    )

    args = parser.parse_args()
    logging.basicConfig(
        format="%(levelname)s:%(message)s", level=args.log_level
    )
    validate_args_combination(args)

    board_config = BOARD_CONFIGS[args.board]
    test_list = get_test_list(board_config, args.tests, args.with_private)
    if args.print_tests:
        print(" ".join(test.config_name for test in test_list))
        return 0
    logging.debug("Running tests: %s", [test.config_name for test in test_list])

    with ThreadPoolExecutor(max_workers=1) as executor:
        for test in test_list:
            test.passed = flash_and_run_test(test, board_config, args, executor)

        colorama.init()
        exit_code = 0
        for test in test_list:
            # print results
            print('Test "' + test.config_name + '": ', end="")
            if test.passed:
                print(colorama.Fore.GREEN + "PASSED")
            else:
                print(colorama.Fore.RED + "FAILED")
                exit_code = 1

            print(colorama.Style.RESET_ALL)

    sys.exit(exit_code)


if __name__ == "__main__":
    sys.exit(main())
