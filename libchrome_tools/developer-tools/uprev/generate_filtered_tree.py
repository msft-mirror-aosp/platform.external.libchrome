#!/usr/bin/env python3
# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import collections
import datetime
import os
import subprocess
import sys
import time

import filter_config
import filtered_utils
import filters
import lazytree
import utils

# Indicating information for look forward.
# look_forward_until_index indicating a 1-based idx in pending_commits that
# there's no changes we care about until look_forward_until_index.
# parent_commit_hash indicating the parent previously made commit we cared
# about.
LookForwardInformation = collections.namedtuple(
    'LookForwardInformation',
    ['look_forward_until_index', 'parent_commit_hash'])

# Use avg speed of last TIMING_DISTANCE commits.
_TIMING_DISTANCE = 100
# Verify the tree is consistent (diff-based, and actual) when a commit is made
# after every _VERIFY_INTEGRITY_DISTANCE in browser repository.
# Merge commits are always verified.
_VERIFY_INTEGRITY_DISTANCE = 1000


def timing(timing_deque, update=True):
    """Returns a speed (c/s), and updates timing_deque.

    Args:
        timing_deque: a deque to store the timing of past _TIMING_DISTANCE.
        update: adds current timestamp to timing_deque if True. It needs to set
            to False, if it wants to be called multiple times with the current
            timestamp.
    """
    first = timing_deque[0]
    now = time.time()
    if update:
        timing_deque.append(now)
        if len(timing_deque) > _TIMING_DISTANCE:
            timing_deque.popleft()
    return _TIMING_DISTANCE / (now - first)


def get_start_commit_of_browser_tree(parent_filtered):
    """Returns the last commit committed in bytes by the script, and its metadata.

    Args:
        parent_filtered: the commit hash of the tip of the filtered branch.
    """
    assert isinstance(parent_filtered, bytes)
    current = parent_filtered
    while True:
        meta = filtered_utils.get_metadata(current)
        if meta.original_commits:
            return current, meta
        if not meta.parents:
            return None, None
        # Follow main line only
        current = meta.parents[0]


def find_filtered_commit(commit, commits_map):
    """Finds the corresponding parent of a browser commit in filtered branch.

    If not found, the corresponding commit of its least ancestor is used.

    Args:
        commit: commit hash in browser repository.
        commits_map: commit hash mapping from original commit to the one in the
            filtered branch. commits_map may be altered.
    """
    look_for = commit
    while look_for not in commits_map:
        meta = filtered_utils.get_metadata(look_for)
        assert len(meta.parents) <= 1
        if len(meta.parents) == 1:
            look_for = meta.parents[0]
        else:
            look_for = 'ROOT'
    commits_map[commit] = commits_map[look_for]
    return commits_map[look_for]


def do_commit(treehash, commithash, meta, commits_map, commit_hash_meta_name):
    """Makes a commit with the given arguments.

    This creates a commit on the filtered branch with preserving the original
    commiter name, email, authored timestamp and the message.
    Also, the special annotation `CrOS-Libchrome-Original-Commit:
    <original-commit-hash>' is appended at the end of commit message.
    The parent commits are identified by the parents of the original commit and
    commits_map.

    Args:
        treehash: tree object id for this commit.
        commithash: original commit hash, used to append to commit message.
        meta: meta data of the original commit.
        commits_map: current known commit mapping. commits_map may be altered.
    """
    parents_parameters = []
    for parent in meta.parents:
        parents_parameters.append('-p')
        parents_parameters.append(find_filtered_commit(parent, commits_map))
    msg = (meta.message + b'\n\n' + commit_hash_meta_name + b': ' + commithash +
           b'\n')
    return subprocess.check_output(
        ['git', 'commit-tree'] + parents_parameters + [treehash],
        env=dict(os.environ,
                 GIT_AUTHOR_NAME=meta.authorship.name,
                 GIT_AUTHOR_EMAIL=meta.authorship.email,
                 GIT_AUTHOR_DATE=b' '.join(
                     [meta.authorship.time, meta.authorship.timezone])),
        input=msg).strip(b'\n')


