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
    # r960000 uprev
    # base::TimeDelta::From*
    r'base::TimeDelta(\(\).|::)From(Days|Hours|Minutes|Seconds|Milliseconds|Microseconds|Nanoseconds)':
    'base::TimeDelta::From* functions will be removed. Use base::<unit> instead, e.g. base::Days, base::Hours.',
    # task related files moved to base/task/
    r'include .base/(bind_post_task|deferred_sequence_task_runner|sequenced_task_runner|sequenced_task_runner_helpers|single_thread_task_runner|task_runner|task_runner_util).h':
    'Task-related files will be moved to base/task/, include base/task/<filename> instead.',
    # removal of base/macro.h
    r'include .base/macro.h':
    'The file will be removed after r941411. Use delete ctor for DISALLOW_* macros and std::ignore for ignore_result.',
    # removal of delete ctor macros
    r'DISALLOW_COPY_AND_ASSIGN':
    'Chromium agreed to return Google C++ style. Use deleted constructor in `public:` manually. See crbug/1010217',
    # removal of ignore_result (now in base/macro.h, to be renamed as
    # base/ignore_result.h)
    r'ignore_result':
    'Will be deprecated after r863041, use std::ignore instead.',
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
