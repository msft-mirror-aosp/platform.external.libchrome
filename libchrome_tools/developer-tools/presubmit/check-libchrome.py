#!/usr/bin/env python3
# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Utility to check if deprecated libchrome calls are introduced.
"""

from __future__ import print_function

import argparse
import os
import re
import subprocess
import sys

# BAD_KEYWORDS are mapping from bad_keyword_in_regex to
# error_msg_if_match_found.
BAD_KEYWORDS = {
    # removal of deprecated base::Bind APIs
    r'base::(Bind\(|Closure|Callback|CancelableCallback|CancelableClosure)':
    'Deprecated base::Bind APIs. Please use the Once or Repeating variants. See crbug/714018.',
    # removal of WARN_UNUSED_RESULT in r961763
    r'WARN_UNUSED_RESULT':
    'The macro will be removed after r961763, use C++17 attribute [[nodiscard]] instead, see crbug.com/1287045.',
    # removal of return by out-param base::FilePath::GetComponents in r970764
    r'GetComponents\(.+\)':
    'The return by out-param version of base::FilePath::GetComponents Will be deprecated after r970764, get output directly as return value instead.',
    # removal of base::{size,empty,data} in r979799 (crrev.com/c/3511268)
    r'base::(size|empty|data)':
    'The functions will be removed from base/cxx17_backports.h, use the std counterparts instead.',
}

LINE_NUMBER_RE=re.compile(r'^@@ [0-9\,\+\-]+ \+([0-9]+)[ \,][0-9 ]*@@')

def addedLinesFromCommit(filename, commit):
  '''Return list of (line number, line) pairs added to file by the given commit.
  '''
  diff = subprocess.check_output(
      ['git', 'show', '--oneline', commit, '--', filename]).decode(
          sys.stdout.encoding).split('\n')
  line_number = -1
  lines = []
  for line in diff:
    m = LINE_NUMBER_RE.match(line)
    if m:
      line_number = int(m.groups(1)[0])-1
      continue
    if not line.startswith('-'):
      line_number += 1
    if line.startswith('+++') or not line.startswith('+'):
      continue
    lines.append((line_number, line[1:]))

  return lines

def checkFileLines(filename, lines, keywords):
  '''Check for forbidden patterns in given lines of a file.

  Args:
    filename: of the source file.
    lines: a list of (line number, line) pairs to be checked.
    keywords: a list of (regex, description) pairs of forbidden patterns

  Returns:
    A list of error messages reporting forbidden patterns found.
  '''
  errors = []
  for line_number, line in lines:
    for bad_pattern, error_message in keywords.items():
      m = re.search(bad_pattern, line)
      if m:
        errors.append('In File %s line %s col %s, found %s (pattern: %s), %s' %
                      (filename, line_number, m.span()[0]+1, m.group(0),
                       bad_pattern, error_message))
        break

  return errors


def checkFiles(files, commit, keywords=BAD_KEYWORDS):
  '''Check for forbidden patterns from given list of files.

  Args:
    files: a list of filenames.
    commit: hash of a commit. If given, only lines added to this commit will be
    checked; otherwise check all lines.
    keywords: a list of (regex, description) pairs of forbidden patterns

  Returns:
    A list of error messages reporting forbidden patterns found.
  '''
  errors = []

  for filename in files:
    if not (filename.endswith('.h') or filename.endswith('.cc')):
        continue
    if commit:
      lines = addedLinesFromCommit(filename, commit)
    else:
      with open(filename) as f:
        lines = [(i+1, line) for i, line in enumerate(f.readlines())]
    errors += checkFileLines(filename, lines, keywords)

  return errors


def main():
  parser = argparse.ArgumentParser(
      description='Check no forbidden libchrome features are used.')

  parser.add_argument('--commit',
                      help='Hash of commit to check. Only show errors on ' \
                           'lines added in the commit if set.')
  parser.add_argument('files', nargs='*')
  args = parser.parse_args()

  errors = checkFiles(args.files, args.commit)

  if errors:
      print('\n'.join(errors), file=sys.stderr)
      sys.exit(1)


if __name__ == '__main__':
    main()