def verify_commit(libchrome_filter, original_commit, new_tree):
    """Verifies if new_tree is exactly original_commit after filters.

    Args:
        original_commit: commit hash in Chromium browser tree.
        new_tree: tree hash created for upstream branch commit.
    """
    expected_file_list = libchrome_filter.filter_files(
        utils.get_file_list(original_commit))
    assert utils.git_mktree(expected_file_list) == new_tree


def check_look_forward(pending_commits, base_commit, base_commit_meta,
                       look_forward_from, look_forward_cnt,
                       ignore_look_forward_until, commits_map,
                       libchrome_filter):
    """
    Checks if we can look forward a few commits.

    Returns (look_forward_until, ignore_look_forward_until) indicating we can
    either look forward until look_forward_until[0], or skip checking again until
    ignore_look_forward_until.

    look_forward_until is LookForwardInformation or None.

    ignore_look_forward_until is a number, which indicate we don't need to call
    check_look_forward until ignore_look_forward_until.

    Args:
        pending_commits: list of GitCommitInRevList to process, in topological
            order.
        base_commit: an element in pending_commits.
        base_commit_meta: a GitCommitMetadata for base_commit.
        look_forward_from: the first commit after base_commit to check.
        look_forward_cnt: total count of commits to scan for look-forward.
        ignore_look_forward_until: do not look forward until look_forward_from
            reaching ignore_look_forward_until.
        commits_map: commits_map: current known commit mapping. may be altered.
        libchrome_filter: the filter to use after diff commits.
    """
    # Not to look forward if feature disabled.
    if not look_forward_cnt:
        return None, 0
    # Not to look forward until ignore_look_forward_until.
    if look_forward_from < ignore_look_forward_until:
        return None, ignore_look_forward_until
    # Not to look forward at last few commits.
    if look_forward_from + look_forward_cnt >= len(pending_commits):
        return None, ignore_look_forward_until

    look_forward_until = LookForwardInformation(look_forward_from - 1, None)
    ignore_look_forward_until = look_forward_from - 1
    merge_commit_count = sum(
        len(commit.parent_hashes) != 1
        for commit in pending_commits[look_forward_from:look_forward_from +
                                      look_forward_cnt])
    future_diff = libchrome_filter.filter_diff(
        utils.git_difftree(
            pending_commits[look_forward_from + look_forward_cnt -
                            1].commit_hash, base_commit.commit_hash))
    if len(future_diff) == 0 and merge_commit_count == 0:
        look_forward_until = LookForwardInformation(
            look_forward_from + look_forward_cnt,
            commits_map[base_commit_meta.parents[0]])
    else:
        # It has changes, do not check again at next iteration.
        ignore_look_forward_until = look_forward_from + look_forward_cnt
    return look_forward_until, ignore_look_forward_until


