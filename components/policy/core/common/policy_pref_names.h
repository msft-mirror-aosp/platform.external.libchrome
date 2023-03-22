// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_POLICY_PREF_NAMES_H_
#define COMPONENTS_POLICY_CORE_COMMON_POLICY_PREF_NAMES_H_

#include "build/build_config.h"
#include "components/policy/policy_export.h"

namespace policy {
namespace policy_prefs {

#if BUILDFLAG(IS_WIN)
extern const char kAzureActiveDirectoryManagement[];
extern const char kEnterpriseMDMManagementWindows[];
#endif
extern const char kCloudManagementEnrollmentMandatory[];
extern const char kDlpClipboardCheckSizeLimit[];
extern const char kDlpReportingEnabled[];
extern const char kDlpRulesList[];
#if BUILDFLAG(IS_MAC)
extern const char kEnterpriseMDMManagementMac[];
extern const char kScreenTimeEnabled[];
#endif
extern const char kLastPolicyStatisticsUpdate[];
extern const char kNativeWindowOcclusionEnabled[];
extern const char kSafeSitesFilterBehavior[];
extern const char kSystemFeaturesDisableList[];
extern const char kSystemFeaturesDisableMode[];
extern const char kUrlBlocklist[];
extern const char kUrlAllowlist[];
extern const char kUserPolicyRefreshRate[];
extern const char kIntensiveWakeUpThrottlingEnabled[];
extern const char kUserAgentClientHintsGREASEUpdateEnabled[];
#if BUILDFLAG(IS_ANDROID)
extern const char kBackForwardCacheEnabled[];
#endif  // BUILDFLAG(IS_ANDROID)
extern const char kIsolatedAppsDeveloperModeAllowed[];
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
extern const char kLastPolicyCheckTime[];
#endif
#if BUILDFLAG(IS_IOS)
extern const char kUserPolicyNotificationWasShown[];
#endif
extern const char kEventPathEnabled[];
extern const char kOffsetParentNewSpecBehaviorEnabled[];
extern const char kSendMouseEventsDisabledFormControlsEnabled[];
extern const char kUseMojoVideoDecoderForPepperAllowed[];
extern const char kPPAPISharedImagesSwapChainAllowed[];
extern const char kForceEnablePepperVideoDecoderDevAPI[];
extern const char kForceGoogleSafeSearch[];
extern const char kForceYouTubeRestrict[];
extern const char kHideWebStoreIcon[];
extern const char kIncognitoModeAvailability[];

}  // namespace policy_prefs
}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_POLICY_PREF_NAMES_H_
