#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script to generate and upload libchrome uprev commit.

Given uprev target, generate the uprev commit and upload to gerrit for review.

This should be run inside chroot (for emerge command) from
CHROMIUM_SRC/src/platform/libchrome/ directory.

For a typical automated daily uprev to cros/upstream HEAD (run locally):
  libchrome_tools/developer-tools/uprev/automated_uprev.py
This is equivalent to:
  libchrome_tools/developer-tools/uprev/automated_uprev.py --head

Provide the hash of target commit directly:
  libchrome_tools/developer-tools/uprev/automated_uprev.py --hash <hash>

For incremental uprev, use:
  libchrome_tools/developer-tools/uprev/automated_uprev.py --days <n>
to uprev up to the commit submitted n days after current latest on tracked
branch (default cros/main).
Or provide the target date manually (anything git log takes as --before arg):
  libchrome_tools/developer-tools/uprev/automated_uprev.py --datetime <datetime>
Format could be e.g. "yyyy-MM-dd" or "yyyy-MM-dd'T'HH:mm:ssZ".

For aid to manual uprev:
  libchrome_tools/developer-tools/uprev/automated_uprev.py
    --before_revision <revision target>
If creating for local experiment on top of existing uprev commit at branch:
  libchrome_tools/developer-tools/uprev/automated_uprev.py
    --before_revision <revision target>
    --track_branch <on-going uprev branch>
