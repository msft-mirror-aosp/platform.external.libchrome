#!/usr/bin/env python
# Copyright 2022 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import glob
import os
import shutil
import subprocess

TEST_IMAGE_FILE_NAME = 'chromiumos_test_image'


def _get_latest_release(board):
    """
    Returns the latest release build version.

    Args:
      board: board name in str.
    """
    return subprocess.check_output([
        'gsutil', 'cat',
        'gs://chromeos-image-archive/%s-release/LATEST-main' % (board)
    ]).decode('ascii').strip()


def download_test_image(board, version, output):
    """
    Downloads the test image to given path.

    Args:
      board: board name in str.
      version: build version in str.
      output: location to store the downloaded file.
    """
    subprocess.check_call([
        'gsutil', 'cp',
        'gs://chromeos-image-archive/%s-release/%s/%s.tar.xz' %
        (board, version, TEST_IMAGE_FILE_NAME), output
    ])


class MountedReleaseImage:
    """Stateful class to maintain image downloads and mounts."""

    def __init__(self, board, basedir):
        """
        Initializes MountedReleaseImage

        Args:
          board: board name in str.
          basedir: a temporary dir for the class to work on.
        """
        self.board = board
        self.basedir = basedir
        self.version = _get_latest_release(board)
        self.imagetarball = os.path.join(self.basedir,
                                         '%s-image.tar.xz' % (self.board))
        self.imagedir = os.path.join(self.basedir, '%s-image' % (self.board))
        self.rootfs = os.path.join(self.imagedir, 'rootfs')
        self.stateful = os.path.join(self.imagedir, 'stateful')

    def _download_latest_release_image(self):
        """Downloads and extracts the latest release build test image."""
        os.makedirs(self.imagedir, exist_ok=True)
        download_test_image(self.board, self.version, self.imagetarball)
        subprocess.check_call(
            ['tar', 'xf', self.imagetarball, '-C', self.imagedir])

    def _mount_latest_release_image(self, umount=False):
        """
        Mounts the latest test image.

        Args:
          umount: umount instead of mount.
        """
        commands = [
            os.path.join(os.environ.get('HOME'),
                         'trunk/src/scripts/mount_gpt_image.sh'), '-i',
            '%s.bin' % (TEST_IMAGE_FILE_NAME), '-f', self.imagedir,
            '--rootfs_mountpt', self.rootfs, '--stateful_mountpt', self.stateful
        ]
        if umount:
            commands.append('-u')
        subprocess.check_call(commands),

    def _umount_image(self, fromerror=False):
        """Unmounts the test image."""
        try:
            self._mount_latest_release_image(umount=True)
        except subprocess.CalledProcessError as e:
            if not fromerror:
                raise e

    def _prepare_rootfs_dir(self):
        """Prepares mounted rootfs for latest test image."""
        # self.basedir must not exist to prevent accidental deletion at cleanup.
        os.makedirs(self.basedir)
        self._download_latest_release_image()
        self._mount_latest_release_image()

    def _cleanup(self, fromerror=False):
        """Cleans up basedir."""
        self._umount_image(fromerror)
        shutil.rmtree(self.basedir)

    def __enter__(self):
        """Prepares rootfs dir for latest image and enters with block."""
        try:
            self._prepare_rootfs_dir()
        except Exception as e:
            self._cleanup(fromerror=True)
            raise e
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        """Cleans up basedir and exits with block."""
        self._cleanup()

    def path(self, path):
        """
        Returns path prepended by rootfs path.

        Args:
          path: relative path to image rootfs.
        """
        return os.path.join(self.rootfs, path)


class FakeMountedReleaseImage:
    """
    A fake provider for MountedReleaseImage.

    To use the fake, test image but be mounted to /tmp/m by mount_gpt_image.sh
    manually.
    """

    def __init__(self, board, basedir):
        """Initializes FakeMountedReleaseImage. All args are dropped"""
        self.rootfs = '/tmp/m'

    def __enter__(self):
        """Enters with block."""
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        """Exits with block."""
        pass

    def path(self, path):
        """
        Returns path prepended by rootfs path.

        Args:
          path: relative path to image rootfs.
        """
        return os.path.join(self.rootfs, path)
