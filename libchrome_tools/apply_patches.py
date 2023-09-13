#!/usr/bin/env python3
# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Tool to apply libchrome patches.
"""

import argparse
import datetime
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
_PATCH_BASENAME_PATTERN = r"(" + "|".join(PREFIXES) + r")-(\d{4})-(.+)\.patch"
# A matched result has group (1) prefix, (2) number in the prefix group, (3)
# descriptive name of patch.
PATCH_BASENAME_RE = re.compile(_PATCH_BASENAME_PATTERN)
# A matched result has group (1) commit hash, (2) prefix, (3) number in the
# prefix group, (4) descriptive name of patch.
PATCH_NAME_TRAILER_RE = re.compile(r"(\w+): " + _PATCH_BASENAME_PATTERN + r"$")

TAG = "HEAD-before-patching"


def apply_patch_order_key_fn(patch_name) -> (int, int):
    """Return key for sorting patches in apply order at build time.

    First number is prefix order, e.g. "long-term" is 0 and "cherry-pick"
    is 1 and so on.
    Second is the 4-digit number for the patch within that patch group.
    """
    m = PATCH_BASENAME_RE.match(patch_name)
    assert m, f"Patch name is invalid: should match {_PATCH_BASENAME_PATTERN}."
    return PREFIXES.index(m.group(1)), m.group(2)


class PatchCommit(NamedTuple):
    """Hash of a patch commit and the corresponding patch file basename."""
    hash: str
    patch_name: str


class PotentialPatchCommit(NamedTuple):
    """Commit that is potentially a patch commit."""
    hash: str
    patch_name_trailers: Sequence[str]

    def is_patch_commit(self) -> bool:
        return (len(self.patch_name_trailers) == 1
                and PATCH_BASENAME_RE.match(self.patch_name_trailers[0]))

    def to_patch_commit(self) -> PatchCommit:
        assert self.is_patch_commit()
        return PatchCommit(self.hash, self.patch_name_trailers[0])


class CommandResult(NamedTuple):
    retcode: int
    stdout: str
    stderr: str


def _run_or_log_cmd(cmd: Sequence[str],
                    fatal: bool = True,
                    dry_run: bool = False) -> CommandResult:
    logging.debug("$ %s", " ".join(cmd))
    if dry_run:
        return CommandResult(0, "", "")
    completed_process = subprocess.run(cmd,
                                       check=fatal,
                                       capture_output=True,
                                       universal_newlines=True)
    return CommandResult(completed_process.returncode,
                         completed_process.stdout.strip(),
                         completed_process.stderr.strip())


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


def get_all_patches(libchrome_path: str) -> Sequence[str]:
    """Return list of patches in given libchrome repo in correct apply order."""
    patches = [
        p for p in os.listdir(
            os.path.join(libchrome_path, "libchrome_tools", "patches"))
        if PATCH_BASENAME_RE.match(p)
    ]
    return sorted(patches, key=apply_patch_order_key_fn)


def _git_apply_patch(patch: str, use_git_apply: bool, threeway: bool,
                     fatal: bool, dry_run: bool) -> int:
    # "-C1" to be compatible with `patch` and to be more robust against upstream
    # changes.
    git_apply_cmd = ["git", "apply" if use_git_apply else "am", "-C1", patch]
    if threeway:
        git_apply_cmd.append("--3way")
    return _run_or_log_cmd(git_apply_cmd, fatal, dry_run).retcode


def apply_patch(patch_name: str, libchrome_path: str, ebuild: bool,
                use_git_apply: bool, dry_run: bool) -> None:
    """Apply given patch.

    Args:
        patch_name: Basename of patch to apply. Must exist in the patches directory.
        libchrome_path: Absolute real path to libchrome repository.
        ebuild: In ebuild mode (not a git repository).
        use_git_apply: Use git apply (instead of git am).
        dry_run: In dry run mode (commands are logged not run).
    """
    assert not ebuild or use_git_apply, (
        "--ebuild mode must be run with no_commit = True.")
    patch_path = os.path.join(libchrome_path, "libchrome_tools", "patches",
                              patch_name)
    if patch_path.endswith(".patch"):
        # git apply/ am is fatal (exit immediately if fail) if in ebuild mode.
        # Otherwise, if fail (return code is non-zero), rerun to leave a 3-way
        # merge marker.
        if _git_apply_patch(patch_path,
                            use_git_apply,
                            threeway=False,
                            fatal=ebuild,
                            dry_run=dry_run):
            # Failed `git am` will leave rebase directory even without --3way.
            if not use_git_apply:
                _run_or_log_cmd(['git', 'am', '--abort'], True, dry_run)
            if _git_apply_patch(patch_path,
                                use_git_apply,
                                threeway=True,
                                fatal=False,
                                dry_run=dry_run):
                raise RuntimeError(
                    f"Failed to git {'apply' if use_git_apply else 'am'} patch "
                    f"{patch_name}; please check 3-way merge markers and "
                    "resolve conflicts.")
        # Record patch name in created commit for future formatting patches.
        if not use_git_apply:
            patch_name_trailers = _run_or_log_cmd([
                "git", "log", "-n1",
                '--format=%(trailers:key=patch-name,valueonly)'
            ], True, dry_run).stdout.splitlines()
            if patch_name_trailers and patch_name not in patch_name_trailers:
                logging.warning(
                    "Applied patch contains patch-name trailers (%s) different "
                    "from filename (%s). Overwriting with filename.",
                    patch_name_trailers, patch_name)
            _run_or_log_cmd([
                "git", "-c", "trailer.ifexists=replace", "commit", "--amend",
                "--no-edit", "--trailer", f"patch-name: {patch_name}"
            ], True, dry_run)
    elif os.stat(patch_path).st_mode & stat.S_IXUSR != 0:
        if _run_or_log_cmd([patch_path], ebuild, dry_run).retcode:
            raise RuntimeError(
                f"Patch script {patch_name} failed. Please fix.")
        # Commit local changes made by script as a temporary commit unless in
        # no-commit mode.
        if not use_git_apply:
            _commit_script_patch(patch_path, dry_run)
    else:
        raise RuntimeError(f"Invalid patch file {patch_name}.")


def assert_git_repo_state_and_get_current_branch(dry_run: bool = False) -> str:
    """Assert git repo is in the right state (clean and at a branch)."""
    # Abort if git repository is dirty.
    if _run_or_log_cmd(["git", "diff", "--quiet"], False, dry_run).retcode:
        raise RuntimeError("Git working directory is dirty. Abort script.")
    current_branch = _run_or_log_cmd(["git", "branch", "--show-current"],
                                     True, dry_run).stdout
    if not dry_run and not current_branch:
        raise RuntimeError("Not on a branch. Abort script.")
    return current_branch


def sanitize_patch_args(arg_name: str, arg_value: Optional[str],
                         libchrome_path: str) -> Optional[str]:
    """Assert the patch argument is valid.

    It should be either not provided, basename-only, or a path in the right
    directory (<libchrome_path>/libchrome_tools/patches/).

    Args:
        arg_name: name of argument sanitized ("first" or "last").
        arg_value: value of the patch argument.
        libchrome_path: absolute real path of the target libchrome directory.

    Returns:
        Basename of patch, or None if not provided.
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
                f"--{arg_name} ({arg_value})) is given as a path but its "
                f"parent directory is not {patch_dir}. You can specify target "
                "libchrome repository with --libchrome_path.")

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
                   last: Optional[str]) -> Sequence[str]:
    """Return patches between first (or real first) and last (or real last).

    Args:
        patches: Basename of all patches in the patch directory, sorted in
        apply order.
        first: Basename of first patch to apply. Must exist in patches if given.
        last: Basename of last patch to apply. Must exist in patches if given.

    Returns:
        The clamped sequence of patches.
    """
    first_index = patches.index(first) if first else 0
    last_index = (patches.index(last) + 1) if last else len(patches)
    return patches[first_index:last_index]


