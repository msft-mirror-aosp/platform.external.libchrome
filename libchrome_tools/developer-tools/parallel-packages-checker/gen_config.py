#!/usr/bin/env python3
# Copyright 2021 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import multiprocessing
import os
import re
import subprocess
import sys
import tempfile

TEMPLATE = '''
# THIS FILE IS GENERATED. DO NOT EDIT MANUALLY.


ALL_BOARDS = %s
DEFAULT_BOARDS = %s

# Not used. For reference only.
BOARD_PACKAGES = %s

BOARDS_MAPPING = {
    'all': ALL_BOARDS,
    'default': DEFAULT_BOARDS,
}

# Not used. For reference only.
DEFAULT_BOARDS_REASON = %s
'''

_BUILDER_CONFIG_PATH = '/mnt/host/source/infra/config/generated/builder_configs.cfg'
_VERSION_REMOVE_RE = re.compile('(.*?)(-[0-9.]+)?(-r[0-9]+)?$')


def get_gs_file(board, latest_release, latest_milestone, filename):
    try:
        pattern_to_ls = 'gs://chromeos-image-archive/%s-postsubmit/%s*/%s' % (
            board, latest_release, filename)
        # -1 is empty str after split '\n', -2 is the last item.
        recent_postsubmit = subprocess.check_output(
            ['gsutil', 'ls', pattern_to_ls]).decode('ascii').split('\n')[-2]
    except subprocess.CalledProcessError:
        pattern_to_ls = 'gs://chromeos-image-archive/%s-postsubmit/R%d-*/%s' % (
            board, latest_milestone, filename)
        try:
            recent_postsubmit = subprocess.check_output(
                ['gsutil', 'ls', pattern_to_ls]).decode('ascii').split('\n')[-2]
        except subprocess.CalledProcessError:
            return None
    return recent_postsubmit


def get_pkg_from_db(path):
    dirs = set()
    for dirpath, dirnames, filenames in os.walk(path):
        for filename in filenames:
            if filename == 'DEPEND' or filename == 'RDEPEND':
                filepath = os.path.join(dirpath, filename)
                with open(filepath, 'r') as f:
                    if 'chromeos-base/libchrome' in f.read():
                        dirs.add(dirpath)
    pkg_names_with_ver = [
        # x has format like
        # /tmp/xxxx/var_new/db/pkg/chromeos-base/libbrillo-0.0.1-r11111
        '/'.join(x.split('/')[-2:]) for x in dirs
    ]
    pkg_names = [
        _VERSION_REMOVE_RE.match(x).group(1) for x in pkg_names_with_ver
    ]
    return sorted(pkg_names)


@contextlib.contextmanager
def managed_mounted_fs(image_dir):
    subprocess.check_call(['./mount_image.sh', 'chromiumos_test_image.bin'],
                          cwd=image_dir)
    try:
        yield
    finally:
        subprocess.check_call(
            ['./umount_image.sh', 'chromiumos_test_image.bin'], cwd=image_dir)
        print(image_dir, 'umounted')


def get_board_packages(args):
    idx, board, latest_release, latest_milestone = args
    print('Started', idx, board)
    with tempfile.TemporaryDirectory() as tmpdir:
        packages_with_libchrome_deps = set()
        recent_postsubmit = get_gs_file(board, latest_release, latest_milestone,
                                        'stateful.tgz')
        if recent_postsubmit:
            subprocess.check_call([
                'gsutil', 'cp', recent_postsubmit,
                os.path.join(tmpdir, 'stateful.tgz')
            ],
                                  stderr=subprocess.DEVNULL)
            subprocess.check_call([
                'tar', 'xf',
                os.path.join(tmpdir, 'stateful.tgz'), '-C', tmpdir
            ])

            pkgs = get_pkg_from_db(os.path.join(tmpdir, 'var_new/db/pkg'))
        else:
            image_zip = get_gs_file(board, latest_release, latest_milestone,
                                    'image.zip')
            if not image_zip:
                print('Skipped', idx, board)
                return (board, set())

            print('Use image.zip', idx, board)

            subprocess.check_call(
                ['gsutil', 'cp', image_zip,
                 os.path.join(tmpdir, 'image.zip')],
                stderr=subprocess.DEVNULL)
            subprocess.check_call(
                ['unzip',
                 os.path.join(tmpdir, 'image.zip'), '-d', tmpdir])

            with managed_mounted_fs(image_dir=tmpdir):
                pkgs = get_pkg_from_db(
                    os.path.join(tmpdir, 'dir_1/var_overlay/db/pkg'))

        packages_with_libchrome_deps.update(pkgs)
        print('Done', idx, board, len(packages_with_libchrome_deps), 'packages')
        return (board, packages_with_libchrome_deps)


