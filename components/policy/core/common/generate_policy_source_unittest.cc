// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>
#include <memory>
#include <string>

#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/policy/core/common/policy_details.h"
#include "components/policy/core/common/proxy_settings_constants.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

// This unittest tests the code generated by
// chrome/tools/build/generate_policy_source.py.

namespace policy {

namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Checks if two schemas are the same or not. Note that this function doesn't
// consider restrictions on integers and strings nor pattern properties.
bool IsSameSchema(Schema a, Schema b) {
  if (a.valid() != b.valid())
    return false;
  if (!a.valid())
    return true;
  if (a.type() != b.type())
    return false;
  if (a.type() == base::Value::Type::LIST)
    return IsSameSchema(a.GetItems(), b.GetItems());
  if (a.type() != base::Value::Type::DICTIONARY)
    return true;
  Schema::Iterator a_it = a.GetPropertiesIterator();
  Schema::Iterator b_it = b.GetPropertiesIterator();
  while (!a_it.IsAtEnd()) {
    if (b_it.IsAtEnd())
      return false;
    if (strcmp(a_it.key(), b_it.key()) != 0)
      return false;
    if (!IsSameSchema(a_it.schema(), b_it.schema()))
      return false;
    a_it.Advance();
    b_it.Advance();
  }
  if (!b_it.IsAtEnd())
    return false;
  return IsSameSchema(a.GetAdditionalProperties(), b.GetAdditionalProperties());
}
#endif

}  // namespace

