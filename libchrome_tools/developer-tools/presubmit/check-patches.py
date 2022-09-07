#!/usr/bin/env python3
# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Utility to check if libchrome_tools/patches/patches follow the convention.
"""

from __future__ import print_function

import argparse
import os
import re
import subprocess
import sys

MODE_NONE = 0
MODE_CHERRY_PICK = 1
MODE_BACKWARD_COMPATIBILITY = 2


def checkPatchesFileNameConvention(commit):
    '''Check if libchrome_tools/patches/patches file follow the convention.

    Args:
      commit: hash of a commit. If given, only lines added to this commit will be

    Returns:
      A list of error messages reporting bad patches settings.
    '''
    errors = []
    patches_list_file = subprocess.check_output(
        ['git', 'show',
         '%s:%s' % (commit, 'libchrome_tools/patches/patches')])

    mode = MODE_NONE
    cherry_pick_seen = False
    backward_seen = False
    for line in patches_list_file.splitlines():
        line = line.decode('utf-8')
        if not line:
            continue
        if line.startswith('#'):
            if '===== CHERRY PICKS =====' in line:
                cherry_pick_seen = True
                mode = MODE_CHERRY_PICK
            elif '===== BACKWARD COMPATIBILITY PATCHES FOR UPREVS =====' in line:
                backward_seen = True
                mode = MODE_BACKWARD_COMPATIBILITY
            elif '===== ' in line and ' =====' in line:
                mode = MODE_NONE
        else:
            if mode == MODE_CHERRY_PICK:
                if re.match('^cherry-pick-r[0-9]*-', line):
                    continue
                errors.append(
                    'Cherry pick patch name must starts with cherry-pick-rXXXX, but found %s'
                    % (line))
            if mode == MODE_BACKWARD_COMPATIBILITY:
                if line.startswith('backward-compatibility-'):
                    continue
                errors.append(
                    'Backward compatibility patch must starts with backward-compatibility-, but found %s'
                    % (line))

    if not cherry_pick_seen:
        errors.append(
            'cherry pick section missing, please add line `# ===== CHERRY PICKS =====` with at least 5 =s on each side'
        )

    if not backward_seen:
        errors.append(
            'backward compatibility section missing, please add line `# ===== BACKWARD COMPATIBILITY PATCHES FOR UPREVS =====` with at least 5 =s on each side'
        )

    return errors


def main():
    parser = argparse.ArgumentParser(
        description='Check libchrome patches in good shape.')

    parser.add_argument('--commit',
                        help='Hash of commit to check. Only show errors on ' \
                             'lines added in the commit if set.')
    parser.add_argument('files', nargs='*')

    args = parser.parse_args(sys.argv)

    errors = []
    errors += checkPatchesFileNameConvention(args.commit)

    if errors:
        print('\n'.join(errors), file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()
