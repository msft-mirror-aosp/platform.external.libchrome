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
import stat
import subprocess
import sys


def main() -> None:
    logging.basicConfig(level=logging.INFO)

    parser = argparse.ArgumentParser(description="Apply libchrome patches.")
    parser.add_argument(
        "--libchrome-checkout-path",
        default=os.getcwd(),
        required=False,
        help="Path of the libchrome checkout. Defaults to the current directory.",
    )
    args = parser.parse_args()

    # Apply all patches in directory, ordered by type then patch number.
    PREFIXES = [
        "long-term",
        "cherry-pick",
        "backward-compatibility",
        "forward-compatibility",
    ]
    for prefix in PREFIXES:
        for patch in sorted(
            glob.glob(
                "%s/libchrome_tools/patches/%s-*"
                % (args.libchrome_checkout_path, prefix)
            )
        ):
            logging.info("Applying %s...", patch)
            if patch.endswith(".patch"):
                # "-C1" to be compatible with `patch` and to be more robust
                # against upstream changes.
                cmd_args = [
                    "git",
                    "apply",
                    "-C1",
                    patch,
                ]
            elif os.stat(patch).st_mode & stat.S_IXUSR != 0:
                cmd_args = [patch]
            else:
                raise RuntimeError("Invalid patch file %s" % patch)

            subprocess.check_call(cmd_args)


if __name__ == "__main__":
    main()
