#!/usr/bin/env python
# Copyright 2022 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import elftools.elf.elffile
import subprocess
import re


def read_symbol(elfpath, dynamic=False):
    """
    Reads global symbols provided or wanted by given binary.

    Args:
      elfpath: path to the binary.
      dynamic: reads .dynsym rather than .symtab.
    """
    provides = []
    wants = []

    with open(elfpath, 'rb') as file:
        elf = elftools.elf.elffile.ELFFile(file)
        symtab = elf.get_section_by_name('.dynsym' if dynamic else '.symtab')
        assert symtab, elfpath
        symbols = collections.defaultdict(list)
        for symbol in symtab.iter_symbols():
            if not symbol.name:
                continue
            # Weak symbols don't provide actual implementations. Use STB_GLOBAL
            # only.
            if symbol.entry.st_info.bind != 'STB_GLOBAL':
                continue
            assert symbol.entry.st_info.type in [
                'STT_FUNC', 'STT_NOTYPE', 'STT_OBJECT'
            ], (elfpath, symbol.name, symbol.entry)
            idx = symbol.entry.st_shndx
            if idx == 'SHN_UNDEF':
                wants.append(symbol.name)
            else:
                assert symbol.entry.st_info.type != 'STT_NOTYPE', symbol
                provides.append(symbol.name)

    return provides, wants