def get_patch_head_commit(dry_run: bool = False) -> Optional[str]:
    """Return oneline log of commit tagged HEAD-before-patching, if exists."""
    _, tags, _ = _run_or_log_cmd(["git", "tag"], True, dry_run)
    tags = tags.splitlines() if tags else []
    if TAG in tags:
        return _run_or_log_cmd(["git", "log", TAG, "-1", "--oneline"], True,
                               dry_run).stdout
    return None


def _potential_patch_commits_since_hash(
        commit_hash: str,
        dry_run: bool = False) -> Sequence[PotentialPatchCommit]:
    "Return the list of PotentialPatchCommit since given commit."
    # Each line from output is "<hash>:" followed by a ","-separated list of
    # values of the patch-name trailer, sorted from HEAD to given commit hash.
    # It may contain empty line.
    output = _run_or_log_cmd([
        "git",
        "log",
        "--format=%H:%(trailers:key=patch-name,valueonly,separator=%x2C)",
        f"{commit_hash}..",
    ], True, dry_run).stdout.split('\n')
    # Reverse output so that it is sorted from commit hash to HEAD.
    output.reverse()
    # Parse output by line so each entry is a tuple (hash: str, unparsed string
    # of ','-separated trailers: str).
    output = [line.split(':', maxsplit=2) for line in output if line]
    return [
        PotentialPatchCommit(
            line[0],  # hash
            line[1].split(',') if line[1] else []  # trailers: list of str
        ) for line in output
    ]


