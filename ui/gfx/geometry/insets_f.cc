// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/insets_f.h"

#include "base/strings/stringprintf.h"

namespace gfx {

std::string InsetsF::ToString() const {
  // Print members in the same order of the constructor parameters.
  return base::StringPrintf("%f,%f,%f,%f", top(),  left(), bottom(), right());
}

void InsetsF::SetToMax(const gfx::InsetsF& other) {
  top_ = std::max(top_, other.top_);
  left_ = std::max(left_, other.left_);
  bottom_ = std::max(bottom_, other.bottom_);
  right_ = std::max(right_, other.right_);
}

}  // namespace gfx
