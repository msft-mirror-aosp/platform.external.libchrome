#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Helper for adding, removing, or replacing an include to/from source file(s).

This is adapted from Chromium's tools/add_header.py with tweaks, such as the
logic that classifies header according to ChromeOS convention (e.g. libchrome
headers are often decorated by <> instead of "").

This code does NOT aggressively re-sort includes. It tries to add/ remove header
with minimal change to the original code.

It inserts includes using some heuristics and might not be always accurate.
Review before submission.

Usage:
change_header.py --add "<utility>" foo/bar.cc foo/baz.cc foo/baz.h
    Add <utility> to the three files

change_header.py --remove "<base/optional.h>" foo/bar.cc foo/baz.cc foo/baz.h
change_header.py --remove '"base/optional.h"' foo/bar.cc foo/baz.cc foo/baz.h
change_header.py --remove 'base/optional.h' foo/bar.cc foo/baz.cc foo/baz.h
    All three remove any include of base/optional.h, if exists with matching or
    non-matching decorator, from the three files

change_header.py --replace 'base/optional.h' '<optional>' foo/bar.cc foo/baz.cc foo/baz.h
    Replace base/optional.h (regardless of decorator) by <optional>, from the
    three files
change_header.py --replace 'base/bind_post_task.h' 'base/task/bind_post_task.h'
              --same_decorator foo/bar.cc foo/baz.cc foo/baz.h
    Replace base/bind_post_task.h (regardless of decorator) by
    base/task/bind_post_task.h with same decorator as removed header, from the
    three files
change_header.py --replace 'base/bind_post_task.h' 'base/task/bind_post_task.h'
              --same_decorator --sort_only_prefix base/ foo/bar.cc
    Replace base/bind_post_task.h (regardless of decorator) by
    base/task/bind_post_task.h with same decorator as removed header, from the
    file. Only sort the headers immediately next to the replaced header that
    start with base/. Only applicable when the old and new headers both start
    with this prefix.
