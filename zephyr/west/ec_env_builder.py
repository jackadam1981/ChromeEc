# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" Extend EnvBuilder to create a virtual environment for EC Zephyr """

import argparse
import os
import os.path
from pathlib import Path
import platform
import shutil
from subprocess import PIPE
from subprocess import Popen
import sys
import tarfile
from threading import Thread
from urllib.parse import urlparse
from urllib.request import urlretrieve
import venv

import requests  # pylint: disable=import-error


class EcEnvBuilder(venv.EnvBuilder):
    """
    This builder installs setuptools and pip so that you can pip or
    easy_install other packages into the created virtual environment.

    :param nodist: If true, setuptools and pip are not installed into the
                   created virtual environment.
    :param nopip: If true, pip is not installed into the created
                  virtual environment.
    :param progress: If setuptools or pip are installed, the progress of the
                     installation can be monitored by passing a progress
                     callable. If specified, it is called with two
                     arguments: a string indicating some progress, and a
                     context indicating where the string is coming from.
                     The context argument can have one of three values:
                     'main', indicating that it is called from virtualize()
                     itself, and 'stdout' and 'stderr', which are obtained
                     by reading lines from the output streams of a subprocess
                     which is used to install the app.

                     If a callable is not specified, default progress
                     information is output to sys.stderr.
    """

    def __init__(self, *args, **kwargs):
        self.nodist = kwargs.pop("nodist", False)
        self.nopip = kwargs.pop("nopip", False)
        self.progress = kwargs.pop("progress", None)
        self.verbose = kwargs.pop("verbose", False)
        self.download_percent = 0
        super().__init__(*args, **kwargs)

    def post_setup(self, context):
        """
        Set up any packages which need to be pre-installed into the
        virtual environment being created.

        :param context: The information for the virtual environment
                        creation request being processed.
        """
        os.environ["VIRTUAL_ENV"] = context.env_dir

        if not self.nopip and not self.nodist:
            self.install_pip(context)

        self.setup_zephyr(context)
        self.setup_zephyr_tools(context)
        self.install_postactivate(context)

    @staticmethod
    def install_postactivate(context):
        """
        Modify virtual environment activate bash script to call postactivate.sh
        """
        path = Path(context.env_dir) / "bin" / "activate"
        src = f"{os.path.dirname(__file__)}/postactivate.sh"
        dst = f"{context.env_dir}/bin/postactivate.sh"
        shutil.copyfile(src, dst)

        with open(path, "a+") as activate_file:
            activate_file.write(f"source {os.path.relpath(dst, os.curdir)}\n")
            activate_file.close()

        args = f"source {os.path.relpath(path, os.curdir)}"
        print(f"Activate virtual env by running: '{args}'")

    def reader(self, stream, context):
        """
        Read lines from a subprocess' output stream and either pass to a progress
        callable (if specified) or write progress information to sys.stderr.
        """
        progress = self.progress
        while True:
            line = stream.readline()
            if not line:
                break
            if progress is not None:
                progress(line, context)
            else:
                if not self.verbose:
                    sys.stderr.write(".")
                else:
                    sys.stderr.write(line.decode("utf-8"))
                sys.stderr.flush()
        stream.close()

    def install_script(self, context, name, url):
        """
        Install script for pip installation
        :param context: The information for the virtual environment
                        creation request being processed.
        """
        _, _, path, _, _, _ = urlparse(url)
        filename = os.path.split(path)[-1]
        binpath = context.bin_path
        distpath = os.path.join(binpath, filename)
        # Download script into the virtual environment's binaries folder
        urlretrieve(url, distpath)
        progress = self.progress
        if self.verbose:
            term = "\n"
        else:
            term = ""
        if progress is not None:
            progress("Installing %s ...%s" % (name, term), "main")
        else:
            sys.stderr.write("Installing %s ...%s" % (name, term))
            sys.stderr.flush()
        # Install in the virtual environment
        args = [context.env_exe, filename]
        with Popen(args, stdout=PIPE, stderr=PIPE, cwd=binpath) as proc:
            thread1 = Thread(target=self.reader, args=(proc.stdout, "stdout"))
            thread1.start()
            thread2 = Thread(target=self.reader, args=(proc.stderr, "stderr"))
            thread2.start()
            proc.wait()
            thread1.join()
            thread2.join()
            if progress is not None:
                progress("done.", "main")
            else:
                sys.stderr.write("done.\n")
            # Clean up - no longer needed
            os.unlink(distpath)

    def install_pip(self, context):
        """
        Install pip in the virtual environment.

        :param context: The information for the virtual environment
                        creation request being processed.
        """
        url = "https://bootstrap.pypa.io/get-pip.py"
        self.install_script(context, "pip", url)

    def setup_zephyr(self, context):
        """
        Sets up Zephyr per instructions at
        https://docs.zephyrproject.org/latest/develop/getting_started/index.html

        :param context: The information for the virtual environment
                        creation request being processed.
        """

        zephyr_req = f"{os.getcwd()}/zephyr/scripts/requirements.txt"

        args = [context.env_exe, "-m", "pip", "install", "-r", zephyr_req]
        print(f"Setting up ZephyrSDK - {args}\n")
        with Popen(
            args, stdout=PIPE, stderr=PIPE, cwd=context.bin_path
        ) as proc:
            thread1 = Thread(target=self.reader, args=(proc.stdout, "stdout"))
            thread1.start()
            thread2 = Thread(target=self.reader, args=(proc.stderr, "stderr"))
            thread2.start()
            proc.wait()
            thread1.join()
            thread2.join()
        print("\nDone - Setting up ZephyrSDK")

    @staticmethod
    def get_zephyr_sdk_url():
        """
        Get URL to latest Zephyr SDK Tools

        """
        url = "https://api.github.com/repos/zephyrproject-rtos/sdk-ng/releases/latest"

        response = requests.get(url)
        name = response.json()["name"].lower().replace(" ", "-")
        arch = platform.machine()
        system = platform.system().lower()
        pattern = f"{name}_{system}-{arch}.tar.gz"
        # print(pattern)

        match = []
        for asset in response.json()["assets"]:
            if pattern in asset["browser_download_url"]:
                match.append(asset["browser_download_url"])

        return match[0]

    def show_progress(self, block_num, block_size, total_size):
        """
        Shows download progress
        """
        percent_complete = int(block_num * block_size * 100 / total_size)
        if percent_complete > self.download_percent:
            self.download_percent = percent_complete
            sys.stderr.write(".")
            sys.stderr.flush()

        if self.download_percent >= 100:
            self.download_percent = 0
            sys.stderr.write("\n")
            sys.stderr.flush()

    def setup_zephyr_tools(self, context):
        """
        Setup the Zephyr SDK tools

        :param context: The information for the virtual environment
                        creation request being processed.
        """
        tools_path = self.download_zephyr_tools()
        tar_file_name = tools_path.split("/")[-1]
        extract_name = tar_file_name.split("_")[0]
        extract_path = f"{os.getcwd()}/tools/"
        tools_setup = f"{extract_path}/{extract_name}/setup.sh"

        print(f"Extracting - {tar_file_name}")
        self.extract_zephyr_tools(tools_path, extract_path)

        args = [tools_setup, "-t", "all", "-c", "-h"]
        print(f"Setting up Zephyr SDK Tools - {args}")

        with Popen(
            args, stdout=PIPE, stderr=PIPE, cwd=context.bin_path
        ) as proc:
            thread1 = Thread(target=self.reader, args=(proc.stdout, "stdout"))
            thread1.start()
            thread2 = Thread(target=self.reader, args=(proc.stderr, "stderr"))
            thread2.start()
            proc.wait()
            thread1.join()
            thread2.join()

        print("\nDone Setting up Zephyr SDK Tools!\n")

    def download_zephyr_tools(self):
        """
        Download Zephyr SDK Tools
        """
        url = self.get_zephyr_sdk_url()
        file = url.split("/")[-1]
        path = f"{os.getcwd()}/tools/{file}"
        print(f"Downloading Zephyr SDK Tools: {url}\nTo: {path}")
        os.makedirs(os.path.dirname(path), exist_ok=True)
        urlretrieve(url, path, self.show_progress)

        return path

    @staticmethod
    def extract_zephyr_tools(tools_path, output_path):
        """
        Extract Zephyr SDK Tools
        """
        with tarfile.open(tools_path) as file:
            file.extractall(output_path)
            file.close()


