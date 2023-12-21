// Copyright 2020 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "change_header.h"

#include <memory>

extern "C" {
#include <vboot/vboot_host.h>
}

#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>
#include <brillo/flag_helper.h>

namespace foo {

}  // namespace foo
