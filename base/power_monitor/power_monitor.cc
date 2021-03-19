// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/power_monitor/power_monitor.h"

#include <utility>

#include "base/logging.h"
#include "base/power_monitor/power_monitor_source.h"
#include "base/trace_event/base_tracing.h"
#include "build/build_config.h"

namespace base {

void PowerMonitor::Initialize(std::unique_ptr<PowerMonitorSource> source) {
  DCHECK(!IsInitialized());
  GetInstance()->source_ = std::move(source);
}

bool PowerMonitor::IsInitialized() {
  return GetInstance()->source_.get() != nullptr;
}

// static
void PowerMonitor::AddPowerSuspendObserver(PowerSuspendObserver* obs) {
  GetInstance()->power_suspend_observers_->AddObserver(obs);
}

// static
void PowerMonitor::RemovePowerSuspendObserver(PowerSuspendObserver* obs) {
  GetInstance()->power_suspend_observers_->RemoveObserver(obs);
}

// static
void PowerMonitor::AddPowerStateObserver(PowerStateObserver* obs) {
  GetInstance()->power_state_observers_->AddObserver(obs);
}

// static
void PowerMonitor::RemovePowerStateObserver(PowerStateObserver* obs) {
  GetInstance()->power_state_observers_->RemoveObserver(obs);
}

// static
void PowerMonitor::AddPowerThermalObserver(PowerThermalObserver* obs) {
  GetInstance()->thermal_state_observers_->AddObserver(obs);
}

// static
void PowerMonitor::RemovePowerThermalObserver(PowerThermalObserver* obs) {
  GetInstance()->thermal_state_observers_->RemoveObserver(obs);
}

// static
bool PowerMonitor::AddPowerSuspendObserverAndReturnSuspendedState(
    PowerSuspendObserver* obs) {
  PowerMonitor* power_monitor = GetInstance();
  AutoLock auto_lock(power_monitor->is_system_suspended_lock_);
  power_monitor->power_suspend_observers_->AddObserver(obs);
  return power_monitor->is_system_suspended_;
}

PowerMonitorSource* PowerMonitor::Source() {
  return GetInstance()->source_.get();
}

bool PowerMonitor::IsOnBatteryPower() {
  DCHECK(IsInitialized());
  return GetInstance()->source_->IsOnBatteryPower();
}

void PowerMonitor::ShutdownForTesting() {
  GetInstance()->source_ = nullptr;

  PowerMonitor* power_monitor = GetInstance();
  {
    AutoLock auto_lock(power_monitor->is_system_suspended_lock_);
    power_monitor->is_system_suspended_ = false;
  }
}

// static
PowerThermalObserver::DeviceThermalState
PowerMonitor::GetCurrentThermalState() {
  DCHECK(IsInitialized());
  return GetInstance()->source_->GetCurrentThermalState();
}

// static
void PowerMonitor::SetCurrentThermalState(
    PowerThermalObserver::DeviceThermalState state) {
  DCHECK(IsInitialized());
  GetInstance()->source_->SetCurrentThermalState(state);
}

#if defined(OS_ANDROID)
int PowerMonitor::GetRemainingBatteryCapacity() {
  DCHECK(IsInitialized());
  return GetInstance()->source_->GetRemainingBatteryCapacity();
}
#endif  // defined(OS_ANDROID)

void PowerMonitor::NotifyPowerStateChange(bool battery_in_use) {
  DCHECK(IsInitialized());
  DVLOG(1) << "PowerStateChange: " << (battery_in_use ? "On" : "Off")
           << " battery";
  GetInstance()->power_state_observers_->Notify(
      FROM_HERE, &PowerStateObserver::OnPowerStateChange, battery_in_use);
}

void PowerMonitor::NotifySuspend() {
  DCHECK(IsInitialized());
  TRACE_EVENT_INSTANT0("base", "PowerMonitor::NotifySuspend",
                       TRACE_EVENT_SCOPE_PROCESS);
  DVLOG(1) << "Power Suspending";

  PowerMonitor* power_monitor = GetInstance();
  AutoLock auto_lock(power_monitor->is_system_suspended_lock_);
  if (!power_monitor->is_system_suspended_) {
    power_monitor->is_system_suspended_ = true;
    GetInstance()->power_suspend_observers_->Notify(
        FROM_HERE, &PowerSuspendObserver::OnSuspend);
  }
}

void PowerMonitor::NotifyResume() {
  DCHECK(IsInitialized());
  TRACE_EVENT_INSTANT0("base", "PowerMonitor::NotifyResume",
                       TRACE_EVENT_SCOPE_PROCESS);
  DVLOG(1) << "Power Resuming";

  PowerMonitor* power_monitor = GetInstance();
  AutoLock auto_lock(power_monitor->is_system_suspended_lock_);
  if (power_monitor->is_system_suspended_) {
    power_monitor->is_system_suspended_ = false;
    GetInstance()->power_suspend_observers_->Notify(
        FROM_HERE, &PowerSuspendObserver::OnResume);
  }
}

void PowerMonitor::NotifyThermalStateChange(
    PowerThermalObserver::DeviceThermalState new_state) {
  DCHECK(IsInitialized());
  DVLOG(1) << "ThermalStateChange: "
           << PowerMonitorSource::DeviceThermalStateToString(new_state);
  GetInstance()->thermal_state_observers_->Notify(
      FROM_HERE, &PowerThermalObserver::OnThermalStateChange, new_state);
}

PowerMonitor* PowerMonitor::GetInstance() {
  static base::NoDestructor<PowerMonitor> power_monitor;
  return power_monitor.get();
}

PowerMonitor::PowerMonitor()
    : power_state_observers_(
          base::MakeRefCounted<ObserverListThreadSafe<PowerStateObserver>>()),
      power_suspend_observers_(
          base::MakeRefCounted<ObserverListThreadSafe<PowerSuspendObserver>>()),
      thermal_state_observers_(
          base::MakeRefCounted<
              ObserverListThreadSafe<PowerThermalObserver>>()) {}

PowerMonitor::~PowerMonitor() = default;

}  // namespace base