def get_patch_commits_since_tag(current_branch: str,
                                allow_tag_not_exist: bool = True,
                                dry_run: bool = False) -> Sequence[PatchCommit]:
    """Get list of patch commits since commit tagged as HEAD-before-patching.

    This would assert that the tag is on current branch and patch commits are
    in the right order.
    If tag does not exist, abort or tag current HEAD depending on value of
    allow_tag_not_exist flag.

    Args:
        current_branch: Name of branch currently on.
        allow_tag_not_exist: If False, abort if tag does not exist; otherwise
            tag current HEAD (and return an empty list).
        dry_run: In dry run mode or not.

    Returns:
        List of patch commits applied since patch-head (sorted from
        HEAD-before-patching to HEAD).
    """
    # Get one-line description of the current HEAD-before-patching commit, if
    # exists.
    patch_head = get_patch_head_commit(dry_run)
    if patch_head:
        logging.info("Tag %s already exists: %s.", TAG, patch_head)
        patch_head_branches = _run_or_log_cmd(
            ["git", "branch", "--contains", TAG, "--format=%(refname:short)"],
            True,
            dry_run,
        ).stdout.splitlines()
        if not dry_run and current_branch not in patch_head_branches:
            raise RuntimeError(
                f"Tag '{TAG}' is on branches "
                f"({', '.join(patch_head_branches)}) which does not include "
                f"current branch {current_branch}. "
                "Please confirm you are on the right branch, or delete "
                f"the obsolete tag by `git tag -d {TAG}`.")

        # Assume the tag is still valid if all commits from HEAD-before-patching
        # to HEAD is generated from a patch and skip re-tagging.
        patch_head_commit_hash = patch_head.split(maxsplit=1)[0]
        commits = _potential_patch_commits_since_hash(patch_head_commit_hash,
                                                      dry_run)
        if all(c.is_patch_commit() for c in commits):
            logging.info("Confirmed only patch commits from %s to HEAD.", TAG)

            commits = [c.to_patch_commit() for c in commits]
            applied_patches = [c.patch_name for c in commits]
            logging.info("%d commits since %s.", len(applied_patches), TAG)
            applied_patches_sorted = sorted(applied_patches,
                                            key=apply_patch_order_key_fn)
            if applied_patches_sorted != applied_patches:
                raise RuntimeError(
                    "Applied patches in git log are not in correct application "
                    "order. This may cause unexpected apply failure when `emerge "
                    "libchrome` with the generated patches. "
                    "Please resort them using `git rebase -i`, correct order:\n",
                    ','.join(applied_patches_sorted))

            return commits

        # Abort if there is non-patch commit and suggest actions.
        non_patch_commits_summary = "\n".join([
            f"{commit.hash}: {len(commit.patch_name_trailers)} patch-name "
            "trailer(s)." for commit in commits
            if not commit.is_patch_commit()
        ])
        raise RuntimeError(
            f"There is non-patch commit from {TAG} to HEAD:\n"
            f"{non_patch_commits_summary}\n"
            "If the tag is obsolete (you would like to return to current HEAD "
            f"after modifying patches), run `git tag -d {TAG}` to remove the "
            "tag and rerun the script.\n"
            "Otherwise, run `git rebase -i` with (r)eword option to add or "
            "remove patch-name trailer to/ from the commit message so that "
            "there is exactly one per commit.", )

    if allow_tag_not_exist:
        # Tag current HEAD; no patch commit since then by definition.
        _run_or_log_cmd(["git", "tag", TAG], True, dry_run)
        logging.info("Tagged current HEAD as HEAD-before-patching.")
        return []

    raise RuntimeError(
        f"Tag {TAG} does not exist. Please run `git tag <hash> {TAG}` with "
        "hash being commit to reset to if you have applied patches manually.")


