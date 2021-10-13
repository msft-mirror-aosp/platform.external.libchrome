#!/usr/bin/env python3
# Copyright 2021 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Script to generate cros/upstream branch.

Usage: update_upstream.py [--delete | --add | --forward | --all]

--delete: Run delete stage to delete unnecessary files. Create one single commit
to delete unnecessary files.
--add: Run add stage to add new files. Create a series of commits containing the
history of new files, and one merge commit to add to cros/upstream.
--forward: Run forward stage to update to newer version. Create a series commits
to update cros/upstream to newer revision.
--all: Run all stages. equals to --delete --add --forward
"""

import argparse
import subprocess
import sys
import tempfile

import filter_config
import filtered_utils
import filters
import lazytree
import utils

GENERATED_FILTERED_TREE = 'libchrome_tools/developer-tools/uprev/generate_filtered_tree.py'
CROS_LIBCHROME_RECOVERED_FROM_COMMIT = b'CrOS-Libchrome-Recovered-From-Commit'
INITIAL_COMMIT = 'ba8bd83211d4bbca7a48793d567b06d5d4451005'


def verify_tree(current):
    """ Verifies current commit has the filtered files matching filter_config.

    Returns True if if's correct, False otherwise.

    Args:
        current: current cros/upstream commit hash.
    """
    current_metadata = filtered_utils.get_metadata(current)
    original_commit = current_metadata.original_commit_cursor
    new_filter = filters.Filter(filter_config.WANT, filter_config.WANT_EXCLUDE,
                                filter_config.ALWAYS_WANT, filter_config.KEEP,
                                filter_config.KEEP_EXCLUDE)
    expected_tree = lazytree.LazyTree()
    for f in utils.get_file_list(original_commit):
        if new_filter.want_file(f.path):
            expected_tree[f.path] = f

    return expected_tree.hash() == current_metadata.tree


def verify_author(current):
    """ Verifies current commit has the same authorship as original commit.

    Returns True if if's correct, False otherwise.

    Args:
        current: current cros/upstream commit hash.
    """
    current_metadata = filtered_utils.get_metadata(current)
    original_commit = current_metadata.original_commit_cursor
    original_metadata = filtered_utils.get_metadata(original_commit)

    return current_metadata.authorship == original_metadata.authorship


def delete(current):
    """ Deletes unnecessary files for new filters.

    Returns a string containing the commit after delete.

    Args:
        current: current cros/upstream commit hash.
    """
    current_metadata = filtered_utils.get_metadata(current)
    original_commit = current_metadata.original_commit_cursor
    new_filter = filters.Filter(filter_config.WANT, filter_config.WANT_EXCLUDE,
                                filter_config.ALWAYS_WANT, filter_config.KEEP,
                                filter_config.KEEP_EXCLUDE)
    tree = lazytree.LazyTree(current_metadata.tree)
    current_files = utils.get_file_list(current)

    for f in current_files:
        if not new_filter.want_file(f.path):
            del tree[f.path]

    if tree.hash() == current_metadata.tree:
        return current

    msg = b'Remove unnecessary files due to filter change\n\n%s: %s' % (
        filtered_utils.CROS_LIBCHROME_CURRENT_COMMIT, original_commit)
    new_commit = subprocess.check_output(
        ['git', 'commit-tree', '-p', current,
         tree.hash()], input=msg).strip()
    return new_commit.decode('ascii')


def add(current):
    """ Adds newly wanted files for new filters.

    Args:
        current: current cros/upstream commit hash.
    """
    current_metadata = filtered_utils.get_metadata(current)
    original_commit = current_metadata.original_commit_cursor
    new_filter = filters.Filter(filter_config.WANT, filter_config.WANT_EXCLUDE,
                                filter_config.ALWAYS_WANT, filter_config.KEEP,
                                filter_config.KEEP_EXCLUDE)

    tree = lazytree.LazyTree()

    original_files = utils.get_file_list(original_commit)
    new_files = set(new_filter.filter_files([], original_files))
    for f in new_files:
        if new_filter.want_file(f.path):
            tree[f.path] = f

    if tree.hash() == current_metadata.tree:
        return current

    old_files = set(utils.get_file_list(current))
    files_to_add = list(new_files.difference(old_files))

    if not files_to_add:
        return current

    proc = subprocess.run([
        GENERATED_FILTERED_TREE,
        INITIAL_COMMIT,
        original_commit,
        '--commit_hash_meta_name',
        CROS_LIBCHROME_RECOVERED_FROM_COMMIT,
        '--filter_files',
        '/dev/stdin',
    ],
                          input=b''.join(f.path + b'\n' for f in files_to_add),
                          stdout=subprocess.PIPE)
    assert proc.returncode == 0
    last_commit = proc.stdout.strip().decode('ascii')

    current_tree = lazytree.LazyTree(current_metadata.tree)
    new_file_history_head_filelist = utils.get_file_list(last_commit)
    for f in new_file_history_head_filelist:
        current_tree[f.path] = f

    assert tree.hash() == current_tree.hash(), (tree.hash(),
                                                current_tree.hash())

    msg = b'Add new files due to filter change\n\n%s: %s' % (
        filtered_utils.CROS_LIBCHROME_CURRENT_COMMIT, original_commit)
    new_commit = subprocess.check_output(
        ['git', 'commit-tree', '-p', current, '-p', last_commit,
         tree.hash()],
        input=msg).strip()
    return new_commit.decode('ascii')


def forward(current, target):
    """ Forwards to given upstream Chromium commit.

    Returns a string containing the commit after forward.

    Args:
        current: current cros/upstream commit hash.
        target: target commit in Chromium src tree.
    """
    proc = subprocess.run([
        GENERATED_FILTERED_TREE,
        current,
        target,
    ],
                          stdout=subprocess.PIPE)
    assert proc.returncode == 0
    return proc.stdout.strip().decode('ascii')


def main():
    # Init args
    parser = argparse.ArgumentParser(
        description='Generate Libchrome Upstream Branch')
    parser.add_argument(
        'current',
        metavar='current',
        type=str,
        help='commit hash to start from, usually use cros/upstream.')
    parser.add_argument(
        'target',
        metavar='target',
        type=str,
        help='commit hash in browser tot. only useful if --forward is enabled.')

    parser.add_argument('--all',
                        dest='all',
                        action='store_const',
                        const=True,
                        default=False,
                        help='Run all stages.')
    parser.add_argument('--delete',
                        dest='delete',
                        action='store_const',
                        const=True,
                        default=False,
                        help='Run delete files stage.')
    parser.add_argument('--add',
                        dest='add',
                        action='store_const',
                        const=True,
                        default=False,
                        help='Run add files stage.')
    parser.add_argument('--forward',
                        dest='forward',
                        action='store_const',
                        const=True,
                        default=False,
                        help='Run forward to <target> stage.')

    args = parser.parse_args(sys.argv[1:])

    current = args.current
    target = args.target

    assert args.all or args.delete or args.add or args.forward

    if args.all or args.delete:
        current = delete(current)

    if args.all or args.add:
        current = add(current)

    if args.all or args.forward:
        assert verify_tree(
            current), 'Files must be correctly filtered before forward stage'
        pre_forward = current
        current = forward(current, target) or current
        if current != pre_forward:
            assert verify_tree(
                current), 'Files must be correctly filtered after forward stage'
            # Verify the last commit authorship, to ensure forward process
            # correctly passed environment variables to git (especially if via
            # Chromium Infra git wrapper)
            assert verify_author(current), 'Commit must have original author'

    print(current)


if __name__ == '__main__':
    main()
