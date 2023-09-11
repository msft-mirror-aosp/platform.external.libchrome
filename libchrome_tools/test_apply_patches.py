#!/usr/bin/env python
# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import re
import subprocess
import tempfile
import unittest
import unittest.mock
from pathlib import Path
from shutil import copy
from typing import Sequence

import apply_patches


def init_git_repo_with_patches(inject_error: bool = False):
    # Create tmp directory as a git repository (acting-libchrome) with
    # patches directory structure.
    repo_dir = tempfile.TemporaryDirectory()
    repo_dir_path = repo_dir.name
    logging.debug('temporary directory at: %s', repo_dir_path)
    patch_dir = os.path.join(repo_dir_path, 'libchrome_tools/', 'patches/')
    os.makedirs(patch_dir)

    # Copy all files in local test data directory to tmp directory.
    os.chdir(
        os.path.join(
            Path(__file__).resolve().parent, 'testdata', 'apply_patches'))
    for patch in os.listdir():
        copy(patch, patch_dir)

    # Change to tmp directory for git commands. Make initial commit.
    os.chdir(repo_dir_path)
    subprocess.check_call(["git", "init"])
    with open(os.path.join(repo_dir_path, "init_file.txt"),
              "w",
              encoding='utf-8') as f:
        f.write("foo\n")
    subprocess.check_call(["git", "add", "."])
    subprocess.check_call(["git", "commit", "-m", "initial commit"])

    # Need to be done in separate step (not writing a different line to the file
    # directly in above initial commit) otherwise 3-way merge will fail with
    # "repository lacks the necessary blob to perform 3-way merge" error.
    if inject_error:
        with open(os.path.join(repo_dir_path, "init_file.txt"),
                  "w",
                  encoding='utf-8') as f:
            f.write("wrong line\n")
        subprocess.check_call(["git", "commit", "-a", "--amend", "--no-edit"])

    subprocess.check_call(["git", "checkout", "-b", "new-branch"])
    return repo_dir


def git_log_length() -> int:
    git_log = subprocess.check_output(["git", "log", "--oneline"]).strip()
    return len(git_log.splitlines())


def get_HEAD_commit_oneline() -> str:
    return subprocess.check_output(["git", "log", "HEAD", "-1", "--oneline"],
                                   universal_newlines=True).strip()


def read_file(file_path: str) -> str:
    with open(file_path, "r", encoding='utf-8') as f:
        return f.read()


def assertRegexIn(pattern, sequence: Sequence[str]) -> re.Match:
    """Assert that >=1 string in the sequence matches the regex pattern.

    Returns:
        The matched object of the first matched string in sequence.
    """
    regex = re.compile(pattern)
    for s in sequence:
        m = regex.search(s)
        if m:
            return m
    raise AssertionError(
        f'No item in sequence {",".join(sequence)} matches regex '
        f'{pattern}.')


class TestCommandNotRun(unittest.TestCase):
    def setUp(self):
        """Create temporary git directory to be act as the libchrome directory.

        Containing a target file to be modified (init_file.txt) and patches to
        be applied.
        """
        logging.basicConfig(level=logging.DEBUG)
        logging.info("Running test: %s", self.id)
        self.repo_dir = init_git_repo_with_patches()
        self.repo_dir_path = self.repo_dir.name

    def tearDown(self):
        self.repo_dir.cleanup()


    def read_init_file(self) -> str:
        return read_file(os.path.join(self.repo_dir_path, "init_file.txt"))

    def test_dirty_git_repo(self):
        with open(os.path.join(self.repo_dir_path, "init_file.txt"),
                  "w",
                  encoding='utf-8') as f:
            f.write("dirty git repo\n")

        with self.assertRaisesRegex(
                RuntimeError, 'Git working directory is dirty. Abort script.'):
            apply_patches.assert_git_repo_state_and_get_current_branch()

        self.assertEqual(self.read_init_file(), "dirty git repo\n")

    def test_git_repo_no_branch(self):
        # Go to 'detached HEAD' state.
        head_hash = subprocess.check_output(["git", "log", "--oneline", "-1"],
                                            universal_newlines=True).split()[0]
        subprocess.check_call(["git", "checkout", head_hash])

        with self.assertRaisesRegex(RuntimeError,
                                    'Not on a branch. Abort script.'):
            apply_patches.assert_git_repo_state_and_get_current_branch()

        self.assertEqual(self.read_init_file(), "foo\n")