def apply_patches(libchrome_path: str,
                  ebuild: bool,
                  no_commit: bool,
                  first: Optional[str] = None,
                  last: Optional[str] = None,
                  dry_run: bool = False) -> None:
    """Apply patches in libchrome_tools/patches/ to the target libchrome.

    Args:
        libchrome_path: Absolute real path to the target libchrome.
        ebuild: Flag to apply patches in ebuild mode (not a git repo).
        no_commit: Flag to not commit changes made by each patch.
        first: First patch to apply (basename), must be in patch directory.
        last: Last patch to apply (basename), must be in patch directory.
        dry_run: Flag for dry run mode (commands are logged without running).
    """
    # Change to the target libchrome directory (after resolving paths above).
    os.chdir(libchrome_path)

    if not ebuild:
        # Make sure git repo is in good state.
        current_branch = assert_git_repo_state_and_get_current_branch(
            dry_run=dry_run)
        # Make sure either HEAD-before-patching tag does not exist (and tag
        # HEAD), or all commits from the tagged commit to HEAD are patch
        # commits sorted in application order.
        _ = get_patch_commits_since_tag(current_branch,
                                        allow_tag_not_exist=True,
                                        dry_run=dry_run)

    # Get the list of all patches in the libchrome_tools/patches/ directory and
    # clamp to range as specified.
    patches = _clamp_patches(get_all_patches(libchrome_path), first, last)

    for patch in patches:
        logging.info("Applying %s...", patch)
        apply_patch(patch, libchrome_path, ebuild, no_commit, dry_run)