def process_commits(libchrome_filter, pending_commits, commits_map,
                    look_forward, commit_hash_meta_name, progress_callback,
                    commit_callback):
    """Processes new commits in browser repository.

    Returns the commit hash of the last commit made.

    Args:
        libchrome_filter: the filter to use after diff commits.
        pending_commits: list of GitCommitInRevList to process, in topological
            order.
        commits_map: current known commit mapping. may be altered.
            progress_callback: callback for every commit in pending_commits. It
            should take (idx, total, orig_commit_hash, meta) as parameters.
        look_forward: look at next look_forward commits and skip all of them if
            next look_forward combined doesn't have any diff (submit + revert
            will be ignored), and has no merge commit.
        progress_callback: callback when a commit in Chromium upstream has been
            read. It should take (idx, tot, original_commit_hash, meta) as
            parameters.
        commit_callback: callback when a commit is made to filtered branch. It
            should take (orig_commit_hash, new_commit_hash, meta) as parameters.
    """
    last_commit = None
    last_verified = -1
    look_forward_until = None
    ignore_look_forward_until = 0
    for i, commit in enumerate(pending_commits, start=1):
        # [i, look_forward_until.look_forward_until_index] has no changes. Skip
        # CLs are ignored if it's submitted and reverted. Since it brings no
        # change, and a Reland can bring back the correct history (and usually
        # contains the correct commit message).
        if look_forward_until and i < look_forward_until.look_forward_until_index:
            commits_map[
                commit.commit_hash] = look_forward_until.parent_commit_hash
            if progress_callback:
                progress_callback(i, len(pending_commits), commit.commit_hash,
                                  None)
            continue

        meta = filtered_utils.get_metadata(commit.commit_hash)
        if progress_callback:
            progress_callback(i, len(pending_commits), commit.commit_hash, meta)

        # Read diff and parent tree.
        diff_with_parent = libchrome_filter.filter_diff(
            utils.git_difftree(meta.parents[0] if meta.parents else None,
                               commit.commit_hash))
        git_lazytree = lazytree.LazyTree(
            filtered_utils.
            get_metadata(find_filtered_commit(meta.parents[0], commits_map)
                        ).tree if meta.parents else None)

        if len(meta.parents) <= 1 and len(diff_with_parent) == 0:
            # not merge commit    AND no diff
            if len(meta.parents) == 1 and meta.parents[0] in commits_map:
                commits_map[commit.commit_hash] = commits_map[meta.parents[0]]
                # Check if [i, i+look_forward] has no changes.
                look_forward_until, ignore_look_forward_until = check_look_forward(
                    pending_commits, commit, meta, i, look_forward,
                    ignore_look_forward_until, commits_map, libchrome_filter)
            continue

        # Apply diff and commit.
        for op, f in diff_with_parent:
            if op == utils.DiffOperations.ADD or op == utils.DiffOperations.REP:
                git_lazytree[f.path] = f
            elif op == utils.DiffOperations.DEL:
                del git_lazytree[f.path]
        treehash_after_diff_applied = git_lazytree.hash()
        filtered_commit = do_commit(treehash_after_diff_applied,
                                    commit.commit_hash, meta, commits_map,
                                    commit_hash_meta_name)
        if commit_callback:
            commit_callback(commit.commit_hash, filtered_commit, meta)
        commits_map[commit.commit_hash] = filtered_commit
        last_commit = filtered_commit
        if len(meta.parents) > 1 or (i - last_verified >=
                                     _VERIFY_INTEGRITY_DISTANCE):
            # merge commit    OR  every _VERIFY_INTEGRITY_DISTANCE
            last_verified = i
            verify_commit(libchrome_filter, commit.commit_hash,
                          treehash_after_diff_applied)
    # Verify last commit
    if last_commit:
        verify_commit(libchrome_filter, pending_commits[-1].commit_hash,
                      filtered_utils.get_metadata(last_commit).tree)
    return last_commit