def run_env_builder(args=None):
    """
    Create the virtual build environment
    """
    compatible = True
    if sys.version_info < (3, 3):
        compatible = False
    elif not hasattr(sys, "base_prefix"):
        compatible = False
    if not compatible:
        raise ValueError(
            "This script is only for use with " "Python 3.3 or later"
        )

    parser = argparse.ArgumentParser(
        prog=__name__,
        description="Creates virtual Python "
        "environments in one or "
        "more target "
        "directories.",
    )
    parser.add_argument(
        "dirs",
        metavar="ENV_DIR",
        nargs="+",
        help="A directory in which to create the " "virtual environment.",
    )
    parser.add_argument(
        "--no-setuptools",
        default=False,
        action="store_true",
        dest="nodist",
        help="Don't install setuptools or pip in the " "virtual environment.",
    )
    parser.add_argument(
        "--no-pip",
        default=False,
        action="store_true",
        dest="nopip",
        help="Don't install pip in the virtual " "environment.",
    )
    parser.add_argument(
        "--system-site-packages",
        default=False,
        action="store_true",
        dest="system_site",
        help="Give the virtual environment access to the "
        "system site-packages dir.",
    )
    if os.name == "nt":
        use_symlinks = False
    else:
        use_symlinks = True
    parser.add_argument(
        "--symlinks",
        default=use_symlinks,
        action="store_true",
        dest="symlinks",
        help="Try to use symlinks rather than copies, "
        "when symlinks are not the default for "
        "the platform.",
    )
    parser.add_argument(
        "--clear",
        default=False,
        action="store_true",
        dest="clear",
        help="Delete the contents of the "
        "virtual environment "
        "directory if it already "
        "exists, before virtual "
        "environment creation.",
    )
    parser.add_argument(
        "--upgrade",
        default=False,
        action="store_true",
        dest="upgrade",
        help="Upgrade the virtual "
        "environment directory to "
        "use this version of "
        "Python, assuming Python "
        "has been upgraded "
        "in-place.",
    )
    parser.add_argument(
        "--verbose",
        default=False,
        action="store_true",
        dest="verbose",
        help="Display the output "
        "from the scripts which "
        "install setuptools and pip.",
    )
    options = parser.parse_args(args)
    if options.upgrade and options.clear:
        raise ValueError("you cannot supply --upgrade and --clear together.")
    builder = EcEnvBuilder(
        system_site_packages=options.system_site,
        clear=options.clear,
        symlinks=options.symlinks,
        upgrade=options.upgrade,
        nodist=options.nodist,
        nopip=options.nopip,
        verbose=options.verbose,
    )
    for env in options.dirs:
        builder.create(env)


if __name__ == "__main__":
    RETURN_CODE = 1
    try:
        run_env_builder()
        RETURN_CODE = 0
    except Exception as ex:  # pylint: disable=broad-except
        print("Error: %s" % ex, file=sys.stderr)
    sys.exit(RETURN_CODE)