def main():
    srcdir = os.path.dirname(__file__)

    all_postsubmit = subprocess.check_output([
        'jq',
        '.[][] | select (.id.name == "postsubmit-orchestrator") | .orchestrator.childSpecs[].name',
        '-r',
        _BUILDER_CONFIG_PATH,
    ])
    all_postsubmit = all_postsubmit.split(b'\n')
    all_criticals = subprocess.check_output([
        'jq', '.[][] | select (.general.critical == true) | .id.name', '-r',
        _BUILDER_CONFIG_PATH
    ])
    all_criticals = all_criticals.split(b'\n')
    critical_postsubmit = set(all_postsubmit).intersection(set(all_criticals))
    all_boards = []
    for postsubmit in critical_postsubmit:
        postsubmit = postsubmit.strip()
        if not postsubmit:
            continue
        postsubmit = postsubmit.decode('ascii')
        assert postsubmit.endswith('-postsubmit')
        board = postsubmit[:-len('-postsubmit')]
        # In builder_configs, generic boards has asan builders, which we don't
        # need.
        if re.match('(amd64|arm|arm64)-generic.+', board):
            continue
        # chromite is not a board.
        if board == 'chromite':
            continue
        # kernel_checkconfig-* is not a board.
        if board.startswith('kernel_checkconfig-'):
            continue
        all_boards.append(board)
    all_boards = sorted(all_boards)
    print('%d boards found %s' % (len(all_boards), all_boards))

    default_boards = []
    libchrome_users = set()
    libchrome_users_by_board = {}

    latest_release = subprocess.check_output([
        'gsutil', 'cat',
        'gs://chromeos-image-archive/%s-release/LATEST-main' % (all_boards[10])
    ]).decode('ascii')
    latest_milestone = int(latest_release.split('-')[0][1:])
    print('Latest release %s, R%d' % (latest_release, latest_milestone))

    with multiprocessing.Pool(processes=10) as pool:
        all_board_pkgs = pool.map(
            get_board_packages,
            [(idx, board, latest_release, latest_milestone)
             for idx, board in enumerate(all_boards, start=1)],
            chunksize=1)

    for board, pkgs in all_board_pkgs:
        assert len(pkgs) > 0, 'board %s must have packages' % (board)
        libchrome_users = libchrome_users | pkgs
        libchrome_users_by_board[board] = pkgs

    print('Total of %d packages depending on libchrome' %
          (len(libchrome_users)))

    # Deep copy.
    libchrome_users_by_board_copy = {}
    for board, pkg_by_board in libchrome_users_by_board.items():
        libchrome_users_by_board_copy[board] = sorted(list(pkg_by_board))

    # Use greedy algorithm to find a sub-optimal minimum boards coverage.
    boards_reason = {}
    while libchrome_users:
        max_board, max_board_cnt = None, 0
        for board in libchrome_users_by_board:
            libchrome_users_by_board[board] = libchrome_users_by_board[
                board].intersection(libchrome_users)
            if len(libchrome_users_by_board[board]) > max_board_cnt:
                max_board, max_board_cnt = board, len(
                    libchrome_users_by_board[board])
        assert max_board
        default_boards.append(max_board)
        libchrome_users.difference_update(libchrome_users_by_board[max_board])
        boards_reason[max_board] = sorted(
            list(libchrome_users_by_board[max_board]))
        del libchrome_users_by_board[max_board]
    print('Recommended coverage: %s' % (default_boards))

    with open(os.path.join(srcdir, 'config.py'), 'w') as f:
        f.write(TEMPLATE %
                (repr(all_boards), repr(default_boards),
                 repr(libchrome_users_by_board_copy), repr(boards_reason)))

    subprocess.check_call([
        '/mnt/host/depot_tools/yapf', '-i',
        os.path.join(srcdir, 'config.py'), '--style=google'
    ])


if __name__ == '__main__':
    main()