class TestSanitizePatchArgs(unittest.TestCase):
    def setUp(self):
        """Create temporary git directory to be act as the libchrome directory.

        Containing a target file to be modified (init_file.txt) and patches to
        be applied.
        """
        logging.basicConfig(level=logging.DEBUG)
        logging.info("Running test: %s", self.id)
        self.repo_dir = init_git_repo_with_patches(True)
        self.repo_dir_path = self.repo_dir.name

    def tearDown(self):
        self.repo_dir.cleanup()

    def first_patch_not_found(self):
        wrong_patch_name = 'wrong_patch_name.patch'
        with self.assertRaisesRegex(
                ValueError, "--first ({wrong_patch_name})) does not exist"):
            apply_patches.sanitize_patch_args(
                'first', wrong_patch_name, self.repo_dir_path)

    def last_patch_not_found(self):
        wrong_patch_name = 'wrong_patch_name.patch'
        with self.assertRaisesRegex(
                ValueError, "--last ({wrong_patch_name})) does not exist"):
            apply_patches.sanitize_patch_args(
                'last', wrong_patch_name, self.repo_dir_path)

    def patch_arg_in_wrong_dir(self):
        patch_path_with_wrong_dir = 'foo/wrong_patch_name.patch'
        with self.assertRaisesRegex(
                ValueError,
                f"--first ({patch_path_with_wrong_dir})) is given as a path "
                "but its parent directory is not"):
            apply_patches.apply_patches(
                'first', patch_path_with_wrong_dir, self.repo_dir_path)

    def invalid_file_as_patch_arg(self):
        # Add file without correct patch prefix to patches directory.
        invalid_file = "invalid_patch_name.patch"
        with open(invalid_file, "w") as f:
            f.write("testing\n")
        subprocess.check_call(["git", "commit", "-a", "--amend", "--no-edit"])

        with self.assertRaisesRegex(
                ValueError,
                f"{invalid_file} is not a valid patch: patch name must start "
                "with prefixes"):
            apply_patches.apply_patches(
                'first', invalid_file, self.repo_dir_path)


