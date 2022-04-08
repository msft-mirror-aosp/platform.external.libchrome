#!/usr/bin/env python
# Copyright 2022 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Identify unused libchrome cc files, by
analyzing binary symbol from libchrome .o(s) and image ELF executable/.so(s).

Usage:

1)
  $ export LIBCHROME_BOARD=<some board>
  $ FEATURES=noclean emerge-$LIBCHROME_BOARD libchrome
2)
  ./usagechecker.py --libchrome-reference-board $LIBCHROME_BOARD --test-board
  <board1> --test-board <board2>
"""

import argparse
import collections
import glob
import os
import subprocess
import sys

import elf
import dependency_graph
import crosimage

TMPDIR = 'tmpdir'


def get_libchrome_graph(board):
    """Returns a DependencyGraph for libchrome intermediate .o

    Args:
      board: board to use. requires a board with uncleaned libchrome temporary
        portage directory.
    """
    libchrome_build_dir = glob.glob(
        '/build/%s/tmp/portage/chromeos-base/libchrome-0.0.1-r*/work/build/out/Default'
        % (board))
    assert len(libchrome_build_dir
              ) == 1, 'Please do FEATURES=noclean emerge-$BOARD libchrome'
    libchrome_build_dir = libchrome_build_dir[0]
    objects_dir = os.path.join(libchrome_build_dir, 'obj/libchrome')
    objects = glob.iglob(os.path.join(objects_dir, '**/*.o'), recursive=True)

    object_symbols = {}
    for o in objects:
        # test_support .o(s) only compiles to .a static library. It is not
        # supposed to be used by any production binary from the image. The
        # image doesn't have unittest which uses test_support, either. Besides,
        # the .a doesn't consume any final image size.
        # Thus, ignore any test_support objects.
        if 'test_support.' in o:
            continue
        object_symbols[o[len(objects_dir) + 1:]] = elf.read_symbol(o)

    graph = dependency_graph.DependencyGraph()
    for o, symbols in object_symbols.items():
        provides, wants = symbols
        graph.add_object(o, provides, wants)

    return graph


def get_target_file(path):
    """Gets list of files that is potentially dynamically linked.

    Args:
      path: search path root begins here.
    """
    wanted = []
    for dirpath, _, files in os.walk(path):
        for filename in files:
            # If we found libchrome itself, ignoring.
            if 'libbase-' in filename or 'libmojo.so' == filename:
                continue
            filepath = os.path.join(dirpath, filename)
            # We only want ELF executables or shared libaries. Both should have
            # executable permissions.
            if not os.access(filepath, os.X_OK):
                continue
            deps = subprocess.run(
                ['readelf', '-d', filepath],
                stdout=subprocess.PIPE,
                stderr=subprocess.DEVNULL).stdout.decode('utf-8')
            if 'libbase-' in deps or 'libmojo.so' in deps:
                wanted.append(filepath)
    return wanted


def get_image_graph(board):
    """Returns a DependencyGraph for dynamically linked binaries in board image.

    Args:
      board: board to scan image for.
    """
    graph = dependency_graph.DependencyGraph()
    with crosimage.MountedReleaseImage(board, "testdir") as image:
        # with crosimage.MountedReleaseImage(board, TMPDIR) as image:
        files_to_scan = get_target_file(image.rootfs)
        for f in files_to_scan:
            provides, wants = elf.read_symbol(f, dynamic=True)
            graph.add_object(f[len(image.rootfs) + 1:],
                             provides,
                             wants,
                             ignore_duplicate_symbol_error=True)

    return graph


def get_boards_used_symbols(boards):
    """Gets a set of used symbols for given boards.

    Args:
      boards: list of str, each str represents a board name.
    """
    symbols = set()
    for board in boards:
        graph = get_image_graph(board)
        symbols |= set(graph.symbol_used.keys())

    return symbols


def dive_symbol(symbol, graph, cache):
    """Gets the set of needed objects in graph, recursively, based on given symbol to use.

    Args:
      symbol: a needed symbol.
      graph: a DependencyGraph that one object provide neede symbol.
      cache: a set used in previous calls to dive_symbol with the same graph to
        reduce recursive lookups.
    """
    if symbol not in graph.symbol_provided:
        return set()

    if symbol in cache:
        return cache[symbol]

    # Prevent infinite recursion lookup.
    cache[symbol] = set()

    objects = set()
    provided_by_list = graph.symbol_provided[symbol]

    for provided_by in provided_by_list:
        objects.add(provided_by)

        for want_symbol in graph.wants[provided_by]:
            objects |= dive_symbol(want_symbol, graph, cache)

    cache[symbol] = objects
    return objects


def make_group(files_unused, graph):
    """
    Returns groups where object should be included/excluded atomically.

    With files_unused, usually developers may not want to remove all files at
    once to reduce unexpected errors/regressions, make_group will analyze the
    object dependency in graph to provide all possible groups that files should
    be included/excluded atomically.

    This is not a strongly connected components.
    For example,
     A -> B <-> C
     |
    \|/
     D -> E  <- F
    Objects dependency may look like above.
    With strongly connected components, A / BC / D / E / F are the groups,
    which is not correct. e.g. we cannot remove E-only.
    Instead, groups are:
    A / ABC / F / ADEF / AD / ABCDEF / etc.
    Each group can be removed together and all objects remaining can still have
    their dependency available.

    Args:
      files_unused: list of object to analyze.
      graph: a DependencyGraph containing original objects where files_unused is
        calculated.
    """
    groups = {}
    for file in files_unused:
        groups[file] = frozenset([file] +
                                 graph.get_all_dependencies(file, reverse=True))

    return sorted(list(set(groups.values())), key=len)


def main():
    parser = argparse.ArgumentParser(
        description=
        'Check libchrome and board images to identify unused libchrome objects.'
    )

    parser.add_argument('--libchrome-reference-board',
                        type=str,
                        required=True,
                        help='Board where libchrome objects is located.')
    parser.add_argument(
        '--test-board',
        action='append',
        type=str,
        required=True,
        help=
        'Board name that test image will be used. should be the same architecture as libchrome reference board.'
    )
    parser.add_argument(
        '--print-groups',
        help=
        'Print unused objects in groups based on their inter-dependency. Note: very long output.',
        action='store_const',
        const=True,
        default=False)

    arg = parser.parse_args(sys.argv[1:])
    print(arg)

    graph_libchrome = get_libchrome_graph(arg.libchrome_reference_board)
    wanted_symbols = get_boards_used_symbols(arg.test_board)
    files_unused = graph_libchrome.provides.keys()
    cache = {}
    for used_symbol in wanted_symbols:
        needed_objects = dive_symbol(used_symbol, graph_libchrome, cache)
        files_unused -= needed_objects

    files_unused = sorted(files_unused)
    print('\n'.join(files_unused))
    print()

    if arg.print_groups:
        files_unused_grouped = make_group(files_unused, graph_libchrome)
        for groupid, group in enumerate(files_unused_grouped, start=1):
            group = sorted(group)
            for item in group:
                print('%03d %03d %s' % (groupid, len(group), item))

        print()

    print(len(files_unused))


if __name__ == '__main__':
    main()