def format_patches(libchrome_path: str,
                   backup_branch: bool = False,
                   dry_run: bool = False) -> None:
    """Format all commits from HEAD-before-patching to HEAD as patches.

    Args:
        libchrome_path: Absolute real path to libchrome repository.
        backup_branch: Whether or not to back up current state with patch
            commits as a branch before resetting.
        dry_run: Flag in dry run mode.
    """
    # Change to the target libchrome directory (after resolving paths above).
    os.chdir(libchrome_path)

    # Make sure git repo is in good state.
    current_branch = assert_git_repo_state_and_get_current_branch(
        dry_run=dry_run)
    # Make sure HEAD-before-patching tag exists, and all commits from the
    # tagged commit to HEAD are patch commits sorted in application order.
    commits = get_patch_commits_since_tag(current_branch,
                                          allow_tag_not_exist=True,
                                          dry_run=dry_run)

    # If requested, checkout to a new branch now to save a copy of the current
    # git history before resetting to HEAD-before-patching.
    if backup_branch:
        now_str = datetime.datetime.now().strftime("%Y%m%d%H%M%S")
        branch_name = f"apply-patch-backup-{now_str}"
        # Retry with a new branch name at a new time if branch name already in
        # use (unlikely).
        try:
            _run_or_log_cmd(["git", "checkout", "-b", branch_name])
        except subprocess.CalledProcessError:
            now_str = datetime.datetime.now().strftime("%Y%m%d%H%M%S")
            branch_name = f"apply-patch-backup-{now_str}"
            _run_or_log_cmd(["git", "checkout", "-b", branch_name])
        logging.info("Backed up git history to branch %s.", branch_name)
        _run_or_log_cmd(["git", "checkout", current_branch])

    # Format commits as patches, without numbering (-N). Result is ordered in
    # commit order (HEAD-before-patching to HEAD).
    formatted_patches = _run_or_log_cmd(["git", "format-patch", TAG, "-N"],
                                        dry_run=dry_run).stdout.splitlines()
    logging.info("Formatted %d commits since %s as patches.",
                 len(formatted_patches), TAG)

    # Reset to tagged commit.
    _run_or_log_cmd(["git", "reset", "--hard", TAG], dry_run=dry_run)
    logging.info("Reset to %s.", TAG)

    # Move and rename files to <libchrome>/libchrome_tools/patches/ with name
    # specified by the patch-name trailer.
    patch_dir = os.path.join(libchrome_path, "libchrome_tools", "patches")
    for (formatted_patch_file,
         patch_name) in zip(formatted_patches,
                            [c.patch_name for c in commits]):
        os.rename(formatted_patch_file, os.path.join(patch_dir, patch_name))
    logging.info("Moved and renamed formatted patches, please check %s.",
                 patch_dir)

    # Delete tag.
    _run_or_log_cmd(["git", "tag", "-d", TAG], dry_run=dry_run)


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
        "--format-patches",
        "-f",
        default=False,
        required=False,
        action="store_true",
        help=(
            f"Format all commits from commit tagged {TAG} to HEAD as patches "
            f"and reset to {TAG}."),
    )
    parser.add_argument(
        "--backup-branch",
        "-b",
        required=False,
        action="store_true",
        help=("Back up the current state with patch commits as a new branch "
              "before resetting. Only with --format-patches."))
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

    # In format patches mode, do not allow flags for applying patches.
    if args.format_patches:
        if args.ebuild or args.no_commit or args.first or args.last:
            raise ValueError(
                "In --format-patches mode, do not accept flags for applying "
                "patches: --ebuild, --no-commit, --first, --last.")
    else:
        if args.backup_branch:
            raise ValueError(
                "--backup-branch only available in --format-patches mode.")

    # In non-ebuild mode, change to libchrome directory which should be a git
    # repository for git commands like am and commit at the right repository.
    if not args.ebuild:
        if not os.path.exists(os.path.join(args.libchrome_path, ".git")):
            raise AttributeError(
                f"Libchrome path {args.libchrome_path} is not a git repository "
                "but not running in --ebuild mode.")
    # Never commit changes from patches in ebuild mode.
    else:
        logging.info('In --ebuild mode, --no-commit is always set to True.')
        args.no_commit = True

    # Expand and resolve path as an absolute real path.
    libchrome_path = Path(args.libchrome_path).expanduser().resolve()

    # Assert first or last exists in <libchrome_path>/libchrome_tools/patches/,
    # and is a valid patch.
    # Crop to basename (the only relevant part) after passing the check.
    first = sanitize_patch_args('first', args.first, libchrome_path)
    last = sanitize_patch_args('last', args.last, libchrome_path)

    if args.format_patches:
        format_patches(libchrome_path, args.backup_branch, args.dry_run)
    else:
        apply_patches(libchrome_path, args.ebuild, args.no_commit, first, last,
                      args.dry_run)


if __name__ == "__main__":
    main()
