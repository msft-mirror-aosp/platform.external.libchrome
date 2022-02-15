#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
import subprocess
import tempfile
import unittest
from change_header import *

class TestClassifyHeader(unittest.TestCase):
  def test_is_system_header(self):
    self.assertTrue(IsSystemHeader('<sys/time.h>'))
    self.assertTrue(IsSystemHeader('<memory>'))

    self.assertFalse(IsSystemHeader('"foo.h"'))

  def test_is_c_system_header(self):
    self.assertTrue(IsCSystemHeader('<unistd.h>'))

    self.assertFalse(IsCSystemHeader('<tuple>'))
    self.assertFalse(IsCSystemHeader('"bar.h"'))

  def test_is_cxx_system_header(self):
    self.assertTrue(IsCXXSystemHeader('<optional>'))

    self.assertFalse(IsCXXSystemHeader('<signal.h>'))
    self.assertFalse(IsCXXSystemHeader('"baz.h"'))

  def test_is_cros_header(self):
    self.assertTrue(IsCrOSHeader('<base/macros.h>'))
    self.assertTrue(IsCrOSHeader('"base/strings/string_number_conversions.h"'))
    self.assertTrue(IsCrOSHeader('<ipc/ipc.h>'))
    self.assertTrue(IsCrOSHeader('<mojo/core/core.h>'))
    self.assertTrue(IsCrOSHeader('"dbus/message.h"'))
    self.assertTrue(IsCrOSHeader('"gtest/gtest.h"'))
    self.assertTrue(IsCrOSHeader('"brillo/dbus/exported_object_manager.h"'))

    self.assertFalse(IsCrOSHeader('<stdlib.h>'))
    self.assertFalse(IsCrOSHeader('<utility>'))

  def test_is_decorated_true(self):
    self.assertTrue(IsDecorated('<foo>'))
    self.assertTrue(IsDecorated('<foo.h>'))
    self.assertTrue(IsDecorated('"foo.h"'))

    self.assertFalse(IsDecorated('foo.h'))
    self.assertFalse(IsDecorated('foo'))
    self.assertFalse(IsDecorated('"foo.h'))
    self.assertFalse(IsDecorated('foo.h"'))
    self.assertFalse(IsDecorated('<foo.h"'))
    self.assertFalse(IsDecorated('foo.h>'))

  def test_classify_header(self):
    self.assertEqual(ClassifyHeader('<sys/socket.h>'), HEADER_TYPE_C_SYSTEM)
    self.assertEqual(ClassifyHeader('<iostream>'), HEADER_TYPE_CXX_SYSTEM)
    self.assertEqual(ClassifyHeader('<base/notrrached.h>'), HEADER_TYPE_CROS)
    self.assertEqual(ClassifyHeader('"base/logging.h"'), HEADER_TYPE_CROS)
    self.assertEqual(ClassifyHeader(
        '<mojo/core/embedder/scoped_ipc_support.h>'), HEADER_TYPE_CROS)
    self.assertEqual(ClassifyHeader('<dbus/object_proxy.h>'), HEADER_TYPE_CROS)
    self.assertEqual(ClassifyHeader('"brillo/variant_dictionary.h"'),
                     HEADER_TYPE_CROS)
    self.assertEqual(ClassifyHeader('"vm_tools/vsh/scoped_termios.h"'),
                     HEADER_TYPE_USER)

  def test_is_primary_include(self):
    # when called in repository root, e.g. platform2/
    self.assertTrue(IsPrimaryInclude('"vm_tools/vsh/vsh_client.h"',
                                     "vm_tools/vsh/vsh_client.cc"))
    # when called in inidividual package directory, e.g. vm_tools/
    self.assertTrue(IsPrimaryInclude('"vm_tools/vsh/vsh_client.h"',
                                     "vsh/vsh_client.cc"))

    self.assertFalse(IsPrimaryInclude('"vm_tools/vsh/vsh_client.h"',
                                      "vm_toost/vsh/vsh.cc"))
    self.assertFalse(IsPrimaryInclude('"vm_tools/vsh/vsh_client.h"',
                                      "vm_toost/vsh/vsh.h"))
    self.assertFalse(IsPrimaryInclude('"vm_tools/vsh/vsh.cc"',
                                      "vm_toost/vsh/vsh_client.h"))
    self.assertFalse(IsPrimaryInclude('"vm_tools/vsh/vsh.cc"',
                                      "vm_toost/vsh/vsh_client.h"))


