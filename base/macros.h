// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains macros and macro-like constructs (e.g., templates) that
// are commonly used throughout Chromium source. (It may also contain things
// that are closely related to things that are commonly used that belong in this
// file.)

#ifndef BASE_MACROS_H_
#define BASE_MACROS_H_

#if defined(ANDROID)
// Prefer Android's libbase definitions to our own.
#include <android-base/macros.h>
#endif  // defined(ANDROID)

// ALL DISALLOW_xxx MACROS ARE DEPRECATED; DO NOT USE IN NEW CODE.
// Use explicit deletions instead.  See the section on copyability/movability in
// //styleguide/c++/c++-dos-and-donts.md for more information.

// Put this in the declarations for a class to be uncopyable.
// DEPRECATED: See above. Makes a class uncopyable.
#if !defined(DISALLOW_COPY)
#define DISALLOW_COPY(TypeName) \
  TypeName(const TypeName&) = delete
#endif

// Put this in the declarations for a class to be unassignable.
// DEPRECATED: See above. Makes a class unassignable.
#if !defined(DISALLOW_ASSIGN)
#define DISALLOW_ASSIGN(TypeName) TypeName& operator=(const TypeName&) = delete
#endif

// Put this in the declarations for a class to be uncopyable and unassignable.
// DEPRECATED: See above. Makes a class uncopyable and unassignable.
#if !defined(DISALLOW_COPY_AND_ASSIGN)
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  DISALLOW_COPY(TypeName);                 \
  DISALLOW_ASSIGN(TypeName)
#endif

// DEPRECATED: See above. Disallow all implicit constructors, namely the
// default constructor, copy constructor and operator= functions.
// This is especially useful for classes containing only static methods.
#if !defined(DISALLOW_IMPLICIT_CONSTRUCTORS)
#define DISALLOW_IMPLICIT_CONSTRUCTORS(TypeName) \
  TypeName() = delete;                           \
  DISALLOW_COPY_AND_ASSIGN(TypeName)
#endif

// Used to explicitly mark the return value of a function as unused. If you are
// really sure you don't want to do anything with the return value of a function
// that has been marked WARN_UNUSED_RESULT, wrap it with this. Example:
//
//   std::unique_ptr<MyType> my_var = ...;
//   if (TakeOwnership(my_var.get()) == SUCCESS)
//     ignore_result(my_var.release());
//
template<typename T>
inline void ignore_result(const T&) {
}

#endif  // BASE_MACROS_H_
