// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_POLICY_PROTO_DECODERS_H_
#define COMPONENTS_POLICY_CORE_COMMON_POLICY_PROTO_DECODERS_H_

#include "base/memory/weak_ptr.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_export.h"

namespace enterprise_management {
class CloudPolicySettings;
}  // namespace enterprise_management

namespace policy {

class CloudExternalDataManager;
class PolicyMap;

enum class PolicyPerProfileFilter {
  // Applies to the browser profile.
  kTrue,
  // Applies to all browser instances.
  kFalse,
  // Any user policy.
  kAny
};

// Decode all of the fields in |policy| which are recognized (see the metadata
// in policy_constants.cc) and store them in the given |map|, with the given
// |source| and |scope|. Deprecated: Use DecodeProtoFieldsPerProfile instead.
POLICY_EXPORT void DecodeProtoFields(
    const enterprise_management::CloudPolicySettings& policy,
    base::WeakPtr<CloudExternalDataManager> external_data_manager,
    PolicySource source,
    PolicyScope scope,
    PolicyMap* map);

// Decode all the fields in |policy| that match the needed |per_profile| flag
// which are recognized (see the metadata in policy_constants.cc) and store them
// in the given |map|, with the given |source| and |scope|. In case
// |per_profile| is nullopt, the flag is ignored and all the policies are
// included.
POLICY_EXPORT void DecodeProtoFieldsPerProfile(
    const enterprise_management::CloudPolicySettings& policy,
    base::WeakPtr<CloudExternalDataManager> external_data_manager,
    PolicySource source,
    PolicyScope scope,
    PolicyMap* map,
    PolicyPerProfileFilter per_profile);

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_POLICY_PROTO_DECODERS_H_
