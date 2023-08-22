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
from typing import NamedTuple, Optional, Sequence

PREFIXES = [
    "long-term",
    "cherry-pick",
    "backward-compatibility",
    "forward-compatibility",
]


class CommandResult(NamedTuple):
    retcode: int
    stdout: str
    stderr: str


def _run_or_log_cmd(cmd: Sequence[str], fatal: bool,
                    dry_run: bool) -> CommandResult:
    logging.debug("$ %s", " ".join(cmd))
    if dry_run:
        return CommandResult(0, "", "")
    completed_process = subprocess.run(cmd,
                                       check=fatal,
                                       capture_output=True,
                                       universal_newlines=True)
    return CommandResult(completed_process.returncode,
                         completed_process.stdout, completed_process.stderr)


def _commit_script_patch(patch: str, dry_run: bool) -> None:
    _run_or_log_cmd(["git", "add", "."], True, dry_run)
    _run_or_log_cmd(
        [
            "git",
            "commit",
            "-m",
            "Temporary commit for script-based patch",
            "-m",
            f"patch-name: {os.path.basename(patch)}",
        ],
        True,
        dry_run,
    )


def _git_apply_patch(patch: str, use_git_apply: bool, threeway: bool,
                     fatal: bool, dry_run: bool) -> int:
    # "-C1" to be compatible with `patch` and to be more robust against upstream
    # changes.
    git_apply_cmd = ["git", "apply" if use_git_apply else "am", "-C1", patch]
    if threeway:
        git_apply_cmd.append("--3way")
    return _run_or_log_cmd(git_apply_cmd, fatal, dry_run).retcode


def apply_patch(patch: str, ebuild: bool, use_git_apply: bool,
                dry_run: bool) -> None:
    """Applying given patch."""
    assert not ebuild or use_git_apply, (
        "--ebuild mode must be run with no_commit = True.")
    if patch.endswith(".patch"):
        # git apply/ am is fatal (exit immediately if fail) if in ebuild mode.
        # Otherwise, if fail (return code is non-zero), rerun to leave a 3-way
        # merge marker.
        if _git_apply_patch(patch,
                            use_git_apply,
                            threeway=False,
                            fatal=ebuild,
                            dry_run=dry_run):
            # Failed `git am` will leave rebase directory even without --3way.
            if not use_git_apply:
                _run_or_log_cmd(['git', 'am', '--abort'], True, dry_run)
            if _git_apply_patch(patch,
                                use_git_apply,
                                threeway=True,
                                fatal=False,
                                dry_run=dry_run):
                raise RuntimeError(
                    f"Failed to git {'apply' if use_git_apply else 'am'} patch "
                    f"{patch}; please check 3-way merge markers and resolve "
                    "conflicts.")
    elif os.stat(patch).st_mode & stat.S_IXUSR != 0:
        if _run_or_log_cmd([patch], ebuild, dry_run).retcode:
            raise RuntimeError(f"Patch script {patch} failed. Please fix.")
        # Commit local changes made by script as a temporary commit unless in
        # no-commit mode.
        if not use_git_apply:
            _commit_script_patch(patch, dry_run)
    else:
        raise RuntimeError(f"Invalid patch file {patch}.")


def apply_patches(libchrome_path: str, ebuild: bool, no_commit: bool,
                  dry_run: bool) -> None:
    os.chdir(libchrome_path)
    # Abort if git repository is dirty (in non-ebuild mode).
    if (not ebuild and _run_or_log_cmd(["git", "diff", "--quiet"], False,
                                       dry_run).retcode):
        raise RuntimeError(
            "Git working directory is dirty. Abort applying patches."
        )

    # Apply all patches in directory, ordered by type then patch number.
    for prefix in PREFIXES:
        for patch in sorted(glob.glob(f"libchrome_tools/patches/{prefix}-*")):
            logging.info("Applying %s...", patch)
            apply_patch(patch, ebuild, no_commit, dry_run)


def main() -> None:
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
              "repository. Defaults to False."),
    )
    parser.add_argument(
        "--no-commit",
        default=False,
        required=False,
        action="store_true",
        help=("Do not commit changes made by each patch."
              "Defaults to False except in --ebuild mode (always True)."),
    )
    parser.add_argument(
        "--dry-run",
        default=False,
        required=False,
        action="store_true",
        help=(
            "dry run mode where patches are not really applied. Log as debug "
            "statements instead. Defaults to False."),
    )
    args = parser.parse_args()

    logging.basicConfig(level=logging.INFO)

    # In non-ebuild mode, change to libchrome directory which should be a git
    # repository for git commands like am and commit at the right repository.
    if not args.ebuild:
        if not os.path.exists(os.path.join(args.libchrome_path, ".git")):
            raise AttributeError(
                f"Libchrome path {args.libchrome_path} is not a git repository "
                "but not running in --ebuild mode.")
    # Never commit changes from patches in ebuild mode.
    else:
        logging.info('In --ebuild mode, --no_commit is always set to True.')
        args.no_commit = True

    apply_patches(args.libchrome_path, args.ebuild, args.no_commit,
                  args.dry_run)


if __name__ == "__main__":
    main()
