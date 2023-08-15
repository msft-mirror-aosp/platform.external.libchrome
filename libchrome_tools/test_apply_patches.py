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


class TestRunCommand(unittest.TestCase):
    def setUp(self):
        """Create temporary git directory to be act as the libchrome directory.

        Containing a target file to be modified (init_file.txt) and patches to
        be applied.
        """
        logging.basicConfig(level=logging.DEBUG)
        # Create tmp directory as a git repository (acting-libchrome) with
        # patches directory structure.
        self.repo_dir = tempfile.TemporaryDirectory()
        self.repo_dir_path = self.repo_dir.name
        logging.debug('temporary directory at: ', self.repo_dir_path)
        self.patch_dir = os.path.join(self.repo_dir_path, 'libchrome_tools/',
                                      'patches/')
        os.makedirs(self.patch_dir)

        # Copy all files in local test data directory to tmp directory.
        os.chdir(
            os.path.join(
                Path(__file__).resolve().parent, 'testdata', 'apply_patches'))
        for patch in os.listdir():
            copy(patch, self.patch_dir)

        # Change to tmp directory for git commands. Make initial commit.
        os.chdir(self.repo_dir_path)
        subprocess.check_call(["git", "init"])
        with open(os.path.join(self.repo_dir_path, "init_file.txt"),
                  "w",
                  encoding='utf-8') as f:
            f.write("foo\n")
        subprocess.check_call(["git", "add", "."])
        subprocess.check_call(["git", "commit", "-m", "initial commit"])
        subprocess.check_call(["git", "checkout", "-b", "new-branch"])

    def tearDown(self):
        self.repo_dir.cleanup()

    def git_log_length(self) -> int:
        git_log = subprocess.check_output(["git", "log", "--oneline"])
        return len(git_log.splitlines())

    def read_init_file(self) -> str:
        with open(os.path.join(self.repo_dir_path, "init_file.txt"),
                  "r",
                  encoding='utf-8') as f:
            file_content = f.read()
            return file_content

    def test_dryrun(self):
        apply_patches.apply_patches(self.repo_dir_path,
                                    ebuild=False,
                                    dry_run=True)
        # Initial commit only.
        self.assertEqual(self.git_log_length(), 1)
        # File content unchanged.
        self.assertEqual(self.read_init_file(), "foo\n")

    def test_default_apply_success(self):
        apply_patches.apply_patches(self.repo_dir_path,
                                    ebuild=False,
                                    dry_run=False)
        self.assertEqual(self.read_init_file(), "bar\nbaz\nfoo\n")
        # Initial commit and 3 commits from patches.
        self.assertEqual(self.git_log_length(), 1)

    def test_default_apply_fail(self):
        # Replace "foo" by "wrong line" so that the first patch will fail to
        # apply.
        with open(os.path.join(self.repo_dir_path, "init_file.txt"),
                  "w",
                  encoding='utf-8') as f:
            f.write("wrong line")
        subprocess.check_call(["git", "commit", "-a", "--amend", "--no-edit"])

        # Assert the git apply command failed with --3way option.
        with self.assertRaisesRegex(
                subprocess.CalledProcessError,
                r"'git', 'apply', '-C1', 'libchrome_tools/patches/long-term-0000-add-bar-and-baz.patch', '--3way'"
        ):
            apply_patches.apply_patches(self.repo_dir_path,
                                        ebuild=False,
                                        dry_run=False)

        # Initial commit only and file now contains 3-way merge markers.
        self.assertEqual(self.git_log_length(), 1)
        self.assertEqual(
            self.read_init_file(), "<<<<<<< ours\n"
            "wrong line\n"
            "=======\n"
            "foo\n"
            "bar\n"
            "baz\n"
            ">>>>>>> theirs\n")

    def test_ebuild_apply_success(self):
        apply_patches.apply_patches(self.repo_dir_path,
                                    ebuild=True,
                                    dry_run=False)
        self.assertEqual(self.read_init_file(), "bar\nbaz\nfoo\n")

    def test_ebuild_apply_fail(self):
        # Replace "foo" by "wrong line" so that the first patch will fail to
        # apply.
        with open(os.path.join(self.repo_dir_path, "init_file.txt"),
                  "w",
                  encoding='utf-8') as f:
            f.write("wrong line\n")
        subprocess.check_call(["git", "commit", "-a", "--amend", "--no-edit"])

        # Assert the function failed with self-defined RuntimeError.
        with self.assertRaisesRegex(
                RuntimeError,
                'Failed to apply patch libchrome_tools/patches/long-term-0000-add-bar-and-baz.patch.'
        ):
            apply_patches.apply_patches(self.repo_dir_path,
                                        ebuild=True,
                                        dry_run=False)

        # Initial commit only and file is not modified.
        self.assertEqual(self.git_log_length(), 1)
        self.assertEqual(self.read_init_file(), "wrong line\n")