TEST(GeneratePolicySource, ChromeSchemaData) {
  Schema schema = Schema::Wrap(GetChromeSchemaData());
  ASSERT_TRUE(schema.valid());
  EXPECT_EQ(base::Value::Type::DICTIONARY, schema.type());

  Schema subschema = schema.GetAdditionalProperties();
  EXPECT_FALSE(subschema.valid());

  subschema = schema.GetProperty("no such policy exists");
  EXPECT_FALSE(subschema.valid());

  subschema = schema.GetProperty(key::kSearchSuggestEnabled);
  ASSERT_TRUE(subschema.valid());
  EXPECT_EQ(base::Value::Type::BOOLEAN, subschema.type());

  subschema = schema.GetProperty(key::kURLBlocklist);
  ASSERT_TRUE(subschema.valid());
  EXPECT_EQ(base::Value::Type::LIST, subschema.type());
  ASSERT_TRUE(subschema.GetItems().valid());
  EXPECT_EQ(base::Value::Type::STRING, subschema.GetItems().type());

  // Verify that all the Chrome policies are there.
  for (Schema::Iterator it = schema.GetPropertiesIterator(); !it.IsAtEnd();
       it.Advance()) {
    EXPECT_TRUE(it.key());
    EXPECT_FALSE(std::string(it.key()).empty());
    EXPECT_TRUE(GetChromePolicyDetails(it.key()));
  }

#if !BUILDFLAG(IS_IOS)
  subschema = schema.GetProperty(key::kDefaultCookiesSetting);
  ASSERT_TRUE(subschema.valid());
  EXPECT_EQ(base::Value::Type::INTEGER, subschema.type());

  subschema = schema.GetProperty(key::kProxyMode);
  ASSERT_TRUE(subschema.valid());
  EXPECT_EQ(base::Value::Type::STRING, subschema.type());

  subschema = schema.GetProperty(key::kProxySettings);
  ASSERT_TRUE(subschema.valid());
  EXPECT_EQ(base::Value::Type::DICTIONARY, subschema.type());
  EXPECT_FALSE(subschema.GetAdditionalProperties().valid());
  EXPECT_FALSE(subschema.GetProperty("no such proxy key exists").valid());
  ASSERT_TRUE(subschema.GetProperty(key::kProxyMode).valid());
  ASSERT_TRUE(subschema.GetProperty(key::kProxyServer).valid());
  ASSERT_TRUE(subschema.GetProperty(key::kProxyServerMode).valid());
  ASSERT_TRUE(subschema.GetProperty(key::kProxyPacUrl).valid());
  ASSERT_TRUE(subschema.GetProperty(kProxyPacMandatory).valid());
  ASSERT_TRUE(subschema.GetProperty(key::kProxyBypassList).valid());

  // The properties are iterated in order.
  const char* kExpectedProperties[] = {
      key::kProxyBypassList,
      key::kProxyMode,
      kProxyPacMandatory,
      key::kProxyPacUrl,
      key::kProxyServer,
      key::kProxyServerMode,
      nullptr,
  };
  const char** next = kExpectedProperties;
  for (Schema::Iterator it(subschema.GetPropertiesIterator());
       !it.IsAtEnd(); it.Advance(), ++next) {
    ASSERT_TRUE(*next != nullptr);
    EXPECT_STREQ(*next, it.key());
    ASSERT_TRUE(it.schema().valid());
    if (it.key() == key::kProxyServerMode)
      EXPECT_EQ(base::Value::Type::INTEGER, it.schema().type());
    else if (strcmp(it.key(), kProxyPacMandatory) == 0)
      EXPECT_EQ(base::Value::Type::BOOLEAN, it.schema().type());
    else
      EXPECT_EQ(base::Value::Type::STRING, it.schema().type());
  }
  EXPECT_TRUE(*next == nullptr);
#endif  // !BUILDFLAG(IS_IOS)

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  subschema = schema.GetProperty(key::kExtensionSettings);
  ASSERT_TRUE(subschema.valid());
  ASSERT_EQ(base::Value::Type::DICTIONARY, subschema.type());
  EXPECT_FALSE(subschema.GetAdditionalProperties().valid());
  EXPECT_FALSE(subschema.GetProperty("no such extension id exists").valid());
  EXPECT_TRUE(subschema.GetPatternProperties("*").empty());
  EXPECT_TRUE(subschema.GetPatternProperties("no such extension id").empty());
  EXPECT_TRUE(subschema.GetPatternProperties("^[a-p]{32}$").empty());
  EXPECT_TRUE(subschema.GetPatternProperties("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")
                  .empty());
  EXPECT_TRUE(
      subschema.GetPatternProperties("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")
          .empty());
  SchemaList schema_list =
      subschema.GetPatternProperties("abcdefghijklmnopabcdefghijklmnop");
  ASSERT_EQ(1u, schema_list.size());
  subschema = schema_list[0];
  ASSERT_TRUE(subschema.valid());
  ASSERT_EQ(base::Value::Type::DICTIONARY, subschema.type());
  subschema = subschema.GetProperty("installation_mode");
  ASSERT_TRUE(subschema.valid());
  ASSERT_EQ(base::Value::Type::STRING, subschema.type());

  subschema = schema.GetProperty(key::kExtensionSettings).GetProperty("*");
  ASSERT_TRUE(subschema.valid());
  ASSERT_EQ(base::Value::Type::DICTIONARY, subschema.type());
  subschema = subschema.GetProperty("installation_mode");
  ASSERT_TRUE(subschema.valid());
  ASSERT_EQ(base::Value::Type::STRING, subschema.type());
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  subschema = schema.GetKnownProperty(key::kPowerManagementIdleSettings);
  ASSERT_TRUE(subschema.valid());

  EXPECT_TRUE(IsSameSchema(subschema.GetKnownProperty("AC"),
                           subschema.GetKnownProperty("Battery")));

  subschema = schema.GetKnownProperty(key::kDeviceLoginScreenPowerManagement);
  ASSERT_TRUE(subschema.valid());

  EXPECT_TRUE(IsSameSchema(subschema.GetKnownProperty("AC"),
                           subschema.GetKnownProperty("Battery")));
#endif
}

TEST(GeneratePolicySource, PolicyDetails) {
  EXPECT_FALSE(GetChromePolicyDetails(""));
  EXPECT_FALSE(GetChromePolicyDetails("no such policy"));
  EXPECT_FALSE(GetChromePolicyDetails("SearchSuggestEnable"));
  EXPECT_FALSE(GetChromePolicyDetails("searchSuggestEnabled"));
  EXPECT_FALSE(GetChromePolicyDetails("SSearchSuggestEnabled"));

  const PolicyDetails* details =
      GetChromePolicyDetails(key::kSearchSuggestEnabled);
  ASSERT_TRUE(details);
  EXPECT_FALSE(details->is_deprecated);
  EXPECT_FALSE(details->is_device_policy);
  EXPECT_EQ(6, details->id);
  EXPECT_EQ(0u, details->max_external_data_size);

#if !BUILDFLAG(IS_IOS)
  details = GetChromePolicyDetails(key::kJavascriptEnabled);
  ASSERT_TRUE(details);
  EXPECT_TRUE(details->is_deprecated);
  EXPECT_FALSE(details->is_device_policy);
  EXPECT_EQ(9, details->id);
  EXPECT_EQ(0u, details->max_external_data_size);
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  details = GetChromePolicyDetails(key::kDevicePolicyRefreshRate);
  ASSERT_TRUE(details);
  EXPECT_FALSE(details->is_deprecated);
  EXPECT_TRUE(details->is_device_policy);
  EXPECT_EQ(90, details->id);
  EXPECT_EQ(0u, details->max_external_data_size);

  // Policies of type 'external' have a greater-than-zero value for
  // |max_external_data_size|.
  details = GetChromePolicyDetails(key::kWallpaperImage);
  ASSERT_TRUE(details);
  EXPECT_FALSE(details->is_deprecated);
  EXPECT_FALSE(details->is_device_policy);
  EXPECT_EQ(262, details->id);
  EXPECT_GT(details->max_external_data_size, 0u);
#endif
}

