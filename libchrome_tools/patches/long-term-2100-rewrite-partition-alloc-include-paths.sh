#!/bin/bash
# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# crrev.com/c/5023994 changed include paths of partition allocator header files.
# Rewrite them to their original forms.

set -eu

find -type f | xargs sed -i 's|^#include "partition_alloc/|#include "base/allocator/partition_allocator/src/partition_alloc/|g'
