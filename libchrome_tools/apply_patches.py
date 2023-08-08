#!/usr/bin/env python3
# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Tool to apply libchrome patches.
"""

import argparse
import glob
import logging
import os
from pathlib import Path
import stat
import subprocess

PREFIXES = [
    "long-term",
    "cherry-pick",
    "backward-compatibility",
    "forward-compatibility",
]


def apply_patch(patch: str, ebuild: bool):
    """Applying given patch."""
    if patch.endswith(".patch"):
        # "-C1" to be compatible with `patch` and to be more robust against
        # upstream changes.
        try:
            subprocess.check_call(
                [ "git", "apply", "-C1", patch ])
        except subprocess.CalledProcessError as err:
            if not ebuild:
                # Rerun (expected failure) to leave a 3-way merge marker.
                subprocess.check_call([ "git", "apply", "-C1", patch, "--3way"])
            else:
                raise RuntimeError(f"Failed to apply patch {patch}.") from err
    elif os.stat(patch).st_mode & stat.S_IXUSR != 0:
        subprocess.check_call([patch])
    else:
        raise RuntimeError(f"Invalid patch file {patch}.")


def main() -> None:
    logging.basicConfig(level=logging.INFO)

    parser = argparse.ArgumentParser(description="Apply libchrome patches.")
    parser.add_argument(
        "--libchrome-path",
        default=Path(__file__).resolve().parent.parent,
        required=False,
        help=("Path of libchrome to apply patches. "
              "Defaults to parent of this file's directory."),
    )
    parser.add_argument(
        "--ebuild",
        default=False,
        required=False,
        action="store_true",
        help=("Run from ebuild where the libchrome directory is not a git "
              "repository. Defaults to False."))
    args = parser.parse_args()

    # In non-ebuild mode, change to libchrome directory which should be a git
    # repository for git commands like am and commit at the right repository.
    if not args.ebuild:
        if not os.path.exists(os.path.join(args.libchrome_path, '.git')):
            raise AttributeError(
                f"Libchrome path {args.libchrome_path} is not a git repository "
                "but not running in --ebuild mode.")

    os.chdir(args.libchrome_path)

    # Apply all patches in directory, ordered by type then patch number.
    for prefix in PREFIXES:
        for patch in sorted(glob.glob(f"libchrome_tools/patches/{prefix}-*")):
            logging.info("Applying %s...", patch)
            apply_patch(patch, args.ebuild)


if __name__ == "__main__":
    main()