class TestApplyPatchGetPatchCommitsSinceTag(unittest.TestCase):
    def setUp(self) -> None:
        """Create temporary git directory to be act as the libchrome directory.

        Containing a target file to be modified (init_file.txt) and patches to
        be applied.
        """
        logging.basicConfig(level=logging.INFO)
        logging.info(self._testMethodName)
        self.repo_dir = init_git_repo_with_patches()
        self.repo_dir_path = self.repo_dir.name

    def tearDown(self):
        self.repo_dir.cleanup()

    def test_allow_no_patch_head_tag(self):
        """Tag HEAD commit when there is no HEAD-before-patching tag."""
        head_commit = get_HEAD_commit_oneline()
        with self.assertLogs("root", level='INFO') as context:
            patch_commits = apply_patches.get_patch_commits_since_tag(
                "new-branch")
        self.assertIn("INFO:root:Tagged current HEAD as HEAD-before-patching.",
                      context.output)
        self.assertEqual(patch_commits, [])

        patch_head = apply_patches.get_patch_head_commit()
        self.assertIsNotNone(patch_head)
        self.assertEqual(
            patch_head.split(maxsplit=1)[0],
            head_commit.split(maxsplit=1)[0])

    def test_forbid_no_patch_head_tag(self):
        """Abort when there is no HEAD-before-patching tag."""
        with self.assertRaisesRegex(
                RuntimeError,
                rf"Tag {apply_patches.TAG} does not exist\. Please run `git "
                rf"tag \<hash\> {apply_patches.TAG}` with hash being commit to "
                r"reset to if you have applied patches manually\."):
            apply_patches.get_patch_commits_since_tag("new-branch", False)

    def test_keep_existing_valid_patch_head_tag(self):
        """Skip tagging automatically if current tag is valid."""
        old_patch_head = get_HEAD_commit_oneline()
        old_patch_head_hash = old_patch_head.split(maxsplit=1)[0]
        subprocess.check_call(['git', 'tag', apply_patches.TAG])
        # Run apply_patches to get patch commits (only) in git log.
        apply_patches.apply_patches(self.repo_dir_path,
                                    ebuild=False,
                                    no_commit=False)

        with self.assertLogs("root", level='INFO') as context:
            patch_commits = apply_patches.get_patch_commits_since_tag(
                "new-branch")
        self.assertIn(
            f"INFO:root:Tag {apply_patches.TAG} already exists: "
            f"{old_patch_head}.", context.output)
        self.assertIn(
            f"INFO:root:Confirmed only patch commits from {apply_patches.TAG} "
            "to HEAD.", context.output)
        # Applied patch commits since HEAD-before-patching should be the same
        # as all patches in the patches directory.
        self.assertEqual([c.patch_name for c in patch_commits], [
            os.path.basename(p)
            for p in apply_patches.get_all_patches(self.repo_dir_path)
        ])

        # Commit tagged as HEAD-before-patching should remain the same.
        patch_head = apply_patches.get_patch_head_commit()
        self.assertIsNotNone(patch_head)
        self.assertEqual(patch_head.split(maxsplit=1)[0], old_patch_head_hash)

    def test_abort_patch_head_tag_with_non_patch_commits_in_history(self):
        """Abort when there is any non-patch commits."""
        # Tag HEAD as HEAD-before-patching and add commit without patch-name
        # trailer (i.e. non-patch commit).
        head_commit = get_HEAD_commit_oneline()
        subprocess.check_call(['git', 'tag', apply_patches.TAG])
        subprocess.check_call(
            ['git', 'commit', '-m', "empty non-patch commit", "--allow-empty"])
        empty_commit_hash = get_HEAD_commit_oneline().split(maxsplit=1)[0]

        with self.assertRaisesRegex(
                RuntimeError,
                re.compile(
                    fr"There is non-patch commit from {apply_patches.TAG} to "
                    r"HEAD:\n"
                    fr"{empty_commit_hash}.*: 0 patch-name trailer\(s\).",
                    re.MULTILINE)):
            apply_patches.get_patch_commits_since_tag("new-branch")

        # Commit tagged as HEAD-before-patching should remain the same.
        patch_head = apply_patches.get_patch_head_commit()
        self.assertIsNotNone(patch_head)
        self.assertEqual(
            patch_head.split(maxsplit=1)[0],
            head_commit.split(maxsplit=1)[0])

    def test_abort_patch_head_tag_with_patch_commits_in_wrong_order(self):
        """Abort if the commits are in a different order than at built time."""
        # Tag HEAD as HEAD-before-patching and add empty commits with patch-name
        # trailer such that they are in wrong order (backward compatibility
        # patches before long term patches).
        head_commit = get_HEAD_commit_oneline()
        subprocess.check_call(['git', 'tag', apply_patches.TAG])
        subprocess.check_call([
            'git', 'commit', '--allow-empty', '-m',
            "backward compatibility patch commit", '-m', '', '-m',
            'patch-name: backward-compatibility-0001-empty.patch'
        ])
        subprocess.check_call([
            'git', 'commit', '--allow-empty', '-m', "long term patch commit",
            '-m', '', '-m', 'patch-name: long-term-0005-empty.patch'
        ])

        with self.assertRaisesRegex(
                RuntimeError,
                r"Applied patches in git log are not in correct application "
                r"order. This may cause unexpected apply failure when `emerge "
                r"libchrome` with the generated patches."):
            apply_patches.get_patch_commits_since_tag("new-branch")

        # Commit tagged as HEAD-before-patching should remain the same.
        patch_head = apply_patches.get_patch_head_commit()
        self.assertIsNotNone(patch_head)
        self.assertEqual(
            patch_head.split(maxsplit=1)[0],
            head_commit.split(maxsplit=1)[0])