Run with --no_upload option to skip uploading the created change to Gerrit.
"""

import argparse
import datetime
import logging
import os
import re
import subprocess
import sys
import typing

BASE_VER_FILE = "BASE_VER"
BUILD_GN_FILE = "BUILD.gn"
PATCHES_LIST_FILE = "libchrome_tools/patches/patches"

COMMIT_REVISION_RE = re.compile(
    r"\s*Cr-Commit-Position: refs\/heads\/\w+@\{#([0-9]+)\}$"
)
PATCH_RE = re.compile(r"^.+\.patch$")
SECTION_HEADER_RE = re.compile(r"^# ={5}=* .* ={5}=*$")
CHERRY_PICK_PATCH_RE = re.compile(r"^cherry-pick-r([0-9]+)-.+\.patch$")
BUILD_GN_SOURCE_FILE_LINE_RE = re.compile(r"\s*([\"\'])(.*)\1,")

Commit = typing.NamedTuple("Commit", [("hash", str), ("revision", int)])
GitMergeSummary = typing.NamedTuple(
    "GitMergeSummary",
    [("files_added", typing.List[str]), ("files_removed", typing.List[str])],
)


def IsInsideChroot() -> bool:
    """Checks that the script is run inside chroot. Copied from chromite/lib/cros_build_lib.py"""
    return os.path.exists("/etc/cros_chroot_version")


def GetRevisionFromHash(commit_hash: str) -> int:
    """Parses trailer of upstream commit (by hash) and returns revision number."""
    try:
        # `git log <commit>` shows history before <commit>. Use `-1` to restrict it
        # to output the most recent commit only i.e. the specified one.
        commit_log = subprocess.check_output(
            ["git", "log", commit_hash, "-1"], universal_newlines=True
        ).splitlines()
    except subprocess.CalledProcessError as e:
        raise ValueError(
            f"Commit hash {commit_hash} not found on cros/upstream.\n"
            f"Error: {e.output}"
        )

    for line in commit_log:
        m = COMMIT_REVISION_RE.match(line)
        if m:
            return int(m.group(1))

    raise Exception(
        "Cannot find revision number from commit message of " f"{commit_hash}"
    )


def GetLatestCommitHashBeforeRevision(revision: int) -> str:
    """Returns hash of commit with revision number <= the given number.

    git's --grep option provides the most efficient way of filtering commits by
    matching for pattern in commit message. (Compared to e.g. output the entire
    history each with a complete commit message and then search in python.)
    Unfortunately, it does not support printing only the matched pattern. So it
    is not possible to e.g. print the Cr-Commit-Position information (and commit
    hash) for all commits on cros/upstream and search on that concise output
    here.

    The compromised solution is to run git rev-list with grep in rounds with
    a matching pattern decreasing in accuracy, so that once some match is found,
    the first result will be the one closest to the target revision and we can
    terminate the loop.

    Starts from the pattern exactly the same as the given revison, then at each
    iteration gets rid of the rightmost digit and replaces it with the list
    numbers 0 up to the one before it, and padding [0-9] for correct number of
    digits.
    Special handling when the last digit is 0.
    Only the beginning part of this numerical sequence is matched as the full
    pattern contains the trailer header ("Cr-Commit-Position...").

    E.g. if it is looking for 98706, first searches for revision number starting
    with 98706, followed by that (starting with) 9870[0-5], 986[0-9][0-9],
    98[0-5][0-9][0-9], and so on.
    (Note we cannot just search directly with the patterning matching all
    revision numbers that starts with 9 since the first result returned could be
    99xxxx which is after the target.)
    """
    # Beginning part of pattern for revision number, kept as int for performing
    # arithmetic operations when updating at each iteration(//10 and -1).
    revision_prefix = revision
    # Ending part of pattern for revision number, consists of [0-n][0-9]*.
    pattern = ""

    # l is the number of [0-9] for padding in the pattern, goes from 0 (search
    # for exact match of target revision) to
    # (# number of digit in target revision - 1).
    for l in range(len(str(revision))):
        grep_pattern = (
            r"^\s*Cr-Commit-Position: refs\/heads\/\w\+@{#"
            + str(revision_prefix)
            + pattern
            + "}"
        )
        try:
            commit_hash = subprocess.check_output(
                [
                    "git",
                    "rev-list",
                    "cros/upstream",
                    "--grep",
                    grep_pattern,
                    "-n1",
                ],
                universal_newlines=True,
            ).strip()
        except subprocess.CalledProcessError as e:
            raise ValueError(
                "Failed to format git rev-list query for revision "
                f"matching pattern {revision_prefix}{pattern}.\n"
                f"Error: {e.output}"
            )

        if commit_hash:
            return commit_hash

        last_digit = revision_prefix % 10
        if last_digit:
            pattern = r"[0-%d][0-9]\{%d\}"%(last_digit - 1, l)
            revision_prefix //= 10
        else:
            pattern = r"[0-9]\{%d\}"%(l+1)
            revision_prefix = revision_prefix // 10 - 1

    # No commit has revision number with the same number of digits as the target
    # revision number.
    l = len(str(revision))-1
    grep_pattern = r"^\s*Cr-Commit-Position: refs\/heads\/\w\+@{#[0-9]\{,l\}}"
    try:
        commit_hash = subprocess.check_output(
            [
                "git",
                "rev-list",
                "cros/upstream",
                "--grep",
                grep_pattern,
                "-n1",
            ],
            universal_newlines=True,
        ).strip()
    except subprocess.CalledProcessError as e:
        raise ValueError(
            "Failed to format git rev-list query for revision "
            f"matching pattern {revision_prefix}{pattern}.\n"
            f"Error: {e.output}"
        )

    if commit_hash:
        return commit_hash


    raise ValueError(
        f"No commit with revision number <= {revision} found on "
        "cros/upstream."
    )


def GetTargetCommitFromDateTime(end_date: str) -> Commit:
    """Returns the last commit up to end_date (in any format accepted by git)."""
    try:
        commit_hash = subprocess.check_output(
            ["git", "rev-list", "cros/upstream", "--before", end_date, "-n1"],
            universal_newlines=True,
        ).strip()
    except subprocess.CalledProcessError as e:
        raise ValueError(
            f"git rev-list getting commit submitted before {end_date} "
            f"on cros/upstream failed.\n"
            f"Error: {e.output}"
        )

    if not commit_hash:
        raise ValueError(
            f"Invalid date or no commit submitted before {end_date} "
            "found on cros/upstream.\n"
        )

    return commit_hash, GetRevisionFromHash(commit_hash)


def GetLibchromeDate(track_branch: str = "cros/main") -> datetime.datetime:
    """Returns libchrome date.

    I.e. submission date (to upstream chromium) of the commit at r<BASE_VER> at
    branch track_branch (default is cros/main).
    """
    revision = GetLibchromeRevision(track_branch)
    grep_pattern = "^\s*Cr-Commit-Position: refs\/heads\/\w\+@{#%d}" % revision
    try:
        current_date = subprocess.check_output(
            [
                "git",
                "log",
                track_branch,
                "--grep",
                grep_pattern,
                "-n1",
                '--pretty="%cs"',
            ],
            universal_newlines=True,
        ).strip()
        logging.info(
            f"Tracked branch {track_branch} at revision r{revision} was "
            f"submitted on {current_date}."
        )
    except subprocess.CalledProcessError as e:
        raise ValueError(
            "Cannot find upstream commit with version BASE_VER "
            f"{base_ver} on cros/main. \n"
            f"Error: {e.output}"
        )

    return datetime.datetime.strptime(current_date, '"%Y-%m-%d"')


def GetLatestUpstreamCommit() -> Commit:
    """Returns latest commit on cros/upstream."""
    try:
        commit_hash = subprocess.check_output(
            ["git", "rev-list", "cros/upstream", "-n1"], universal_newlines=True
        ).strip()
    except subprocess.CalledProcessError as e:
        raise ValueError(
            f"Cannot find latest commit on cros/upstream.\n"
            f"Error: {e.output}"
        )

    return commit_hash, GetRevisionFromHash(commit_hash)


def GetTargetCommit(args) -> Commit:
    """Returns hash and revision number of uprev target commit on cros/upstream
    based on input argument.
    """
    if args.hash:
        return args.hash, GetRevisionFromHash(args.hash)
    if args.before_revision:
        commit_hash = GetLatestCommitHashBeforeRevision(args.before_revision)
        return commit_hash, GetRevisionFromHash(commit_hash)
    if args.datetime:
        return GetTargetCommitFromDateTime(args.datetime)
    if args.days:
        assert args.days >= 0, "Invalid args for --days; must be non-negative."
        target_date = GetLibchromeDate(args.track_branch) + datetime.timedelta(
            days=args.days
        )
        return GetTargetCommitFromDateTime(str(target_date))
    # default option --head
    return GetLatestUpstreamCommit()


def GetLibchromeRevision(track_branch: str = "cros/main") -> int:
    """Reads BASE_VER to return current revision number on cros/main (or any other
    specified) branch.
    """
    try:
        base_ver = subprocess.check_output(
            ["git", "show", f"{track_branch}:{BASE_VER_FILE}"],
            universal_newlines=True,
        ).strip()
    except subprocess.CalledProcessError as e:
        raise ValueError(
            f"Cannot get BASE_VER on given branch {track_branch}.\n"
            f"Error: {e.output}"
        )
    return int(base_ver)


def UpdateBaseVer(revision: int) -> None:
    """Overwrites BASE_VER with uprev target revision number."""
    with open(BASE_VER_FILE, "w") as f:
        f.write(f"{revision}\n")


def OutdatedPatches(
    revision: int, patches_sources: typing.List[str]
) -> (
    # list of pairs of starting and ending lines for each removed patch
    typing.List[typing.Tuple[int, int]],
    # list of removed patches
    typing.List[str],
):
    """Parses libchrome_tools/patches/patches and returns outdated patches."""
    removed_lines = []
    removed_patches = []

    # Starting idx of a (suspected) patch block; would be updated as the lines are
    # read top-down.
    remove_start_idx = None

    for idx, line in enumerate(patches_sources):
        # Found a cherry-pick patch.
        m = CHERRY_PICK_PATCH_RE.match(line)
        if m:
            remove_revision = m.group(1)
            # The patch is outdated, i.e. we have uprev-ed to at least its revision.
            if int(remove_revision) <= revision:
                removed_patches.append(line)
                removed_lines.append((remove_start_idx, idx + 1))

        # Update remove_start_idx. This could be the index after .patch line or
        # section header, or the index of an empty line.
        m_patch = PATCH_RE.match(line)
        m_section = SECTION_HEADER_RE.match(line)
        if m_patch or m_section:
            remove_start_idx = idx + 1
            continue
        if line == "":
            remove_start_idx = idx
            continue

    return removed_lines, removed_patches


def UpdatePatchesList(revision: int) -> (typing.List[str]):
    """Updates libchrome_tools/patches/patches by removing outdated patches.

    Returns list of removed patches.
    """
    with open(PATCHES_LIST_FILE, "r+") as f:
        sources = f.read().splitlines()

    removed_lines, removed_patches = OutdatedPatches(revision, sources)
    for start, end in reversed(removed_lines):
        del sources[start:end]

    # Write new patches list file if any patch should be removed.
    if removed_patches:
        # To avoid eating the newline at the end of the file.
        sources.append("")
        with open(PATCHES_LIST_FILE, "w") as f:
            f.write("\n".join(sources))

    return removed_patches


def ParseGitMergeSummary(merge_summary: typing.List[str]) -> GitMergeSummary:
    """Returns list of non-test .cc source files added and deleted according to
    git merge command output.
    """
    # Each of the added files and deleted files will be shown in a line in form
    # 'create mode <permission> foo.cc' or 'delete mode <permission> foo.cc'.
    added_files = []
    deleted_files = []
    for line in merge_summary:
        line = line.strip()
        if not line.startswith("create") and not line.startswith("delete"):
            continue

        file = line.split()[-1]
        if file.endswith(".cc") and "unittest" not in file:
            if line.startswith("create"):
                added_files.append(file)
            else:  # The line starts with "delete".
                deleted_files.append(file)
    return added_files, deleted_files


def UpdateBuildGn(deleted_files: typing.List[str]) -> typing.List[str]:
    """Removes deleted .cc files from BUILD.gn sources lists.

    Returns the list of deleted files.
    """
    with open(BUILD_GN_FILE, "r") as f:
        source = f.read().splitlines()

    delete_idx = []
    build_gn_deleted_files = []
    for idx, line in enumerate(source):
        m = BUILD_GN_SOURCE_FILE_LINE_RE.match(line)
        if m and m.group(2) in deleted_files:
            delete_idx.append(idx)
            deleted_files.remove(m.group(2))
            build_gn_deleted_files.append(m.group(2))

    if delete_idx:
        for idx in reversed(delete_idx):
            del source[idx]

        with open(BUILD_GN_FILE, "w") as f:
            source.append(
                ""
            )  # To avoid eating the newline at the end of the file.
            f.write("\n".join(source))

    return build_gn_deleted_files


def EmergeLibchrome(recipe: bool) -> bool:
    """Returns whether or not running emerge libchrome succeeds.

    Runs the commands silently to avoid spamming output.
    """
    subprocess.run(
        ["sudo", "cros-workon", "--host", "start", "libchrome"],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        check=True,
    )
    emerge_result = subprocess.run(
        ["sudo", "emerge", "libchrome"],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    return emerge_result.returncode == 0


def CreateUprevCommit(
    commit_hash: str,
    revision: int,
    track_branch: str,
) -> typing.List[str]:
    """Runs git merge and conduct typical changes (update BASE_VER, etc).

    Returns the final commit message as list of strings (each str is one line).
    """
    subprocess.run(
        [
            "git",
            "checkout",
            "-B",
            f"r{revision}-uprev",
            "--track",
            track_branch,
            "--quiet",
        ],
        check=True,
    )

    message = [
        f"Automated commit: libchrome r{revision} uprev\n",
        f"Merge with upstream commit {commit_hash}",
    ]

    try:
        merge_summary = subprocess.check_output(
            ["git", "merge", commit_hash, "-m", "\n".join(message)],
            universal_newlines=True,
        ).splitlines()
    except subprocess.CalledProcessError as e:
        # Provide target hash in an empty commit for further local debugging.
        subprocess.run(["git", "merge", "--abort", "--quiet"])
        message.insert(2, "GIT MERGE FAILED")
        subprocess.run(
            [
                "git",
                "commit",
                "--allow-empty",
                "--quiet",
                "-m",
                "\n".join(message),
            ],
            check=True,
        )
        return message

    UpdateBaseVer(revision)

    _, removed_files = ParseGitMergeSummary(merge_summary)
    removed_from_gn_files = UpdateBuildGn(removed_files)
    if removed_from_gn_files:
        message.append(f"Removed following files from {BUILD_GN_FILE} sources:")
        message.extend(["  * " + f for f in removed_from_gn_files])

    removed_patches = UpdatePatchesList(revision)
    if removed_patches:
        message.append(f"Removed following patches from {PATCHES_LIST_FILE}:")
        message.extend(["  * " + f for f in removed_patches])

    message.append(f"\nBUG=None")
    message.append(f"TEST=sudo emerge libchrome")

    subprocess.run(
        [
            "git",
            "add",
            "BASE_VER",
            "BUILD.gn",
            "libchrome_tools/patches/patches",
        ],
        check=True,
    )
    subprocess.run(
        ["git", "commit", "--amend", "--quiet", "-m", "\n".join(message)],
        check=True,
    )

    return message


def PushOptions(emerge_success=None) -> str:
    """
    Returns options to be used with `git push`.
    TODO(b/251642220): before formal rotation, add Bot-Commit vote to submit
    automatically
      * remove Commit-Queue+1 and Auto-Submit+1
      * remove fqj, hidehiko from reviewers
      * (optional) find out who is on-duty and add them as reviewer
    """
    push_options = (
        "%"
        +
        # Add reviewers
        "r=fqj@google.com,"
        + "r=hidehiko@google.com,"
        +
        # Set topic for easy searching of commits on gerrit
        "topic=libchrome-automated-uprev,"
        +
        # Submit CL to CQ automatically after approval
        "l=Auto-Submit+1,"
    )
    # Add verified label according to 'sudo emerge libchrome' result, not set if
    # emerge is not run
    if emerge_success:
        push_options += "l=Verified+1,"
    elif emerge_success == False:
        push_options += "l=Verified-1,"
    # Start CQ dry run on upload if emerge libchrome succeeded locally
    if emerge_success:
        push_options += "l=Commit-Queue+1,"
    return push_options


def UploadUprevCommit(push_options: str) -> None:
    subprocess.run(
        ["git", "push", "cros", "HEAD:refs/for/main" + push_options],
        check=True,
    )



def main():
    parser = argparse.ArgumentParser(
        description="Generate and upload libchrome uprev commit."
    )

    group = parser.add_mutually_exclusive_group()
    group.add_argument(
        "--hash",
        type=str,
        help="Uprev to the given hash, exit with error if not found.",
    )
    group.add_argument(
        "--before_revision",
        type=int,
        help="Uprev to the latest commit on or before the given revision number.",
    )
    group.add_argument(
        "--datetime",
        type=str,
        help="Uprev to the latest commit on or before the given date time.",
    )
    group.add_argument(
        "--days",
        type=int,
        help="Uprev to the latest commit submitted n days after current latest on"
        " cros/main.",
    )
    group.add_argument(
        "--head",
        action="store_true",
        help="Uprev to (latest commit) HEAD on cros/upstream.",
    )

    parser.add_argument(
        "--track_branch",
        type=str,
        help="Branch to be tracked by the uprev commit. Default is cros/main.",
        default="cros/main",
    )

    parser.add_argument(
        "--query_commit",
        action="store_true",
        help="Only output hash and revision number and do not perform merge.",
        default=False,
    )

    parser.add_argument(
        "--no_upload",
        action="store_true",
        help="Do not upload uprev commit to gerrit.",
        default=False,
    )

    parser.add_argument(
        "--recipe",
        action="store_true",
        help="The command is running as a recipe, i.e. not running manually."
        "Output git push options instead of running git push directly.",
        default=False,
    )

    args = parser.parse_args()

    logging.getLogger().setLevel("CRITICAL" if args.recipe else "INFO")

    target_commit_hash, target_commit_revision = GetTargetCommit(args)
    logging.info(
        f"Uprev to revision {target_commit_revision} with hash "
        f"{target_commit_hash}"
    )
    if args.query_commit:
        exit()

    if target_commit_revision < GetLibchromeRevision(args.track_branch):
        raise Exception(
            f"Target revision {target_commit_revision} has been reached already: "
            f"libchrome is currently at {GetLibchromeRevision(args.track_branch)}."
        )

    if subprocess.run(["git", "diff", "--quiet"]).returncode:  # if git dirty
        raise Exception(
            "Git working directory is dirty. Abort creating uprev commit."
        )

    commit_message = CreateUprevCommit(
        target_commit_hash, target_commit_revision, args.track_branch
    )

    emerge_success = None
    if IsInsideChroot():
        if not EmergeLibchrome(args.recipe):
            commit_message.insert(2, "EMERGE LIBCHROME IS FAILING\n")
            emerge_success = False
        else:
            emerge_success = True
    else:
        logging.warning("Not inside chroot, emerge libchrome is not run.")
        commit_message.insert(2, "DID NOT EMERGE LIBCHROME\n")
    subprocess.run(
        ["git", "commit", "--amend", "--quiet", "-m", "\n".join(commit_message)],
        check=True,
    )

    push_options = PushOptions(emerge_success)
    if args.recipe:
        print(push_options)
        return

    if not args.no_upload:
        UploadUprevCommit(push_options)
    logging.info("Finished running automated_uprev.py; emerge libchrome " +
                 ("succeeded" if emerge_success else "failed"))


if __name__ == "__main__":
    sys.exit(main())
