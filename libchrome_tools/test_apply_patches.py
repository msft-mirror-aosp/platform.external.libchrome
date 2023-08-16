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


class TestApplyPatchesSucceed(unittest.TestCase):
    def setUp(self):
        """Create temporary git directory to be act as the libchrome directory.

        Containing a target file to be modified (init_file.txt) and patches to
        be applied.
        """
        logging.basicConfig(level=logging.DEBUG)
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
                                    no_commit=False,
                                    dry_run=False)
        self.assertEqual(self.read_init_file(), "bar\nbaz\nfoo\n")
        # Initial commit and 3 commits from patches.
        self.assertEqual(git_log_length(), 4)

    def test_no_commit_apply(self):
        apply_patches.apply_patches(self.repo_dir_path,
                                    ebuild=False,
                                    no_commit=True,
                                    dry_run=False)
        self.assertEqual(self.read_init_file(), "bar\nbaz\nfoo\n")
        # Initial commit only.
        self.assertEqual(git_log_length(), 1)

    def test_ebuild_apply(self):
        apply_patches.apply_patches(self.repo_dir_path,
                                    ebuild=True,
                                    no_commit=True,
                                    dry_run=False)
        self.assertEqual(self.read_init_file(), "bar\nbaz\nfoo\n")


class TestApplyPatchesFail(unittest.TestCase):
    def setUp(self):
        """Create temporary git directory to be act as the libchrome directory.

        Containing a target file to be modified (init_file.txt) and patches to
        be applied.
        """
        logging.basicConfig(level=logging.DEBUG)
        self.repo_dir = init_git_repo_with_patches(inject_error=True)
        self.repo_dir_path = self.repo_dir.name

    def tearDown(self):
        self.repo_dir.cleanup()

    def read_init_file(self) -> str:
        return read_file(os.path.join(self.repo_dir_path, "init_file.txt"))

    def test_default_apply(self):
        with self.assertRaisesRegex(
                RuntimeError, 'Failed to git am patch '
                'libchrome_tools/patches/long-term-0000-add-bar-and-baz.patch'
        ):
            apply_patches.apply_patches(self.repo_dir_path,
                                        ebuild=False,
                                        no_commit=False,
                                        dry_run=False)
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
                RuntimeError, 'Failed to git apply patch '
                'libchrome_tools/patches/long-term-0000-add-bar-and-baz.patch'
        ):
            apply_patches.apply_patches(self.repo_dir_path,
                                        ebuild=False,
                                        no_commit=True,
                                        dry_run=False)

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
                r"libchrome_tools/patches/long-term-0000-add-bar-and-baz.patch"
                r"'\]"):
            apply_patches.apply_patches(self.repo_dir_path,
                                        ebuild=True,
                                        no_commit=True,
                                        dry_run=False)

        # Initial commit only and file is not modified.
        self.assertEqual(git_log_length(), 1)
        self.assertEqual(self.read_init_file(), "wrong line\n")