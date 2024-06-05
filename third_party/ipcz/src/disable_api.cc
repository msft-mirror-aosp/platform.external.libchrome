// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/41494930): remove this file once ipcz is enabled on ChromeOS.

// This file is only compiled if the `ipcz` USE flag is disabled,
// to conditionally disable IPCz in libchrome.

#include "api.h"

extern "C" {

IPCZ_EXPORT IpczResult IPCZ_API IpczGetAPI(IpczAPI* api) {
  return IPCZ_RESULT_UNKNOWN;
};

}  // namespace "C"
