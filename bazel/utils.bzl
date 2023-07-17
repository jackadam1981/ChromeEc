# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("@bazel_skylib//lib:shell.bzl", "shell")

"""EC Bazel module containing shared utility functions."""

def gen_shell_wrapper(argv, env):
    """ Generates a bash script that can be used to create executables.

    Args:
    	argv: A sequence where the 0th element is a command.
    	env: A dictionary of environment variables to export

    Returns:
    	A string that represents a bash script.
    """
    script = "#!/bin/bash\n"
    for key, val in env.items():
        script += "export %s=%s\n" % (key, shell.quote(val))
    script += '%s "$@"\n' % " ".join([shell.quote(x) for x in argv])
    return script
