# Copyright 2021 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import subprocess
import os

import zmake.util as util


def _get_num_commits(repo, vcsid):
    """Get the number of commits that have been made in a Git repository.

    Args:
        repo: The path to the git repo.

    Returns:
        An integer, the number of commits that have been made.
    """
    try:
        FNULL = open(os.devnull, 'w')
        result = subprocess.run(['git', '-C', repo, 'rev-list', 'HEAD', '--count'],
                                check=True, stdout=subprocess.PIPE, stderr=FNULL,
                                encoding='utf-8')
        commits = result.stdout
    except:
        commits, hash = vcsid.split('-')

    return int(commits)


def _get_revision(repo, vcsid):
    """Get the index's current revision of a Git repo.

    Args:
        repo: The path to the git repo.

    Returns:
        A string, of the current revision.
    """
    try:
        FNULL = open(os.devnull, 'w')
        result = subprocess.run(['git', '-C', repo, 'log', '-n1', '--format=%H'],
                                check=True, stdout=subprocess.PIPE, stderr=FNULL,
                                encoding='utf-8')
        hash = result.stdout
    except:
        commits, hash = vcsid.split('-')

    return hash


def get_version_string(project, zephyr_base, modules, static=False):
    """Get the version string associated with a build.

    Args:
        project: a zmake.project.Project object
        zephyr_base: the path to the zephyr directory
        modules: a dictionary mapping module names to module paths
        static: if set, create a version string not dependent on git
            commits, thus allowing binaries to be compared between two
            commits.

    Returns:
        A version string which can be placed in FRID, FWID, or used in
        the build for the OS.
    """
    major_version, minor_version, *_ = util.read_zephyr_version(zephyr_base)
    project_id = project.project_dir.parts[-1]
    num_commits = 0

    # Fall back the VCSID provided by the packaging system,
    try:
        vcsid = os.environ['VCSID']
    except:
        vcsid = "9999-unknown"

    if static:
        vcs_hashes = 'STATIC'
    else:
        repos = {
            'os': zephyr_base,
            **modules,
        }

        for repo in repos.values():
            num_commits += _get_num_commits(repo, vcsid)

        vcs_hashes = ','.join(
            '{}:{}'.format(name, _get_revision(repo, vcsid)[:6])
            for name, repo in sorted(repos.items()))

    return '{}_v{}.{}.{}-{}'.format(
        project_id, major_version, minor_version, num_commits, vcs_hashes)
