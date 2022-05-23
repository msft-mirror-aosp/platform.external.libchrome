#!/usr/bin/env python3
# Copyright 2022 The Chromium OS Authors. All rights reserved.
# # Use of this source code is governed by a BSD-style license that can be
# # found in the LICENSE file.
"""
This script generates a csv row for go/libchrome-uprev-metrics-edit of data for
the day it was run.

Note android (internal and aosp) commits are not counted.
"""

from datetime import date, datetime
from pathlib import Path
import base64
import sys
import re
import requests

# Path to the libchrome dir.
LIBCHROME_DIR = Path(__file__).resolve().parent.parent.parent
# Find chromite relative to $CHROMEOS_CHECKOUT/src/aosp/external/libchrome/, so
# go up four dirs. For importing following the chromite libraries.
sys.path.insert(0, str(LIBCHROME_DIR.parent.parent.parent.parent))

# pylint: disable=wrong-import-position
from chromite.lib import gerrit
from chromite.lib import gob_util

chromium_helper = gerrit.GetGerritHelper(gob='chromium', print_cmd=False)
chrome_helper = gerrit.GetGerritHelper(gob='chrome-internal', print_cmd=False)

# String for current uprev's bug, used to identify CLs associated to it.
current_bug = 'b:228144902'
# String for previous uprev's bug, used to identify CLs associated to it.
previous_bug = 'b:211560276'
# String for individual post-uprev cleanup's bug(s), used to identify CLs
# associated to them.
previous_bug_cleanup = ['b:231676453', 'b:231676446']
# String for date of previous uprev's submission date, used to identify
# post-uprev CLs.
previous_uprev_date = '2022-05-12'
# For formatting output string used in go/libchrome-uprev-metrics-edit
# indicates whether current uprev data should be put in "Uprev A CLs" coloumn
# (will put in 'Uprev B CLS' if set to false).
# The value alternates whenever there is a new uprev.
current_is_uprev_A = True


def datetime_to_string(day: datetime) -> str:
  """ Format a daytime object into a string in format, e.g. '2022-05-23'. """
  return day.strftime('%Y-%m-%d')


def print_info(today):
  """ Print basic data e.g. date, bug numbers, for this run. Remind user to
  update constants.
  """
  YELLOW='\033[33m'
  RESET='\033[39m'
  print(f"This script generates data on " \
        f"{YELLOW}{datetime_to_string(today)}{RESET} with respect to" \
        f"on-going uprev with bug {YELLOW}{current_bug}{RESET};")
  print(f"and previous uprev {YELLOW}{previous_bug}{RESET} submitted on " \
        f"{YELLOW}{previous_uprev_date}{RESET}.")
  print("WARNING: change the constants at the beginning of script if the " \
        "information is out-of-date.")
  print("Copy and paste the last data line (following header line) with 'CSV " \
        "as columns' option in go/libchrome-uprev-metrics-edit.\n")


def getChromiumToTRevision() -> str:
  """ Return chromium ToT revision number by parsing Chromium HEAD commit's
  commit message.
  """
  chromium_ToT = requests.get(
      'https://chromium.googlesource.com/chromium/src/+/refs/heads/master')
  if not chromium_ToT.text:
    raise Exception('Cannot get latest chromium commit')

  # Make sure there is \n before 'Cr-Commit-Position'.
  # This is to handle the case that the HEAD commit is a revert/reland of some
  # earlier commits, in which case the quoted commit message would contain its
  # revision number too.
  CHROMIUM_COMMIT_REVISION_NUMBER_RE = re.compile(
      '\nCr-Commit-Position: refs\/heads\/\w+@\{#([0-9]+)\}')
  m = CHROMIUM_COMMIT_REVISION_NUMBER_RE.search(chromium_ToT.text)
  if not m:
    raise Exception(
        f'Cannot find revision number from chromium HEAD commit, returned ' \
        f'text from request: {chromium_ToT.text}')
  return m[1]


def getLibchromeRevision() -> str:
  """ Return revision number of CrOS libchrome by parsing libchrome/BASE_VER
  file at HEAD.
  """
  libchrome_BASEVER = requests.get(
      "https://chromium.googlesource.com/aosp/platform/external/libchrome/+/refs/heads/master/BASE_VER/?format=TEXT")
  if not libchrome_BASEVER.text:
    raise Exception('Cannot get CrOS ToT libchrome BASE_VER file')
  # Decode and remove trailing newline character.
  return (base64.b64decode(libchrome_BASEVER.text)
          .decode(sys.stdout.encoding)[:-1])