class TestClampPatchesNoAppliedPatchCommits(unittest.TestCase):
    def setUp(self):
        logging.basicConfig(level=logging.DEBUG)
        logging.info("Running test: %s", self.id)
        self.patches = [
            "long-term-0000-foo.patch",
            "long-term-0001-bar.patch",
            "backward-compatibility-0001-baz.patch",
        ]

    def test_no_clamp(self):
        clamped_patches = apply_patches.clamp_patches(self.patches, [], None,
                                                      None)
        self.assertEqual(clamped_patches, self.patches)

    def test_clamp_only_first_specified(self):
        clamped_patches = apply_patches.clamp_patches(self.patches, [],
                                                      self.patches[-1], None)
        self.assertEqual(clamped_patches, self.patches[-1:])

    def test_clamp_only_last_specified(self):
        clamped_patches = apply_patches.clamp_patches(self.patches, [], None,
                                                      self.patches[1])
        self.assertEqual(clamped_patches, self.patches[:2])

    def test_clamp_first_last_specified(self):
        clamped_patches = apply_patches.clamp_patches(self.patches, [],
                                                      self.patches[1],
                                                      self.patches[1])
        self.assertEqual(clamped_patches, self.patches[1:2])

    def test_clamp_first_after_last(self):
        with self.assertRaises(
                RuntimeError,
                msg=
                f"Invalid input --last {self.patches[0]}: last patch to apply "
                "should not be applied before the first one "
                f"({self.patches[1]})."):
            apply_patches.clamp_patches(self.patches, [], self.patches[1],
                                        self.patches[0])


def _patch_name_to_patch_commit(patch_name: str) -> apply_patches.PatchCommit:
    return apply_patches.PatchCommit('mockHash', patch_name)


class TestClampPatchesWithAppliedPatchCommitsInHistory(unittest.TestCase):
    def setUp(self):
        logging.basicConfig(level=logging.DEBUG)
        logging.info("Running test: %s", self.id)
        self.patches = [
            "long-term-0000-foo.patch",
            "long-term-0001-bar.patch",
            "backward-compatibility-0001-baz.patch",
        ]

    def test_no_clamp(self):
        # Apply all patches after the first one which is already in history.
        clamped_patches = apply_patches.clamp_patches(
            self.patches,
            [_patch_name_to_patch_commit(pn)
             for pn in self.patches[:1]], None, None)
        self.assertEqual(clamped_patches, self.patches[1:])

    def test_clamp_only_first_specified(self):
        # Apply all patches after the specified --first.
        # Skipping (the second) patch is allowed as long as order is right.
        clamped_patches = apply_patches.clamp_patches(
            self.patches,
            [_patch_name_to_patch_commit(pn)
             for pn in self.patches[:1]], self.patches[2], None)
        self.assertEqual(clamped_patches, self.patches[2:])

    def test_clamp_only_last_specified(self):
        # Apply the second patch only -- first is already in history and second
        # is specified to be --last.
        clamped_patches = apply_patches.clamp_patches(
            self.patches,
            [_patch_name_to_patch_commit(pn)
             for pn in self.patches[:1]], None, self.patches[1])
        self.assertEqual(clamped_patches, self.patches[1:2])

    def test_clamp_first_last_specified(self):
        # Apply all patches after the specified --first and until --last.
        clamped_patches = apply_patches.clamp_patches(
            self.patches,
            [_patch_name_to_patch_commit(pn)
             for pn in self.patches[:1]], self.patches[2], self.patches[2])
        self.assertEqual(clamped_patches, self.patches[2:])

    def test_clamp_first_not_after_last_applied_patch(self):
        with self.assertRaises(
                RuntimeError,
                msg=f"Invalid input --first {self.patches[0]}: first patch to "
                "apply should be applied after the last patch commit "
                f"({self.patches[0]}) in history."):
            apply_patches.clamp_patches(
                self.patches,
                [_patch_name_to_patch_commit(pn)
                 for pn in self.patches[:1]], self.patches[0], None)

    def test_clamp_last_not_after_last_applied_patch(self):
        with self.assertRaises(
                RuntimeError,
                msg=
                f"Invalid input --last {self.patches[0]}: last patch to apply "
                "should not be applied before the first one "
                f"({self.patches[1]})."):
            apply_patches.clamp_patches(
                self.patches,
                [_patch_name_to_patch_commit(pn)
                 for pn in self.patches[:2]], None, self.patches[0])