def main():
    # Init args
    parser = argparse.ArgumentParser(description='Copy file from given commits')
    parser.add_argument(
        'parent_filtered',
        metavar='parent_filtered',
        type=str,
        nargs=1,
        help=
        'commit hash in filtered branch to continue from. usually HEAD of that branch.'
    )
    parser.add_argument('goal_browser',
                        metavar='goal_browser',
                        type=str,
                        nargs=1,
                        help='commit hash in browser master branch.')
    parser.add_argument(
        '--filter_files',
        metavar='filter_files',
        type=str,
        help=
        'Path a file which should contain file paths to be checked in each line. This overwrites the default file filtering rules, if given.',
        nargs='?')
    parser.add_argument(
        '--commit_hash_meta_name',
        metavar='commit_hash_meta_name',
        type=str,
        default=filtered_utils.CROS_LIBCHROME_ORIGINAL_COMMIT.decode('utf-8'),
        help=
        'Machine-and-people redable metadata key for original commit hash. This overwrites the default value, if given.',
        nargs='?')

    parser.add_argument('--dry_run',
                        dest='dry_run',
                        action='store_const',
                        const=True,
                        default=False)
    parser.add_argument('--verbose',
                        dest='verbose',
                        action='store_const',
                        const=True,
                        default=False)
    parser.add_argument('--quiet',
                        dest='quiet',
                        action='store_const',
                        const=True,
                        default=False)

    arg = parser.parse_args(sys.argv[1:])

    if arg.quiet:
        INFO = VERBOSE = open(os.devnull, 'w')
    elif arg.verbose:
        INFO = sys.stderr
        VERBOSE = sys.stderr
    else:
        INFO = sys.stderr
        VERBOSE = open(os.devnull, 'w')

    # Init filters
    if arg.filter_files:
        with open(arg.filter_files) as f:
            lines = [line.strip().encode('utf-8') for line in f]
        libchrome_filter = filters.Filter([filters.PathFilter(lines)], [], [])
        print('Filter loaded', file=INFO)
    else:
        libchrome_filter = filters.Filter(filter_config.WANT,
                                          filter_config.WANT_EXCLUDE,
                                          filter_config.ALWAYS_WANT)

    # Look for last known commit made by the script in filtered branch.
    print('Looking for last known commit from',
          arg.parent_filtered[0],
          file=INFO)
    last_known, meta_last_known = get_start_commit_of_browser_tree(
        arg.parent_filtered[0].encode('utf-8'))
    if last_known:
        print('Continuing from', last_known, meta_last_known, file=INFO)
    else:
        print('No known last commit', file=INFO)
    print('parent on filter branch', arg.parent_filtered[0], file=INFO)

    # Get a mapping between browser repository and filtered branch for commits
    # in filtered branch.
    print('reading commits details for commits mapping', file=INFO)
    timing_deque = collections.deque([time.time()])
    commits_map = filtered_utils.get_commits_map(
        arg.parent_filtered[0], lambda cur_idx, tot_cnt, cur_hash:
        (print('Reading',
               cur_hash,
               '%d/%d' % (cur_idx, tot_cnt),
               '%f c/s' % timing(timing_deque),
               end='\r',
               file=VERBOSE,
               flush=True),))
    if not 'ROOT' in commits_map:
        commits_map['ROOT'] = subprocess.check_output(
            [
                'git', 'commit-tree', '-p', arg.parent_filtered[0],
                utils.git_mktree([])
            ],
            input=filtered_utils.CROS_LIBCHROME_INITIAL_COMMIT).strip(b'\n')
    print(file=VERBOSE)
    print('loaded commit mapping of', len(commits_map), 'commit', file=INFO)

    # If last_known is not parent_filtered, which means some other commits are
    # made after last_known, use parent_filtered as HEAD to connect.
    if last_known and last_known != arg.parent_filtered[0].encode('utf-8'):
        assert meta_last_known.original_commits[0] in commits_map
        commits_map[meta_last_known.original_commits[0]] = arg.parent_filtered[
            0].encode('ascii')

    # Process newer commits in browser repository from
    # last_known.original_commits
    print('search for commits to filter', file=INFO)
    timing_deque = collections.deque([time.time()])
    pending_commits = utils.git_revlist(
        meta_last_known.original_commits[0] if meta_last_known else None,
        arg.goal_browser[0])
    print(len(pending_commits), 'commits to process', file=INFO)
    new_head = process_commits(
        libchrome_filter,
        pending_commits,
        commits_map,
        # Use look_forward=1000 if filter_files is set.
        # This is used to generate a small sets of newly wanted files.
        1000 if arg.filter_files else None,
        arg.commit_hash_meta_name.encode('utf-8'),
        # Print progress
        lambda cur_idx, tot_cnt, cur_hash, cur_meta:
        (print('Processing',
               cur_hash,
               '%d/%d' % (cur_idx, tot_cnt),
               '%f c/s' % timing(timing_deque, update=False),
               'eta %s' % (datetime.timedelta(seconds=int(
                   (tot_cnt - cur_idx) / timing(timing_deque)))),
               cur_meta.title[:50] if cur_meta else '',
               end='\r',
               file=VERBOSE,
               flush=True),),
        # Print new commits
        lambda orig_hash, new_hash, commit_meta: print(
            b'%s is commited as %s: %s' %
            (orig_hash, new_hash, commit_meta.title[:50]),
            file=INFO))
    print(file=VERBOSE)
    if new_head:
        print(new_head.decode('ascii'))


if __name__ == '__main__':
    main()