class TestRegularExpressions(unittest.TestCase):
  def test_empty_line(self):
    self.assertTrue(EMPTY_LINE_RE.match(''))

    self.assertFalse(EMPTY_LINE_RE.match('foo'))
    self.assertFalse(EMPTY_LINE_RE.match('// foo'))

  def test_comment(self):
    m = COMMENT_RE.match('/* foo')
    self.assertTrue(m)
    self.assertEqual(m.group(1), '/*')

    m = COMMENT_RE.match('/* foo */')
    self.assertTrue(m)
    self.assertEqual(m.group(1), '/*')

    m = COMMENT_RE.match('// foo')
    self.assertTrue(m)
    self.assertEqual(m.group(1), '//')

    self.assertFalse(COMMENT_RE.match(' * foo'))
    self.assertFalse(COMMENT_RE.match(' * foo */'))
    self.assertFalse(COMMENT_RE.match('#include <vector>'))
    self.assertFalse(COMMENT_RE.match(''))
    self.assertFalse(COMMENT_RE.match('namespace base {'))

  def test_comment_end(self):
    self.assertTrue(COMMENT_END_RE.match('/* foo */'))
    self.assertTrue(COMMENT_END_RE.match(' * foo */'))

    self.assertFalse(COMMENT_END_RE.match('/* foo'))
    self.assertFalse(COMMENT_END_RE.match('// foo'))
    self.assertFalse(COMMENT_RE.match('#include <vector>'))
    self.assertFalse(COMMENT_RE.match(''))
    self.assertFalse(COMMENT_RE.match('namespace base {'))

  def test_include(self):
    self.assertTrue(INCLUDE_RE.match('#include <vector>'))
    self.assertTrue(INCLUDE_RE.match('#include <time.h>'))
    self.assertTrue(INCLUDE_RE.match('#include "base/time/time.h"'))

    self.assertFalse(INCLUDE_RE.match('// #include <vector>'))
    self.assertFalse(INCLUDE_RE.match('using ::testing::_;'))
    self.assertFalse(INCLUDE_RE.match('class Foo {'))

  def test_macro(self):
    self.assertTrue(MACRO_RE.match('#include <vector>'))
    self.assertTrue(MACRO_RE.match('#ifdef __GNUG'))
    self.assertTrue(MACRO_RE.match('#define TRUNKS_SCOPED_GLOBAL_SESSION_H_'))
    self.assertTrue(MACRO_RE.match('#if BASE_VER > 12345'))
    self.assertTrue(MACRO_RE.match('#endif'))

    self.assertFalse(MACRO_RE.match('// #include <vector>'))
    self.assertFalse(MACRO_RE.match('using ::testing::_;'))
    self.assertFalse(MACRO_RE.match('class Foo {'))


class TestIsCommentThisAndNext(unittest.TestCase):
  def test_not_comment(self):
    is_comment, in_comment_block = IsCommentThisAndNext('#include "foo.h"', False)
    self.assertFalse(is_comment)
    self.assertFalse(in_comment_block)


  def test_comment_line(self):
    is_comment, in_comment_block = IsCommentThisAndNext('// foo', False)
    self.assertTrue(is_comment)
    self.assertFalse(in_comment_block)

    is_comment, in_comment_block = IsCommentThisAndNext('// foo', True)
    self.assertTrue(is_comment)
    self.assertTrue(in_comment_block)

  def test_comment_block_start(self):
    is_comment, in_comment_block = IsCommentThisAndNext('/* foo', False)
    self.assertTrue(is_comment)
    self.assertTrue(in_comment_block)

    is_comment, in_comment_block = IsCommentThisAndNext('/* foo', True)
    self.assertTrue(is_comment)
    self.assertTrue(in_comment_block)

  def test_comment_block_middle(self):
    is_comment, in_comment_block = IsCommentThisAndNext(' public:', True)
    self.assertTrue(is_comment)
    self.assertTrue(in_comment_block)

  def test_comment_block_end(self):
    is_comment, in_comment_block = IsCommentThisAndNext(' * foo */', True)
    self.assertTrue(is_comment)
    self.assertFalse(in_comment_block)

  def test_comment_block_single_line(self):
    is_comment, in_comment_block = IsCommentThisAndNext('/* foo */', False)
    self.assertTrue(is_comment)
    self.assertFalse(in_comment_block)