class TestApplyPatchesSucceed(unittest.TestCase):
    def setUp(self):
        """Create temporary git directory to be act as the libchrome directory.

        Containing a target file to be modified (init_file.txt) and patches to
        be applied.
        """
        logging.basicConfig(level=logging.DEBUG)
        logging.info("Running test: %s", self.id)
        self.repo_dir = init_git_repo_with_patches()
        self.repo_dir_path = self.repo_dir.name

    def tearDown(self):
        self.repo_dir.cleanup()

    def read_init_file(self) -> str:
        return read_file(os.path.join(self.repo_dir_path, "init_file.txt"))

    def test_dryrun(self):
        apply_patches.apply_patches(self.repo_dir_path,
                                    ebuild=False,
                                    no_commit=True,
                                    dry_run=True)
        # Initial commit only.
        self.assertEqual(git_log_length(), 1)
        # File content unchanged.
        self.assertEqual(self.read_init_file(), "foo\n")

    def test_default_apply(self):
        head_commit = get_HEAD_commit_oneline()
        apply_patches.apply_patches(self.repo_dir_path,
                                    ebuild=False,
                                    no_commit=False)
        self.assertEqual(self.read_init_file(), "bar\nbaz\nfoo\n")
        # Initial commit and 3 commits from patches.
        self.assertEqual(git_log_length(), 4)
        # Assert commit message contains trailer for patch name.
        patch_name_trailers = subprocess.check_output(
            [
                'git', 'log', '--format=%(trailers:key=patch-name,valueonly)',
                '-n3'
            ],
            universal_newlines=True,
        ).strip().split('\n\n')
        self.assertEqual(patch_name_trailers, [
            'backward-compatibility-0500-add-foo-to-end-of-file.patch',
            'long-term-0100-remove-foo.patch',
            'long-term-0000-add-bar-and-baz.patch',
        ])

    def test_default_apply_patch_overwrite_trailer_with_new_name(self):
        # Add trailer with a different patch-name than its file name to patch.
        trailer_patch_name = "backward-compatibility-0500-old-patch-name.patch"
        patch_file_name = (
            "backward-compatibility-0500-add-foo-to-end-of-file.patch")
        subprocess.check_call([
            "git", "interpret-trailers", "--trailer",
            f"patch-name: {trailer_patch_name}", "--in-place",
            os.path.join("libchrome_tools", "patches", patch_file_name)
        ])
        subprocess.check_call(["git", "commit", "-a", "--amend", "--no-edit"])

        # Assert warning-level message is logged for overwriting trailer value.
        with self.assertLogs("root", level='WARNING') as context:
            apply_patches.apply_patches(self.repo_dir_path,
                                        ebuild=False,
                                        no_commit=False,
                                        dry_run=False)
        self.assertIn(
            "WARNING:root:Applied patch contains patch-name trailers "
            f"(['{trailer_patch_name}']) different from filename "
            f"({patch_file_name}). Overwriting with filename.", context.output)

        # Assert last commit's message has only one patch-name trailer and value
        # is its current filename.
        patch_name_trailers = subprocess.check_output(
            [
                'git', 'log', '--format=%(trailers:key=patch-name,valueonly)',
                '-n1'
            ],
            universal_newlines=True,
        ).strip().split('\n\n', maxsplit=1)[0].split('\n')
        self.assertEqual(len(patch_name_trailers), 1)
        self.assertEqual(patch_name_trailers[0], patch_file_name)

    def test_apply_with_first_and_last_args(self):
        apply_patches.apply_patches(
            self.repo_dir_path,
            ebuild=False,
            no_commit=False,
            last='long-term-0000-add-bar-and-baz.patch')
        self.assertEqual(self.read_init_file(), "foo\nbar\nbaz\n")
        # Initial commit and 1 commit from the first patch.
        self.assertEqual(git_log_length(), 2)

        apply_patches.apply_patches(self.repo_dir_path,
                                    ebuild=False,
                                    no_commit=False,
                                    first='long-term-0100-remove-foo.patch',
                                    last='long-term-0100-remove-foo.patch')
        self.assertEqual(self.read_init_file(), "bar\nbaz\n")
        # Initial commit and 2 commits (applied in two different runs).
        self.assertEqual(git_log_length(), 3)

        apply_patches.apply_patches(
            self.repo_dir_path,
            ebuild=False,
            no_commit=False,
            first='backward-compatibility-0500-add-foo-to-end-of-file.patch')
        self.assertEqual(self.read_init_file(), "bar\nbaz\nfoo\n")
        # Initial commit and 3 commits (applied in three different runs).
        self.assertEqual(git_log_length(), 4)

    def test_no_commit_apply(self):
        apply_patches.apply_patches(self.repo_dir_path,
                                    ebuild=False,
                                    no_commit=True)
        self.assertEqual(self.read_init_file(), "bar\nbaz\nfoo\n")
        # Initial commit only.
        self.assertEqual(git_log_length(), 1)

    def test_ebuild_apply(self):
        apply_patches.apply_patches(self.repo_dir_path,
                                    ebuild=True,
                                    no_commit=True)
        self.assertEqual(self.read_init_file(), "bar\nbaz\nfoo\n")


