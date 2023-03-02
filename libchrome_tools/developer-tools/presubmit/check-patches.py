#!/usr/bin/env python3
# Copyright 2020 The ChromiumOS Authors
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

ORIGINAL_COMMIT_RE = re.compile(r'"CrOS-Libchrome-Original-Commit: (\w+)\n"')

PATCH_NAME_RE = re.compile(r'(long-term|cherry-pick|backward-compatibility|forward-compatibility)-[0-9]{4}-(.+)\.\w+')

def isUpstreamCommit(commit):
    '''Check if this is a commit from cros/upstream.
    '''
    output = subprocess.check_output(
        ['git', 'log', commit, '-n1',
         '--pretty="%(trailers:key=CrOS-Libchrome-Original-Commit)"'],
        universal_newlines=True,
    ).strip()

    # libchrome non-upstream commit will return non-empty string '""' as output.
    m = ORIGINAL_COMMIT_RE.match(output)

    if m:
      return True
    return False


def getPatchesForChecking(commit = None):
    '''Return list of patches for checking.

    Args:
      commit: hash of a commit. If given, only patches added to this commit will
      be checked

    Returns:
      A list of patches in their basename.
    '''
    patches = []
    if commit:
      output = subprocess.check_output(
          ['git', 'diff', '%s^' % (commit), commit, '--name-status'],
      ).decode('utf-8').splitlines()
      for line in output:
        # The file is "A"dded or "R"enamed.
        if line[0] == 'A' or line[0] == 'R':
          filename = line.split()[-1]
          if os.path.dirname(filename) == 'libchrome_tools/patches':
            patches.append(os.path.basename(filename))
    else:
      patches = [f for f in os.listdir('libchrome_tools/patches')]

    return [f for f in patches if (f != 'patches' and f!= 'patches.config')]


def checkPatchesFileNameConvention(patches):
    '''Check if patches follow the naming conventions.

    Args:
      patches: list of basename of patches to be checked.

    Returns:
      A list of error messages reporting bad patches settings.
    '''
    errors = []
    for patch in patches:
      print('checking "%s"'% patch)
      m = PATCH_NAME_RE.match(patch)
      if not m:
        errors.append(
            'Patch must in format type-dddd-name.ext, where '
            'type is type of patch (one of long-term, cherry-pick, '
            'backward compatibility, or forward compatibility), '
            'and dddd is a four-digit patch number. Found %s.' % (patch))
        continue

      patch_type = m.group(1)
      name = m.group(2)
      if patch_type == 'cherry-pick':
        if not re.match('^r[0-9]+-', name):
          errors.append(
              'Cherry pick patch name must include revision number of the '
              'cherry-picked change, i.e. start with cherry-pick-dddd-rXXXX, '
              'Found %s.' % (patch))

    return errors


def main():
    parser = argparse.ArgumentParser(
        description='Check libchrome patches in good shape.')

    parser.add_argument('--commit',
                        help='Hash of commit to check. Only show errors on ' \
                             'lines added in the commit if set.')
    parser.add_argument('files', nargs='*')

    args = parser.parse_args(sys.argv)

    if args.commit and isUpstreamCommit(args.commit):
      sys.exit(0)

    patches = getPatchesForChecking(args.commit)

    errors = checkPatchesFileNameConvention(patches)

    if errors:
        print('\n'.join(errors), file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()