class TestCommandArguments(unittest.TestCase):
  def test_not_one_operation(self):
    output = subprocess.run('./change_header.py'.split(), stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    self.assertNotEqual(output.returncode, 0)

    output = subprocess.run('./change_header.py --add foo --remove bar'.split(),
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    self.assertNotEqual(output.returncode, 0)
    self.assertIn('not allowed with argument', str(output.stderr))

  def test_missing_header_param(self):
    output = subprocess.run('./change_header.py --add'.split(),
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    self.assertNotEqual(output.returncode, 0)
    self.assertIn('--add: expected one argument', str(output.stderr))
    output = subprocess.run('./change_header.py --remove'.split(),
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    self.assertNotEqual(output.returncode, 0)
    self.assertIn('--remove: expected one argument', str(output.stderr))
    output = subprocess.run('./change_header.py --replace'.split(),
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    self.assertNotEqual(output.returncode, 0)
    self.assertIn('--replace: expected 2 arguments', str(output.stderr))
    output = subprocess.run('./change_header.py --replace foo'.split(),
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    self.assertNotEqual(output.returncode, 0)
    self.assertIn('--replace: expected 2 arguments', str(output.stderr))

  def test_missing_file(self):
    output = subprocess.run('./change_header.py --add foo'.split(),
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    self.assertNotEqual(output.returncode, 0)
    self.assertIn('the following arguments are required: files',
                  str(output.stderr))
    output = subprocess.run('./change_header.py --remove foo'.split(),
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    self.assertNotEqual(output.returncode, 0)
    self.assertIn('the following arguments are required: files',
                  str(output.stderr))
    output = subprocess.run('./change_header.py --replace foo bar'.split(),
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    self.assertNotEqual(output.returncode, 0)
    self.assertIn('the following arguments are required: files',
                  str(output.stderr))

  def test_unknown_verbosity(self):
    output = subprocess.run(
        './change_header.py --add foo --verbosity info bar.cc'.split(),
        stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    self.assertNotEqual(output.returncode, 0)
    self.assertIn(
        'Verbosity level should be one of DEBUG, INFO, WARNING, ERROR',
        str(output.stderr))

class TestRemoveHeader(unittest.TestCase):
  def setUp(self):
    self.filename = '../testdata/change_header_test.cc'
    with open(self.filename, 'r') as f:
      self.source = f.read().splitlines()

  def test_remove_header_not_there(self):
    header = '<utility>'
    source, removed_header = RemoveHeaderFromSource(self.source, header)
    self.assertIsNone(source)
    self.assertIsNone(removed_header)

  def test_remove_header_wrong_decorator(self):
    header = '"base/logging.h"'
    source, removed_header = RemoveHeaderFromSource(self.source, header)
    self.assertIsNone(source)
    self.assertIsNone(removed_header)

  def test_remove_header_same_decorator(self):
    header = '<base/strings/string_util.h>'
    expected_source = self.source.copy()
    del expected_source[expected_source.index(f'#include {header}')]

    source, removed_header = RemoveHeaderFromSource(self.source, header)
    self.assertIsNotNone(source)
    self.assertIsNotNone(removed_header)

    self.assertEqual(source, expected_source)
    self.assertEqual(header, removed_header)

  def test_remove_header_any_decorator(self):
    header = 'brillo/flag_helper.h'
    expected_source = self.source.copy()
    del expected_source[expected_source.index(f'#include <{header}>')]

    source, removed_header = RemoveHeaderFromSource(self.source, header)
    self.assertIsNotNone(source)
    self.assertIsNotNone(removed_header)

    self.assertEqual(source, expected_source)
    self.assertEqual(f'<{header}>', removed_header)

  def test_remove_header_with_line_break(self):
    header = '<memory>'
    expected_source = self.source.copy()
    idx = expected_source.index(f'#include {header}')
    del expected_source[idx]
    del expected_source[idx]

    source, removed_header = RemoveHeaderFromSource(self.source, header)
    self.assertIsNotNone(source)
    self.assertIsNotNone(removed_header)

    self.assertEqual(source, expected_source)
    self.assertEqual(header, removed_header)


class TestAddHeader(unittest.TestCase):
  def setUp(self):
    self.filename = '../testdata/change_header_test.cc'
    with open(self.filename, 'r') as f:
      self.source = f.read().splitlines()

  def test_add_c_system_header_new_block(self):
    header = '<stdio.h>'
    expected_source = self.source.copy()
    expected_source.insert(5, f'#include {header}')
    expected_source.insert(5, f'')

    source = AddHeaderToSource(os.path.normpath(self.filename), self.source,
                               header, ClassifyHeader(header))

    self.assertIsNotNone(source)
    self.assertEqual(source, expected_source)

  def test_add_cpp_system_header(self):
    header = '<utility>'
    expected_source = self.source.copy()
    expected_source.insert(7, f'#include {header}')

    source = AddHeaderToSource(os.path.normpath(self.filename), self.source,
                               header, ClassifyHeader(header))

    self.assertIsNotNone(source)
    self.assertEqual(source, expected_source)

  def test_add_libchrome_header(self):
    header = '<base/check.h>'
    expected_source = self.source.copy()
    expected_source.insert(8, f'#include {header}')

    source = AddHeaderToSource(os.path.normpath(self.filename), self.source,
                               header, ClassifyHeader(header))

    self.assertIsNotNone(source)
    self.assertEqual(source, expected_source)

  def test_add_header_already_there(self):
    header = '<base/strings/string_number_conversions.h>'

    source = AddHeaderToSource(os.path.normpath(self.filename), self.source,
                               header, ClassifyHeader(header))

    self.assertIsNone(source)


if __name__ == '__main__':
  unittest.main()