class TestApplyPatchesFail(unittest.TestCase):
    def setUp(self):
        """Create temporary git directory to be act as the libchrome directory.

        Containing a target file to be modified (init_file.txt) and patches to
        be applied.
        """
        logging.basicConfig(level=logging.DEBUG)
        logging.info("Running test: %s", self.id)
        self.repo_dir = init_git_repo_with_patches(inject_error=True)
        self.repo_dir_path = self.repo_dir.name

    def tearDown(self):
        self.repo_dir.cleanup()

    def read_init_file(self) -> str:
        return read_file(os.path.join(self.repo_dir_path, "init_file.txt"))

    def test_default_apply(self):
        with self.assertRaisesRegex(
                RuntimeError,
                'Failed to git am patch long-term-0000-add-bar-and-baz.patch'):
            apply_patches.apply_patches(self.repo_dir_path,
                                        ebuild=False,
                                        no_commit=False)
        # Initial commit only with 3-way merge markers.
        self.assertEqual(git_log_length(), 1)
        self.assertEqual(
            self.read_init_file(), "<<<<<<< HEAD\n"
            "wrong line\n"
            "=======\n"
            "foo\n"
            "bar\n"
            "baz\n"
            ">>>>>>> Add bar and baz\n")

    def test_no_commit_apply(self):
        # Assert the git apply command failed with --3way option.
        with self.assertRaisesRegex(
                RuntimeError,
                'Failed to git apply patch long-term-0000-add-bar-and-baz.patch'
        ):
            apply_patches.apply_patches(self.repo_dir_path,
                                        ebuild=False,
                                        no_commit=True)

        # Initial commit only and file now contains 3-way merge markers.
        self.assertEqual(git_log_length(), 1)
        self.assertEqual(
            self.read_init_file(), "<<<<<<< ours\n"
            "wrong line\n"
            "=======\n"
            "foo\n"
            "bar\n"
            "baz\n"
            ">>>>>>> theirs\n")

    def test_ebuild_apply(self):
        # Assert the function failed with self-defined RuntimeError.
        with self.assertRaisesRegex(
                subprocess.CalledProcessError, r"\['git', 'apply', '-C1', '"
                r".*/long-term-0000-add-bar-and-baz.patch'\]"):
            apply_patches.apply_patches(self.repo_dir_path,
                                        ebuild=True,
                                        no_commit=True)

        # Initial commit only and file is not modified.
        self.assertEqual(git_log_length(), 1)
        self.assertEqual(self.read_init_file(), "wrong line\n")


