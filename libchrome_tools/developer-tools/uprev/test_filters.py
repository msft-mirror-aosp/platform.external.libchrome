#!/usr/bin/env python3
# Copyright 2021 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re
import unittest

import filters
import utils


class TestFilters(unittest.TestCase):

    def _build_file_list(self, files):
        return [
            utils.GitFile(path.encode(), None, hash(path)) for path in files
        ]

    def test_filters(self):
        test_filter = filters.Filter([re.compile(rb'want.*')],
                                     [re.compile(rb'want_excluded.*')],
                                     [re.compile(rb'want_excluded_always.*')])

        self.assertEquals(
            test_filter.filter_files(
                self._build_file_list([
                    'want/xxx',
                    'want_excluded/xxx',
                    'want_excluded_always/xxx',
                    'unrelated_upstream_file',
                ])),
            self._build_file_list(
                ['want/xxx', 'want_excluded_always/xxx']),
        )

    def test_path_filter(self):
        test_filter = filters.Filter([filters.PathFilter([b'a/b/c', b'd'])], [],
                                     [])

        self.assertEquals(
            test_filter.filter_files(
                self._build_file_list(
                    ['a', 'b', 'c', 'a/b', 'b/c', 'a/b/c', 'd'])),
            self._build_file_list(['a/b/c', 'd']))