#if BUILDFLAG(IS_CHROMEOS)
TEST(GeneratePolicySource, SetEnterpriseDefaults) {
  PolicyMap policy_map;

  // If policy not configured yet, set the enterprise default.
  SetEnterpriseUsersDefaults(&policy_map);

  const base::Value* multiprof_behavior =
      policy_map.GetValue(key::kChromeOsMultiProfileUserBehavior);
  base::Value expected("primary-only");
  EXPECT_TRUE(expected.Equals(multiprof_behavior));

  // If policy already configured, it's not changed to enterprise defaults.
  policy_map.Set(key::kChromeOsMultiProfileUserBehavior, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                 base::Value("test_value"), nullptr);
  SetEnterpriseUsersDefaults(&policy_map);
  multiprof_behavior =
      policy_map.GetValue(key::kChromeOsMultiProfileUserBehavior);
  expected = base::Value("test_value");
  EXPECT_TRUE(expected.Equals(multiprof_behavior));
}

TEST(GeneratePolicySource, SetEnterpriseSystemWideDefaults) {
  PolicyMap policy_map;

  // If policy not configured yet, set the enterprise system-wide default.
  SetEnterpriseUsersSystemWideDefaults(&policy_map);

  const base::Value* pin_unlock_autosubmit_enabled =
      policy_map.GetValue(key::kPinUnlockAutosubmitEnabled);
  ASSERT_TRUE(pin_unlock_autosubmit_enabled);
  EXPECT_FALSE(pin_unlock_autosubmit_enabled->GetBool());
  const base::Value* allow_dinosaur_easter_egg =
      policy_map.GetValue(key::kAllowDinosaurEasterEgg);
  EXPECT_EQ(nullptr, allow_dinosaur_easter_egg);

  // If policy already configured, it's not changed to enterprise defaults.
  policy_map.Set(key::kPinUnlockAutosubmitEnabled, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(true),
                 nullptr);
  SetEnterpriseUsersSystemWideDefaults(&policy_map);
  pin_unlock_autosubmit_enabled =
      policy_map.GetValue(key::kPinUnlockAutosubmitEnabled);
  ASSERT_TRUE(pin_unlock_autosubmit_enabled);
  EXPECT_TRUE(pin_unlock_autosubmit_enabled->GetBool());
  allow_dinosaur_easter_egg = policy_map.GetValue(key::kAllowDinosaurEasterEgg);
  EXPECT_EQ(nullptr, allow_dinosaur_easter_egg);
}

TEST(GeneratePolicySource, SetEnterpriseProfileDefaults) {
  PolicyMap policy_map;

  // If policy not configured yet, set the enterprise profile default.
  SetEnterpriseUsersProfileDefaults(&policy_map);

  const base::Value* allow_dinosaur_easter_egg =
      policy_map.GetValue(key::kAllowDinosaurEasterEgg);
  ASSERT_TRUE(allow_dinosaur_easter_egg);
  EXPECT_FALSE(allow_dinosaur_easter_egg->GetBool());
  const base::Value* pin_unlock_autosubmit_enabled =
      policy_map.GetValue(key::kPinUnlockAutosubmitEnabled);
  EXPECT_EQ(nullptr, pin_unlock_autosubmit_enabled);

  // If policy already configured, it's not changed to enterprise defaults.
  policy_map.Set(key::kAllowDinosaurEasterEgg, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(true),
                 nullptr);
  SetEnterpriseUsersProfileDefaults(&policy_map);
  allow_dinosaur_easter_egg = policy_map.GetValue(key::kAllowDinosaurEasterEgg);
  ASSERT_TRUE(allow_dinosaur_easter_egg);
  EXPECT_TRUE(allow_dinosaur_easter_egg->GetBool());
  pin_unlock_autosubmit_enabled =
      policy_map.GetValue(key::kPinUnlockAutosubmitEnabled);
  EXPECT_EQ(nullptr, pin_unlock_autosubmit_enabled);
}
#endif

}  // namespace policy
