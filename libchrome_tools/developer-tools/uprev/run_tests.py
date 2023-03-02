#!/usr/bin/env python3
# Copyright 2020 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Run ./run_tests.py to run all unittests
"""

import unittest


def load_tests(loader, tests, pattern):
    return loader.discover('.')


if __name__ == '__main__':
    unittest.main()