class TestFormatPatchesSucceed(unittest.TestCase):
    def setUp(self):
        """Create temporary git directory to be act as the libchrome directory.

        Containing a target file to be modified (init_file.txt) and patches to
        be applied.

        Apply all patches.
        """
        logging.basicConfig(level=logging.DEBUG)
        logging.info("Running test: %s", self.id)
        self.repo_dir = init_git_repo_with_patches()
        self.repo_dir_path = self.repo_dir.name
        self.HEAD_before_patching = get_HEAD_commit_oneline()
        apply_patches.apply_patches(self.repo_dir_path,
                                    ebuild=False,
                                    no_commit=False)

    def tearDown(self):
        self.repo_dir.cleanup()

    def read_init_file(self) -> str:
        return read_file(os.path.join(self.repo_dir_path, "init_file.txt"))

    def test_basic_run(self):
        # Amend last patch to write two "foo"'s instead of one.
        with open(os.path.join(self.repo_dir_path, "init_file.txt"),
                  "a",
                  encoding='utf-8') as f:
            f.write("foo\n")
        subprocess.check_call(["git", "commit", "-a", "--amend", "--no-edit"])

        apply_patches.format_patches(self.repo_dir_path)

        # Check git repo has been reset to original commit.
        current_HEAD = get_HEAD_commit_oneline()
        self.assertEqual(current_HEAD, self.HEAD_before_patching)
        # File should contain original content.
        self.assertEqual(self.read_init_file(), "foo\n")
        # HEAD-before-patching tag no longer exists.
        patch_head = apply_patches.get_patch_head_commit()
        self.assertIsNone(patch_head)

        os.chdir(os.path.join(self.repo_dir_path, "libchrome_tools",
                              "patches"))
        # Get summary of uncommitted changes.
        git_status_short = subprocess.check_output(
            ["git", "status", "--short"],
            universal_newlines=True).splitlines()
        self.assertEqual(len(git_status_short), 3)
        # Each line is status of file (e.g. M for modified, D for deleted)
        # followed by file name.
        git_status_short = [line.split() for line in git_status_short]
        # All files are modified (hash of patch file changes although no change
        # to patch content).
        self.assertSetEqual(set(s for (s, _) in git_status_short), set("M"))
        self.assertSetEqual(set(f for (_, f) in git_status_short),
                            set(os.listdir(".")))

        # Save changes of new patches and reapply them.
        subprocess.check_call(["git", "commit", "-a", "--amend", "--no-edit"])
        apply_patches.apply_patches(self.repo_dir_path,
                                    ebuild=False,
                                    no_commit=False)

        # The file now contains two foo's.
        self.assertEqual(self.read_init_file(), "bar\nbaz\nfoo\nfoo\n")

    def test_with_renamed_patch(self):
        old_patch_name = (
            "backward-compatibility-0500-add-foo-to-end-of-file.patch")
        new_patch_name = "backward-compatibility-0100-add-foo.patch"
        # Change last patch's patch-name trailer value.
        subprocess.check_call([
            "git", "-c", "trailer.ifexists=replace", "commit", "--amend",
            "--no-edit", "--trailer", f"patch-name: {new_patch_name}"
        ])

        apply_patches.format_patches(self.repo_dir_path)

        os.chdir(os.path.join(self.repo_dir_path, "libchrome_tools",
                              "patches"))
        # Get summary of uncommitted changes.
        git_status_short = subprocess.check_output(
            ["git", "status", "--short"],
            universal_newlines=True).splitlines()
        self.assertEqual(len(git_status_short), 3)
        # Each line is status of file (e.g. M for modified, D for deleted)
        # followed by file name.
        git_status_short = [line.split() for line in git_status_short]
        # Local changes includes the new patch file as untracked file, and not
        # the old patch file.
        self.assertIn(["??", new_patch_name], git_status_short)
        self.assertNotIn(old_patch_name, [f for (_, f) in git_status_short])

    def test_backup_branch(self):
        with self.assertLogs("root", level='INFO') as context:
            apply_patches.format_patches(self.repo_dir_path,
                                         backup_branch=True)
        backup_branch = assertRegexIn(
            r"Backed up git history to branch (apply-patch-backup-\d{14}).",
            context.output).group(1)

        # Check that the backup branch indeed exists.
        branches = subprocess.check_output(
            ["git", "branch", "--format=%(refname:short)"],
            universal_newlines=True).splitlines()
        self.assertIn(backup_branch, branches)

        # Go to backup branch.
        subprocess.check_call(["git", "checkout", backup_branch])
        # Check that the initial commit and 3 patch commits are in history.
        self.assertEqual(git_log_length(), 4)
        patch_name_trailers = subprocess.check_output(
            [
                'git', 'log', '--format=%(trailers:key=patch-name,valueonly)',
                '-n3'
            ],
            universal_newlines=True,
        ).strip().split('\n\n')
        self.assertEqual(patch_name_trailers, [
            'backward-compatibility-0500-add-foo-to-end-of-file.patch',
            'long-term-0100-remove-foo.patch',
            'long-term-0000-add-bar-and-baz.patch',
        ])
        # Check that file content is the same as after applying the 3 patches.
        self.assertEqual(self.read_init_file(), "bar\nbaz\nfoo\n")
