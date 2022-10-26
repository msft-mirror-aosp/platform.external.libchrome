// Copyright (C) 2018 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Redirect to system header.
#include <modp_b64/modp_b64.h>

#include <limits.h>

#ifndef MODP_B64_MAX_INPUT_LEN
#define MODP_B64_MAX_INPUT_LEN ((SIZE_MAX - 1) / 4 * 3)
#endif