"""

import argparse
import os.path
import re
import sys
import logging

# The specific values of these constants are also used as a sort key for
# ordering different header types in the correct relative order.
HEADER_TYPE_PRIMARY = 0
HEADER_TYPE_C_SYSTEM = 1
HEADER_TYPE_CXX_SYSTEM = 2
HEADER_TYPE_CROS = 3
HEADER_TYPE_USER = 4
BODY = 5

HEADER_TYPE_INVALID = -1

def IsDecorated(name):
    """Returns whether the name is decorated with "" or <>."""
    return IsSystemHeader(name) or IsUserHeader(name)


def ClassifyHeader(decorated_name):
    assert IsDecorated(decorated_name), \
        f'{decorated_name} is not decorated properly'
    if IsCrOSHeader(decorated_name):
        return HEADER_TYPE_CROS
    elif IsCSystemHeader(decorated_name):
        return HEADER_TYPE_C_SYSTEM
    elif IsCXXSystemHeader(decorated_name):
        return HEADER_TYPE_CXX_SYSTEM
    elif IsUserHeader(decorated_name):
        return HEADER_TYPE_USER
    else:
        return HEADER_TYPE_INVALID


def UndecorateName(name):
    """Remove "" or <> if name is decorated, or return as is."""
    return name[1:-1] if IsDecorated(name) else name


def CopyDecoratorTo(name, decorated_name_src):
    """(Re)decorate name with same decorator as decorator source."""
    name = UndecorateName(name) if IsDecorated(name) else name
    assert IsDecorated(decorated_name_src), \
        f'Decorator source {from_decorated_name} is not decorated properly'
    return decorated_name_src[0] + UndecorateName(
        name) + decorated_name_src[-1]


def IsCrOSHeader(decorated_name):
    """
    Returns true if decorated_name looks like a CrOS header, e.g. libchrome or
    libbrillo.
    Depends only on top directory and decorator does not matter.
    """
    return (UndecorateName(decorated_name).split('/')[0] in [
        'base', 'ipc', 'mojo', 'dbus', 'gtest', 'brillo'
    ] and UndecorateName(decorated_name).endswith('.h'))


def IsSystemHeader(decorated_name):
    """Returns true if decorated_name looks like a system header."""
    return decorated_name[0] == '<' and decorated_name[-1] == '>'


def IsCSystemHeader(decorated_name):
    """Returns true if decoraed_name looks like a C system header."""
    return IsSystemHeader(decorated_name) and UndecorateName(
        decorated_name).endswith('.h')


def IsCXXSystemHeader(decorated_name):
    """Returns true if decoraed_name looks like a C++ system header."""
    return (IsSystemHeader(decorated_name)
            and not UndecorateName(decorated_name).endswith('.h'))


def IsUserHeader(decorated_name):
    """Returns true if decoraed_name looks like a user header."""
    return decorated_name[0] == '"' and decorated_name[-1] == '"'


# Regular expression for matching types of lines
EMPTY_LINE_RE = re.compile(r'\s*$')
COMMENT_RE = re.compile(r'\s*(//|/\*)')
COMMENT_END_RE = re.compile(r'.*\*/$')
INCLUDE_RE = re.compile(r'\s*#(import|include)\s+([<"].+?[">])(\s*(//.*))?$')
MACRO_RE = re.compile(r'\s*#(.*)$')
EXTERN_C_RE = re.compile(r'\s*extern "C"(.*){$')
EXTERN_C_END_RE = re.compile(r'\s*}(\s*//.*)?$')


def _DecomposePath(filename):
    """Decomposes a filename into a list of directories and the basename.

    Args:
      filename: A filename!

    Returns:
      A tuple of a list of directories and a string basename.
    """
    dirs = []
    dirname, basename = os.path.split(filename)
    while dirname:
        dirname, last = os.path.split(dirname)
        dirs.append(last)
    dirs.reverse()
    # Remove the extension from the basename.
    basename = os.path.splitext(basename)[0]
    return dirs, basename


_PLATFORM_SUFFIX = (
        r'(?:_(?:android|aura|chromeos|ios|linux|mac|ozone|posix|win|x11))?')
_TEST_SUFFIX = r'(?:_(?:browser|interactive_ui|ui|unit)?test)?'


def IsPrimaryInclude(decorated_name, filename):
    """Return if this decorated include is a primary include.

    Per the style guide, if moo.cc's main purpose is to implement or test the
    functionality in moo.h, moo.h should be ordered first in the includes.

    Args:
      decorated_name: A decorated name of an include.
      filename: The filename to use as the basis for finding the primary header.
    """
    # Header files never have a primary include.
    if filename.endswith('.h'):
        return False

    basis = _DecomposePath(filename)

    # The list of includes is searched in reverse order of length. Even though
    # matching is fuzzy, moo_posix.h should take precedence over moo.h when
    # considering moo_posix.cc.
    header_type = ClassifyHeader(decorated_name)
    if header_type != HEADER_TYPE_USER:
        return False
    to_test = _DecomposePath(UndecorateName(decorated_name))

    # If the basename to test is longer than the basis, just skip it and
    # continue. moo.c should never match against moo_posix.h.
    if len(to_test[1]) > len(basis[1]):
        return False

    # The basename in the two paths being compared need to fuzzily match.
    # This allows for situations where moo_posix.cc implements the interfaces
    # defined in moo.h.
    escaped_basename = re.escape(to_test[1])
    if not (re.match(escaped_basename + _PLATFORM_SUFFIX + _TEST_SUFFIX + '$',
                     basis[1]) or
            re.match(escaped_basename + _TEST_SUFFIX + _PLATFORM_SUFFIX + '$',
                     basis[1])):
        return False

    return True


def GuessBlockToInsert(include_blocks, header_type):
    """Guess position to insert a header of given type.

    Args:
      include_blocks: Blocks of includes in the original file
      header_type: type of header to be inserted

    Returns:
      A tuple of (begin, end) indices which are:
        * if there is a block of same type in the original file, the same as the
          (begin, end) of that block,
        * otherwise both the index of the empty line where the new header should
          be inserted
    """
    logging.debug(f'Guessing where to include {header_type} header...')
    logging.debug(f'Blocks are {include_blocks}')
    # Experiment shows more likely to be found from bottom.
    for begin, end, block_type in reversed(include_blocks):
        # Insert to this block if its type is "close enough" to target header type.
        if header_type - 0.5 <= block_type and block_type <= header_type + 0.5:
            logging.debug(
                f'\tInsert into block ({block_type}, {begin+1}, {end+1})')
            return (begin, end)

        # Target header belongs to a type between the type of this block and the
        # block below, but not "close enough" to any of them, create a new block
        # between them.
        if block_type < header_type - 0.5:
            logging.debug(
                f'\tInsert after block ({block_type}, {begin+1}, {end+1})')
            return (end + 1, end + 1)

    # No appropriate block found from list. Insert to right before the first block
    # (there is always a body block).
    logging.debug(f'\tInsert before block ({include_blocks[0][2]}, ' \
                  f'{include_blocks[0][0]+1}, {include_blocks[0][1]+1})')
    return (begin, begin)


def IsCommentThisAndNext(line, in_comment_block):
    """Determine if this and next lines are comment, respectively.

    Args:
      line: This line.
      in_comment_block: Whether or not this is in a comment block, from previous
      lines.

    Returns:
      is_comment: whether this line is a comment, could be a single-line
      comment, start/ middle/ end of a comment block.
      in_comment_block_next: whether the next line is in a comment block
    """
    comment_line = COMMENT_RE.match(line)
    comment_start = comment_line.group(1) == '/*' if comment_line else False
    comment_end = COMMENT_END_RE.match(line)

    is_comment = comment_line or in_comment_block
    in_comment_block_next = ((comment_start or in_comment_block)
                             and not comment_end)

    return is_comment, in_comment_block_next


def InsertAt(filename, source, decorated_name, target_header_type):
    """Find indices of block to insert new header with heuristics.
    If there is already a block containing header of same type, insert into it.
    Otherwise, assign a type value to block (average of all its headers), and
    insert to one close enough or create a new block.

    Args:
      filename: The name of the source file, for determining primary include.
      source: The contents of the source file in list of lines.
      decorated_name: The decorated name of the header to add.
      target_header_type: The type of header to be added.

    Returns:
      None if source already contains target header (no change is needed),
      otherwise the (begin, end) indices of range to insert header where begin is
      inclusive and end is exclusive.
      begin == end if new block should be created.
    """
    undecorated_name = UndecorateName(decorated_name)

    blocks = []
    begin = -1
    candidate_block = False
    type_total = 0
    in_comment_block = False
    in_extern_c_block = False

    for idx, line in enumerate(source):
        logging.debug(f'({idx+1}): {line}')

        is_comment, in_comment_block = IsCommentThisAndNext(
            line, in_comment_block)
        if is_comment or in_comment_block:
            continue

        # Aassume a new include should not be inserted inside an extern C block,
        # so ignore everyting inside that block.
        if EXTERN_C_RE.match(line):
            logging.debug(f'({idx+1}): {line} extern c begin - skip this line')
            in_extern_c_block = True
            continue
        if in_extern_c_block and EXTERN_C_END_RE.match(line):
            logging.debug(f'({idx+1}): {line} extern c end - skip this line')
            in_extern_c_block = False
            continue
        if in_extern_c_block:
            logging.debug(f'({idx+1}): {line} extern c block - skip this line')
            continue

        m = INCLUDE_RE.match(line)
        if m:
            # Update begin index of the include block.
            if begin < 0:
                begin = idx

            header_name = m.group(2)
            # Original source already contains target header (ignore decorator), no
            # change is needed.
            if UndecorateName(header_name) == undecorated_name:
                return None, None
            header_type = ClassifyHeader(header_name)

            if header_type == HEADER_TYPE_INVALID:
                logging.warning(f'{filename} ({idx+1}): include {m.group(2)} found ' \
                                f'but cannot be classified.')
                continue

            # Override result of ClassifyHeader if it is primary include.
            if len(blocks) == 0 and IsPrimaryInclude(header_name, filename):
                logging.debug(f'Found primary header.')
                header_type = HEADER_TYPE_PRIMARY

            # A block containing header of same type found, mark it as candidate.
            if header_type == target_header_type:
                candidate_block = True

            # For guessing type of this block of includes.
            type_total += header_type
            continue

        # Not an include line.

        # Finished parsing one block
        if begin >= 0:
            # Return this block directly if it is a candidate block.
            if candidate_block:
                logging.debug(
                    f'Found candidate block from line {begin+1} to {idx}')
                return (begin, idx)

            # Add block to list, reset state, and continue reading.
            type_avg = type_total / (idx - begin)
            blocks.append((begin, idx, type_avg))
            begin = -1
            type_total = 0
            continue

        # Hit real code, add body block (for inserting a new block of include when
        # there is no include at all).
        if (not EMPTY_LINE_RE.match(line) and not MACRO_RE.match(line)):
            logging.debug(f'Hit real code, finished parsing includes.')
            blocks.append((idx, -1, BODY))
            break

    # Candidate block not found, guess where to insert from the list of blocks.
    logging.debug(f'No candidate block found, guess from list.')
    return GuessBlockToInsert(blocks, target_header_type)


def AddHeaderToSource(filename,
                      source,
                      decorated_name,
                      header_type,
                      comment=''):
    """Adds the specified header into the source text with minimal changes.

    Args:
      filename: The name of the source file.
      source: The contents of the source file in list of lines.
      decorated_name: The decorated name of the header to add.
      header_type: The type of header to be added.
      comment: In-line comment following the include header.

    Returns:
      None if no changes are needed or the modified source text otherwise.
    """
    logging.debug(f'Trying to add {decorated_name} to file.')
    # Begin (inclusive) and end (exclusive) indices of the block to insert the new
    # header to; or both None if no change (header already included).
    begin, end = InsertAt(filename, source, decorated_name, header_type)
    logging.debug(f'begin, end = {begin}, {end}')

    if not begin:
        return None

    # Add include to end of block, i.e. at index end.
    source.insert(end, '#include %s%s' % (decorated_name, comment))

    # There is no block in original file of the same type, insert extra empty line
    # after this include statement. No need to sort since it has one line only.
    if begin == end:
        source.insert(end + 1, '')
    # Sort the block. Length increased by one after inserting new include.
    else:
        source[begin:end + 1] = sorted(source[begin:end + 1])

    return source


def RemoveHeaderFromSource(source, name):
    """Removes the specified header from the source text.

    Args:
      source: The contents of the source file in list of lines.
      name: The decorated or undecorated name of the header to remove.

    Returns:
      The modified source text in list of lines, removed header (for
      --same-decorator option with replace operation), in-line comments
      or (None, None) if nothing to remove.
    """
    logging.debug(f'Trying to remove {name} from file.')
    undecorated_name = UndecorateName(name)

    file_length = len(source)
    in_comment_block = False
    in_extern_c_block = False

    for idx, line in enumerate(source):
        logging.debug(f'({idx}): {line}')

        is_comment, in_comment_block = IsCommentThisAndNext(
            line, in_comment_block)
        if is_comment or in_comment_block:
            continue

        # Sometimes extern C blocks are in the include section.  Ignore the
        # lines for extern C but still check the includes inside extern C for in
        # case that is the file to be removed.
        if EXTERN_C_RE.match(line):
            logging.debug(f'({idx}): {line} extern c begin - skip this line')
            in_extern_c_block = True
            continue
        if in_extern_c_block and EXTERN_C_END_RE.match(line):
            logging.debug(f'({idx}): {line} extern c end - skip this line')
            in_extern_c_block = False
            continue

        m = INCLUDE_RE.match(line)
        if m:
            # This include's filepath matches target header filepath, and
            # decorator also matches if input is decorated.
            if UndecorateName(m.group(2)) == undecorated_name:
                # Delete empty line after if it belongs to a single-line block.
                if ((idx > 0 and source[idx - 1] == '')
                        and (idx + 1 < file_length and source[idx + 1] == '')):
                    del source[idx + 1]
                del source[idx]
                logging.debug(
                    f'Header {m.group(2)} found, removing from file.')
                return source, m.group(2), m.group(3)

        # Hitting body of code, stop looking.
        if (not EMPTY_LINE_RE.match(line) and not MACRO_RE.match(line)):
            break

    return None, None, None


def ReplaceHeaderWithMinimumSorting(source, old_header, new_header, prefix,
                                    same_decorator):
    """Replaces the old header by the new one from the source text.
       If old header doesn't exist, do nothing.
       If new header (regardless of decorators) already exists, only delete the
       old one without sorting.

    Args:
      source: The contents of the source file in list of lines.
      old_header: The decorated or undecorated name of the header to remove; in
      both cases match only by the path (decorator will be removed).
      new_header: The decorated or undecorated name of the header to remove; if
      same_decorator is False it must be decorated.
      prefix: indicates sorting range (only the includes surrounding the replaced
      header AND starting with sort_only_prefix will be sorted).
      same_decorator: Whether or not to use the decorator of replaced header for
      the replacement.

    Returns:
      The modified source text in list of lines, or None if old header not found.
    """
    old_header_idx = -1
    new_header_idx = -1
    in_comment_block = False
    in_extern_c_block = False

    # Look for old header (to replace) and check new header does not already
    # exist.
    for idx, line in enumerate(source):
        logging.debug(f'({idx}): {line}')
        # Skip empty line.
        if not line:
            continue
        # Skip comment.
        is_comment, in_comment_block = IsCommentThisAndNext(
            line, in_comment_block)
        if is_comment or in_comment_block:
            continue

        # Sometimes extern C blocks are in the include section.  Ignore the
        # lines for extern C but still check the includes inside extern C.
        if EXTERN_C_RE.match(line):
            logging.debug(f'({idx}): {line} extern c begin - skip this line')
            in_extern_c_block = True
            continue
        if in_extern_c_block and EXTERN_C_END_RE.match(line):
            logging.debug(f'({idx}): {line} extern c end - skip this line')
            in_extern_c_block = False
            continue

        m = INCLUDE_RE.match(line)
        if m:
            if m.group(2)[1:-1] == UndecorateName(old_header):
                logging.debug(f'Found old header.')
                if new_header_idx > -1:  # Delete the line if new header exists.
                    del source[idx]
                    return source
                old_header_idx = idx
            elif m.group(2)[1:-1] == UndecorateName(new_header):
                logging.debug(f'Found new header.')
                if old_header_idx > -1:  # Delete the old header line if exists.
                    del source[old_header_idx]
                    return source
                new_header_idx = idx

        # Hitting body of code, stop looking.
        if not MACRO_RE.match(line):
            break

    if old_header_idx < 0:  # If old header is not found, do nothing.
        return None

    # Only the old header exists but not the new header.
    # Replace the include header path.
    OLD_HEADER_RE = re.compile('([<"])' + UndecorateName(old_header) +
                               '([>"])')
    if same_decorator:
        # (Re)decorate new header by those of the old header (retrieved from
        # pattern).
        source[old_header_idx] = OLD_HEADER_RE.sub(
            r'\1{}\2'.format(UndecorateName(new_header)),
            source[old_header_idx])
    else:
        # New header is guaranteed to be decorated.
        source[old_header_idx] = OLD_HEADER_RE.sub(r'{}'.format(new_header),
                                                   source[old_header_idx])
    logging.debug(
        f'Replaced old header include line with new path: {source[old_header_idx]}'
    )

    # Look at includes before and after the modified include for those with
    # matching prefix.
    for i_delta, line in enumerate(reversed(source[:old_header_idx])):
        m = INCLUDE_RE.match(line)
        if not m or not UndecorateName(m.group(2)).startswith(prefix):
            start = old_header_idx - i_delta
            break
    for i_delta, line in enumerate(source[old_header_idx:]):
        m = INCLUDE_RE.match(line)
        if not m or not UndecorateName(m.group(2)).startswith(prefix):
            end = old_header_idx + i_delta
            break
    logging.debug(
        f'Resorting lines [{start}, {end}) surrounding replaced header line at {old_header_idx}.'
    )
    # Overwrite the range with itself sorted.
    source[start:end] = sorted(source[start:end])
    return source


def ReplaceHeader(source, old_header, new_header, same_decorator, filename):
    """Replaces the old header by the new one from the source text.
       If old header doesn't exist, do nothing.
       Resort blocks touched.

    Args:
      source: The contents of the source file in list of lines.
      old_header: The decorated or undecorated name of the header to remove; in
      both cases match only by the path (decorator will be removed).
      new_header: The decorated or undecorated name of the header to remove; if
      same_decorator is False it must be decorated.
      same_decorator: Whether or not to use the decorator of replaced header for
      the replacement.
      filename: Filename of the file being modified.

    Returns:
      The modified source text in list of lines, or None if old header not found.
    """
    source, removed_header, comment = RemoveHeaderFromSource(
        source, old_header)
    if not source:
        return None
    new_header = (CopyDecoratorTo(new_header, removed_header)
                  if same_decorator else new_header)
    new_source = AddHeaderToSource(os.path.normpath(filename), source,
                                   new_header, ClassifyHeader(new_header),
                                   comment if comment else '')
    # new_source is None if new header already exists, but source has been
    # modified by removing the old header.
    return new_source if new_source else source


def main():
    parser = argparse.ArgumentParser(
        description='Mass change include header of files.')

    # Add, remove, or replace a header by a new one; perform exactly one of the
    # operators.
    group = parser.add_mutually_exclusive_group()
    group.add_argument(
        '--add',
        help='The decorated filename of the header to insert (e.g. "a" or <a>).'
    )
    group.add_argument(
        '--remove',
        help='The decorated or undecorated filename of the header to delete '   \
            '(e.g. "a" or <a>). If decorated, an include will be removed only ' \
            'if decorator also matches, otherwise, an include with same '       \
            'filename will be removed regardless of decorator. '                \
            'No change to file if no match is found.')
    group.add_argument(
        '--replace',
        nargs=2,
        metavar=('OLD', 'NEW'),
        help='Equivalent to --remove OLD and --add NEW if OLD is found.')

    parser.add_argument(
        '--same_decorator',
        help='Use same decorator found in removed header OLD when adding new; ' \
            'for replace operation only.',
        action='store_true')
    parser.add_argument(
        '--sort_only_prefix',
        help='Prefix of blocks of include headers to be reordered after '       \
            'replacing header in-place, it should match both the replaced and ' \
            'replacement headers; for replace operation only.'                  \
    )

    parser.add_argument(
        '--verbosity', type=str,
        help='Verbosity level of logs; one of DEBUG, INFO, WARNING, ERROR. ' \
            'Default is INFO.',
        default='INFO')

    parser.add_argument(
        '--dryrun',
        help='Run the script but do not change the actual file. For debugging.',
        default=False,
        action='store_true')

    parser.add_argument('files', nargs='+')
    args = parser.parse_args()

    assert args.add or args.remove or args.replace, \
           f'Exactly one action out of add, remove, or replace is required.'

    assert args.replace or not args.same_decorator, \
           f'--same_decorator option is only applicable to replace operation.'
    assert args.replace or not args.sort_only_prefix, \
           f'--sort_only_prefix option is only applicable to replace operation.'

    if args.replace:
        assert args.same_decorator or IsDecorated(args.replace[1]), \
               f'Provide decorated new header or use --same-decorator option.'

    if args.sort_only_prefix:
        SORT_ONLY_PREFIX_RE = re.compile(r'[<"]?{}'.format(
            args.sort_only_prefix))
        assert (SORT_ONLY_PREFIX_RE.match(args.replace[0]) and
                SORT_ONLY_PREFIX_RE.match(args.replace[1])), \
               f'--sort_only_prefix should be prefix to both replaced and replacement headers.'

    assert args.verbosity in ['DEBUG', 'INFO', 'WARNING', 'ERROR'], \
        f'Verbosity level should be one of DEBUG, INFO, WARNING, ERROR.'
    logging.getLogger().setLevel(args.verbosity)

    if args.add:
        logging.info(f'Adding {args.add} to files.')
    elif args.remove:
        logging.info(f'Removing {args.remove} from files.')
    else:  # args.replace
        decorator_log = ' with same decorator.' if args.same_decorator else "."
        logging.info(f'Replacing {args.replace[0]} with {args.replace[1]} from ' \
                     f'files{decorator_log}' + '\n')

    for filename in args.files:
        if not filename.endswith('cc') and not filename.endswith(
                'h') and not filename.endswith('cpp'):
            logging.info(f'Skip non-c++ file {filename}...')
            continue
        with open(filename, 'r') as f:
            logging.info(f'Processing {filename}...')
            source = f.read().splitlines()
            if args.remove:
                source, _, _ = RemoveHeaderFromSource(source, args.remove)
                if not source:
                    logging.info(
                        f'To be removed header {args.remove} not found, skipping file.'
                    )
            elif args.replace:
                if args.sort_only_prefix:
                    source = ReplaceHeaderWithMinimumSorting(
                        source, args.replace[0], args.replace[1],
                        args.sort_only_prefix, args.same_decorator)
                else:
                    source = ReplaceHeader(source, args.replace[0],
                                           args.replace[1],
                                           args.same_decorator, filename)
                if not source:
                    logging.info(
                        f'To be replaced header {args.replace[0]} not found, skipping file.'
                    )
            else:  # args.add
                source = AddHeaderToSource(os.path.normpath(filename), source,
                                           args.add, ClassifyHeader(args.add))

        if not source or args.dryrun:
            continue
        with open(filename, 'w', newline='\n') as f:
            source.append(
                '')  # To avoid eating the newline at the end of the file.
            f.write('\n'.join(source))


if __name__ == '__main__':
    sys.exit(main())
