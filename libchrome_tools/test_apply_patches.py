#!/usr/bin/env python
# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import subprocess
import tempfile
import unittest
from pathlib import Path
from shutil import copy

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
    git_log = subprocess.check_output(["git", "log", "--oneline"])
    return len(git_log.splitlines())


def read_file(file_path: str) -> str:
    with open(file_path, "r", encoding='utf-8') as f:
        return f.read()


class TestCommandNotRun(unittest.TestCase):
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


    def read_init_file(self) -> str:
        return read_file(os.path.join(self.repo_dir_path, "init_file.txt"))

    def test_dirty_git_repo(self):
        with open(os.path.join(self.repo_dir_path, "init_file.txt"),
                  "w",
                  encoding='utf-8') as f:
            f.write("dirty git repo\n")

        with self.assertRaisesRegex(
                RuntimeError,
                'Git working directory is dirty. Abort applying patches.'
        ):
            apply_patches.apply_patches(self.repo_dir_path,
                                        ebuild=False,
                                        no_commit=False,
                                        dry_run=False)

        self.assertEqual(self.read_init_file(), "dirty git repo\n")


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
            apply_patches.apply_patches(self.repo_dir_path,
                                        ebuild=False,
                                        no_commit=False,
                                        first=wrong_patch_name)

    def last_patch_not_found(self):
        wrong_patch_name = 'wrong_patch_name.patch'
        with self.assertRaisesRegex(
                ValueError, "--last ({wrong_patch_name})) does not exist"):
            apply_patches.apply_patches(self.repo_dir_path,
                                        ebuild=False,
                                        no_commit=False,
                                        last=wrong_patch_name)

    def patch_arg_in_wrong_dir(self):
        patch_path_with_wrong_dir = 'foo/wrong_patch_name.patch'
        with self.assertRaisesRegex(
                ValueError,
                f"--first ({patch_path_with_wrong_dir})) is given as a path "
                "but its parent directory is not"):
            apply_patches.apply_patches(self.repo_dir_path,
                                        ebuild=False,
                                        no_commit=False,
                                        first=patch_path_with_wrong_dir)

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
            apply_patches.apply_patches(self.repo_dir_path,
                                        ebuild=False,
                                        no_commit=False,
                                        first=invalid_file)


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
        patch_file_name = "backward-compatibility-0500-add-foo-to-end-of-file.patch"
        subprocess.check_call([
            "git", "interpret-trailers", "--trailer",
            f"patch-name: {trailer_patch_name}", "--in-place",
            os.path.join("libchrome_tools", "patches", patch_file_name)
        ])
        subprocess.check_call(["git", "commit", "-a", "--amend", "--no-edit"])

        # Assert warning-level message is logged for overwriting trailer value.
        with self.assertLogs("root", level='WARNING') as cm:
            apply_patches.apply_patches(self.repo_dir_path,
                                        ebuild=False,
                                        no_commit=False,
                                        dry_run=False)
        self.assertIn(
            "WARNING:root:Applied patch contains patch-name trailers "
            f"(['{trailer_patch_name}']) different from filename "
            f"({patch_file_name}). Overwriting with filename.", cm.output)

        # Assert last commit's message has only one patch-name trailer and value
        # is its current filename.
        patch_name_trailers = subprocess.check_output(
            [
                'git', 'log', '--format=%(trailers:key=patch-name,valueonly)',
                '-n1'
            ],
            universal_newlines=True,
        ).strip().split('\n\n')[0].split('\n')
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
        # Initial commit and 2 commits (applied in three different runs).
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