def getLibchromeOriginDate(revision) -> datetime:
  """ Return the date of submssion (to chromium upstream) of a specified
  chromium commit, by revision number, typically the latest one libchrome
  upreved to.
  """
  libchrome_commit = requests.get(f"http://crrev.com/{revision}")
  if not libchrome_commit.text:
    raise Exception(f'Cannot get chromium CL r{revision}')

  # Submission date time comes after committer information
  CHROMIUM_COMMIT_DATE_RE = re.compile(
      'committer<\/th><td>.*<\/td><td>(.+)<\/td><\/tr><tr><th class="Metadata-title">tree')
  m = CHROMIUM_COMMIT_DATE_RE.search(libchrome_commit.text)
  if not m:
    raise Exception(
        f'Cannot find submission date from chromium commit r{revision}, ' \
        f'returned text from request: {libchrome_commit.text}')

  # Matched date will be in format like 'Mon Jan 17 04:51:40 2022'
  return datetime.strptime(m[1], '%a %b %d %H:%M:%S %Y')


def count_commits_for_bug(bug, param_dict={}) -> int:
  """
  Return total number of commits associated to specified bug were submitted,
  including both chromium and chrome internal.

  Args:
    bug: str that will appear on BUG field in commit message to be searched.
    param_dict: dictionary that contains key-value pair to be used as extra
    filters, .e.g. a 'after': <date> means only commits submitted after <date>.
  """
  param_dict['status'] = 'merged'
  bug = 'BUG=.*'+bug
  chromium_cnt = gob_util.QueryChanges(chromium_helper.host, param_dict, first_param=bug)
  chrome_cnt = gob_util.QueryChanges(chrome_helper.host, param_dict, first_param=bug)
  return len(chromium_cnt) + len(chrome_cnt)


def main() -> None:
  today = datetime.now()

  print_info(today)

  chromium_ver = getChromiumToTRevision()
  print(f"Chromium version: {chromium_ver}")

  cros_libchrome_ver = getLibchromeRevision()
  print(f"CrOS libchrome version: {cros_libchrome_ver}")

  chromium_date = today # Chromium HEAD commit should be submitted today
  print(f"Chromium date: {datetime_to_string(chromium_date)}")

  cros_libchrome_date = getLibchromeOriginDate(cros_libchrome_ver)
  print(f"CrOS libchrome date: {datetime_to_string(cros_libchrome_date)}")

  cros_lag_vers = int(chromium_ver) - int(cros_libchrome_ver)
  print(f"Chrome OS libchrome lag version count: {cros_lag_vers}")

  cros_lag_days = (today - cros_libchrome_date).days
  print(f"Chrome OS libchrome lag version count: {str(cros_lag_days)}")

  current_uprev_cl_cnt = str(count_commits_for_bug(current_bug))
  print(f"CLs count for current uprev: {current_uprev_cl_cnt }")

  if previous_bug:
    previous_uprev_cl_cnt = str(count_commits_for_bug(previous_bug))
    print(f"CLs count for previous uprev: {previous_uprev_cl_cnt}")
    previous_uprev_cl_cnt_post_uprev = count_commits_for_bug(previous_bug, {'after': previous_uprev_date})
    for bug in previous_bug_cleanup:
      previous_uprev_cl_cnt_post_uprev += count_commits_for_bug(bug)
    previous_uprev_cl_cnt_post_uprev = str(previous_uprev_cl_cnt_post_uprev)
    print(f"Post-uprev CLs count for previous uprev: {previous_uprev_cl_cnt_post_uprev}")
  else:
    previous_uprev_cl_cnt = ""
    previous_uprev_cl_cnt_post_uprev = ""
    print("Previous uprev is completed; no need to update data")

  print("\nDate, Chromium version, CrOS libchrome version, Chromium date, " \
        "CrOS date, CrOS lag vers, CrOS lag days, Uprev CLs, Post uprev CLs, "\
        "Uprev A CLs, Uprev B CLs")
  print('%s, %s, %s, %s, %s, %d, %d, %s, %s, %s, %s' %
        (datetime_to_string(today), chromium_ver, cros_libchrome_ver,
         datetime_to_string(chromium_date),
         datetime_to_string(cros_libchrome_date), cros_lag_vers,
         cros_lag_days, current_uprev_cl_cnt, previous_uprev_cl_cnt_post_uprev,
         (current_uprev_cl_cnt if current_is_uprev_A else previous_uprev_cl_cnt),
         (previous_uprev_cl_cnt if current_is_uprev_A else current_uprev_cl_cnt)))


if __name__ == '__main__':
  main()
