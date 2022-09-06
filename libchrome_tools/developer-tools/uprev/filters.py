# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Provide filters for libchrome tools."""


class PathFilter:
    """Provides a _sre.SRE_Pattern like class that matches a list of paths."""

    def __init__(self, paths):
        """Initializes an instance to match with the given paths.

        Args:
            paths: paths to match, must be list of bytes.
        """
        self.paths = paths

    def match(self, what):
        """Returns if what matches any files.

        Args:
            what: paths to look for, must be bytes.
        """
        return what in self.paths


class Filter:
    """
    Provide filter functions for libchrome uprev tools
    """

    def __init__(self, want, want_excluded, always_want):
        """
        Initialize filters with given filter rules.
        """
        self.want = want
        self.want_excluded = want_excluded
        self.always_want = always_want

    def want_file(self, path):
        """Returns whether the path wants to be a new file."""
        wanted = False
        for want_file_regex in self.want:
            if want_file_regex.match(path):
                wanted = True
                break
        for exclude_file_regex in self.want_excluded:
            if exclude_file_regex.match(path):
                wanted = False
                break
        for always_want_file_regex in self.always_want:
            if always_want_file_regex.match(path):
                wanted = True
                break
        return wanted

    def filter_files(self, upstream_files):
        """Generates a list of files we want based on hard-coded rules.

        File list must be a list of GitFile.

        Args:
            upstream_files: files in Chromium browser repository.
        """

        files = []
        for upstream_file in upstream_files:
            if self.want_file(upstream_file.path):
                files.append(upstream_file)
        return files

    def filter_diff(self, diff):
        """Returns a subset of diff, after running filters.

        Args:
            diff: diff to filter. diff contains list of utils.GitDiffTree
        """
        filtered = []
        for change in diff:
            path = change.file.path
            if self.want_file(path):
                filtered.append(change)
        return filtered
