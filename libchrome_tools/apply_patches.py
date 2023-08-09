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
import re
import stat
import subprocess
from typing import NamedTuple, Optional, Sequence

PREFIXES = [
    "long-term",
    "cherry-pick",
    "backward-compatibility",
    "forward-compatibility",
]
PATCH_BASENAME_RE = re.compile(r"(" + "|".join(PREFIXES) + r")")


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
        # Record patch name in created commit for future formatting patches.
        if not use_git_apply:
            basename = os.path.basename(patch)
            patch_name_trailers = _run_or_log_cmd([
                "git", "log", "-n1",
                '--format=%(trailers:key=patch-name,valueonly)'
            ], True, dry_run).stdout.strip().splitlines()
            if patch_name_trailers and basename not in patch_name_trailers:
                logging.warning(
                    "Applied patch contains patch-name trailers (%s) different "
                    "from filename (%s). Overwriting with filename.",
                    patch_name_trailers, basename)
            _run_or_log_cmd([
                "git", "-c", "trailer.ifexists=replace", "commit", "--amend",
                "--no-edit", "--trailer",
                f"patch-name: {os.path.basename(patch)}"
            ], True, dry_run)
    elif os.stat(patch).st_mode & stat.S_IXUSR != 0:
        if _run_or_log_cmd([patch], ebuild, dry_run).retcode:
            raise RuntimeError(f"Patch script {patch} failed. Please fix.")
        # Commit local changes made by script as a temporary commit unless in
        # no-commit mode.
        if not use_git_apply:
            _commit_script_patch(patch, dry_run)
    else:
        raise RuntimeError(f"Invalid patch file {patch}.")


def _sanitize_patch_args(arg_name: str, arg_value: Optional[str],
                         libchrome_path: str) -> Optional[str]:
    """Assert the patch argument is either not provided, basename-only, or a
     path in the right directory (<libchrome_path>/libchrome_tools/patches/).

    Args:
      arg_name: name of argument sanitized ("first" or "last").
      arg_value: value of the patch argument.
      libchrome_path: absolute real path of the target libchrome directory.

    Returns: basename of patch, or None if not provided.
    """
    if not arg_value:
        return None

    patch_dir = os.path.join(libchrome_path, 'libchrome_tools', 'patches')

    # If provided as a path, assert parent directory is the target patch_dir.
    if os.path.dirname(arg_value):
        # Expand and resolve path as an absolute real path.
        arg_value = Path(arg_value).expanduser().resolve()
        if os.path.dirname(arg_value) != patch_dir:
            raise ValueError(
                f"--{arg_name} ({arg_value})) is given as a path but its parent "
                f"directory is not {patch_dir}. "
                "You can specify target libchrome repository with --libchrome_path."
            )

    basename = os.path.basename(arg_value)
    # Assert basename of patch has a valid patch name.
    if not PATCH_BASENAME_RE.match(basename):
        raise ValueError(
            f"--{arg_name} ({arg_value}) is not a valid patch: patch name must "
            f"start with prefixes in {', '.join(PREFIXES)}.")

    # Only the basename matters after verifying parent directory is patch_dir,
    # if given as a path.
    # Assert patch exists in the target patch_dir.
    if not os.path.exists(os.path.join(patch_dir, basename)):
        raise ValueError(
            f"--{arg_name} ({arg_value})) does not exist in {patch_dir}.")
    return basename


def _clamp_patches(patches: Sequence[str], first: Optional[str],
                   last: Optional[str], libchrome_path: str) -> Sequence[str]:
    """Return patches between first (or real first) and last (or real last).

    Args:
      patches: Patches as relative path from libchrome_path, i.e. each is a
        string "libchrome_tools/patches/<patch_name>", sorted in apply order.
      first: Basename of first patch to apply. Must exist in patches if given.
      last: Basename of last patch to apply. Must exist in patches if given.
      libchrome_path: Absolute real path to the target libchrome directory.

    Returns: The clamped sequence of patches.
    """
    first_index = ((patches.index(
        os.path.join('libchrome_tools/patches/', first))) if first else 0)
    last_index = (
        (patches.index(os.path.join('libchrome_tools/patches/', last)) +
         1) if last else len(patches))
    return patches[first_index:last_index]


def apply_patches(libchrome_path: str,
                  ebuild: bool,
                  no_commit: bool,
                  first: Optional[str] = None,
                  last: Optional[str] = None,
                  dry_run: bool = False) -> None:
    """Apply patches in libchrome_tools/patches/ to the target libchrome.

    Args:
      libchrome_path: Path to the target libchrome.
      ebuild: Flag to apply patches in ebuild mode (not a git repo).
      no_commit: Flag to not commit changes made by each patch.
      first: First patch to apply (basename or path).
      last: Last patch to apply (basename or path).
      dry_run: Flag for dry run mode (commands are logged without running).
    """
    # Expand and resolve path as an absolute real path.
    libchrome_path = Path(libchrome_path).expanduser().resolve()

    # Assert first or last exists in <libchrome_path>/libchrome_tools/patches/,
    # and is a valid patch.
    # Crop to basename (the only relevant part) after passing the check.
    first = _sanitize_patch_args('first', first, libchrome_path)
    last = _sanitize_patch_args('last', last, libchrome_path)

    # Change to the target libchrome directory (after resolving paths above).
    os.chdir(libchrome_path)

    # Abort if git repository is dirty (in non-ebuild mode).
    if (not ebuild and _run_or_log_cmd(["git", "diff", "--quiet"], False,
                                       dry_run).retcode):
        raise RuntimeError(
            "Git working directory is dirty. Abort applying patches."
        )

    # Get the list of all patches in the libchrome_tools/patches/ directory and
    # clamp to range as specified.
    patches = [
        patch for prefix in PREFIXES
        for patch in sorted(glob.glob(f"libchrome_tools/patches/{prefix}-*"))
    ]
    patches = _clamp_patches(patches, first, last, libchrome_path)

    for patch in patches:
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
        "--first",
        required=False,
        help=("The basename or path of the first patch to apply (inclusive)."
              "Defaults to apply from the first patch in directory."))
    parser.add_argument(
        "--last",
        required=False,
        help=("The basename or path of the last patch to apply (inclusive)."
              "Defaults to apply til the last patch in directory."))
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

    apply_patches(args.libchrome_path, args.ebuild, args.no_commit, args.first,
                  args.last, args.dry_run)


if __name__ == "__main__":
    main()
