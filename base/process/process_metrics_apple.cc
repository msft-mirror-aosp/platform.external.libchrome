// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process_metrics.h"

#include <AvailabilityMacros.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/sysctl.h>

#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/mac/mach_logging.h"
#include "base/mac/scoped_mach_port.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_math.h"
#include "base/time/time.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_MAC)
#include <libproc.h>
#include <mach/mach_vm.h>
#include <mach/shared_region.h>
#else
#include <mach/vm_region.h>
#if BUILDFLAG(USE_BLINK)
#include "base/ios/sim_header_shims.h"
#endif  // BUILDFLAG(USE_BLINK)
#endif

namespace base {

#define TIME_VALUE_TO_TIMEVAL(a, r)   \
  do {                                \
    (r)->tv_sec = (a)->seconds;       \
    (r)->tv_usec = (a)->microseconds; \
  } while (0)

namespace {

bool GetTaskInfo(mach_port_t task, task_basic_info_64* task_info_data) {
  if (task == MACH_PORT_NULL) {
    return false;
  }
  mach_msg_type_number_t count = TASK_BASIC_INFO_64_COUNT;
  kern_return_t kr =
      task_info(task, TASK_BASIC_INFO_64,
                reinterpret_cast<task_info_t>(task_info_data), &count);
  // Most likely cause for failure: |task| is a zombie.
  return kr == KERN_SUCCESS;
}

MachVMRegionResult ParseOutputFromMachVMRegion(kern_return_t kr) {
  if (kr == KERN_INVALID_ADDRESS) {
    // We're at the end of the address space.
    return MachVMRegionResult::Finished;
  } else if (kr != KERN_SUCCESS) {
    return MachVMRegionResult::Error;
  }
  return MachVMRegionResult::Success;
}

bool GetPowerInfo(mach_port_t task, task_power_info* power_info_data) {
  if (task == MACH_PORT_NULL) {
    return false;
  }

  mach_msg_type_number_t power_info_count = TASK_POWER_INFO_COUNT;
  kern_return_t kr = task_info(task, TASK_POWER_INFO,
                               reinterpret_cast<task_info_t>(power_info_data),
                               &power_info_count);
  // Most likely cause for failure: |task| is a zombie.
  return kr == KERN_SUCCESS;
}

}  // namespace

// Implementations of ProcessMetrics class shared by Mac and iOS.
mach_port_t ProcessMetrics::TaskForPid(ProcessHandle process) const {
  mach_port_t task = MACH_PORT_NULL;
#if BUILDFLAG(IS_MAC)
  if (port_provider_) {
    task = port_provider_->TaskForPid(process_);
  }
#endif
  if (task == MACH_PORT_NULL && process_ == getpid()) {
    task = mach_task_self();
  }
  return task;
}

TimeDelta ProcessMetrics::GetCumulativeCPUUsage() {
  mach_port_t task = TaskForPid(process_);
  if (task == MACH_PORT_NULL) {
    return TimeDelta();
  }

  // Libtop explicitly loops over the threads (libtop_pinfo_update_cpu_usage()
  // in libtop.c), but this is more concise and gives the same results:
  task_thread_times_info thread_info_data;
  mach_msg_type_number_t thread_info_count = TASK_THREAD_TIMES_INFO_COUNT;
  kern_return_t kr = task_info(task, TASK_THREAD_TIMES_INFO,
                               reinterpret_cast<task_info_t>(&thread_info_data),
                               &thread_info_count);
  if (kr != KERN_SUCCESS) {
    // Most likely cause: |task| is a zombie.
    return TimeDelta();
  }

  task_basic_info_64 task_info_data;
  if (!GetTaskInfo(task, &task_info_data)) {
    return TimeDelta();
  }

  /* Set total_time. */
  // thread info contains live time...
  struct timeval user_timeval, system_timeval, task_timeval;
  TIME_VALUE_TO_TIMEVAL(&thread_info_data.user_time, &user_timeval);
  TIME_VALUE_TO_TIMEVAL(&thread_info_data.system_time, &system_timeval);
  timeradd(&user_timeval, &system_timeval, &task_timeval);

  // ... task info contains terminated time.
  TIME_VALUE_TO_TIMEVAL(&task_info_data.user_time, &user_timeval);
  TIME_VALUE_TO_TIMEVAL(&task_info_data.system_time, &system_timeval);
  timeradd(&user_timeval, &task_timeval, &task_timeval);
  timeradd(&system_timeval, &task_timeval, &task_timeval);

  return Microseconds(TimeValToMicroseconds(task_timeval));
}

int ProcessMetrics::GetPackageIdleWakeupsPerSecond() {
  mach_port_t task = TaskForPid(process_);
  task_power_info power_info_data;

  GetPowerInfo(task, &power_info_data);

  // The task_power_info struct contains two wakeup counters:
  // task_interrupt_wakeups and task_platform_idle_wakeups.
  // task_interrupt_wakeups is the total number of wakeups generated by the
  // process, and is the number that Activity Monitor reports.
  // task_platform_idle_wakeups is a subset of task_interrupt_wakeups that
  // tallies the number of times the processor was taken out of its low-power
  // idle state to handle a wakeup. task_platform_idle_wakeups therefore result
  // in a greater power increase than the other interrupts which occur while the
  // CPU is already working, and reducing them has a greater overall impact on
  // power usage. See the powermetrics man page for more info.
  return CalculatePackageIdleWakeupsPerSecond(
      power_info_data.task_platform_idle_wakeups);
}

int ProcessMetrics::GetIdleWakeupsPerSecond() {
  mach_port_t task = TaskForPid(process_);
  task_power_info power_info_data;

  GetPowerInfo(task, &power_info_data);

  return CalculateIdleWakeupsPerSecond(power_info_data.task_interrupt_wakeups);
}

// Bytes committed by the system.
size_t GetSystemCommitCharge() {
  base::mac::ScopedMachSendRight host(mach_host_self());
  mach_msg_type_number_t count = HOST_VM_INFO_COUNT;
  vm_statistics_data_t data;
  kern_return_t kr = host_statistics(
      host.get(), HOST_VM_INFO, reinterpret_cast<host_info_t>(&data), &count);
  if (kr != KERN_SUCCESS) {
    MACH_DLOG(WARNING, kr) << "host_statistics";
    return 0;
  }

  return (data.active_count * PAGE_SIZE) / 1024;
}

bool GetSystemMemoryInfo(SystemMemoryInfoKB* meminfo) {
  struct host_basic_info hostinfo;
  mach_msg_type_number_t count = HOST_BASIC_INFO_COUNT;
  base::mac::ScopedMachSendRight host(mach_host_self());
  int result = host_info(host.get(), HOST_BASIC_INFO,
                         reinterpret_cast<host_info_t>(&hostinfo), &count);
  if (result != KERN_SUCCESS) {
    return false;
  }

  DCHECK_EQ(HOST_BASIC_INFO_COUNT, count);
  meminfo->total = static_cast<int>(hostinfo.max_mem / 1024);

  vm_statistics64_data_t vm_info;
  count = HOST_VM_INFO64_COUNT;

  if (host_statistics64(host.get(), HOST_VM_INFO64,
                        reinterpret_cast<host_info64_t>(&vm_info),
                        &count) != KERN_SUCCESS) {
    return false;
  }
  DCHECK_EQ(HOST_VM_INFO64_COUNT, count);

#if defined(ARCH_CPU_ARM64) || \
    MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_16
  // PAGE_SIZE is vm_page_size on arm or for deployment targets >= 10.16,
  // and vm_page_size isn't constexpr.
  DCHECK_EQ(PAGE_SIZE % 1024, 0u) << "Invalid page size";
#else
  static_assert(PAGE_SIZE % 1024 == 0, "Invalid page size");
#endif
  meminfo->free = saturated_cast<int>(
      PAGE_SIZE / 1024 * (vm_info.free_count - vm_info.speculative_count));
  meminfo->speculative =
      saturated_cast<int>(PAGE_SIZE / 1024 * vm_info.speculative_count);
  meminfo->file_backed =
      saturated_cast<int>(PAGE_SIZE / 1024 * vm_info.external_page_count);
  meminfo->purgeable =
      saturated_cast<int>(PAGE_SIZE / 1024 * vm_info.purgeable_count);

  return true;
}

// Both |size| and |address| are in-out parameters.
// |info| is an output parameter, only valid on Success.
MachVMRegionResult GetTopInfo(mach_port_t task,
                              mach_vm_size_t* size,
                              mach_vm_address_t* address,
                              vm_region_top_info_data_t* info) {
  mach_msg_type_number_t info_count = VM_REGION_TOP_INFO_COUNT;
  // The kernel always returns a null object for VM_REGION_TOP_INFO, but
  // balance it with a deallocate in case this ever changes. See 10.9.2
  // xnu-2422.90.20/osfmk/vm/vm_map.c vm_map_region.
  mac::ScopedMachSendRight object_name;

  kern_return_t kr =
#if BUILDFLAG(IS_MAC)
      mach_vm_region(task, address, size, VM_REGION_TOP_INFO,
                     reinterpret_cast<vm_region_info_t>(info), &info_count,
                     mac::ScopedMachSendRight::Receiver(object_name).get());
#else
      vm_region_64(task, reinterpret_cast<vm_address_t*>(address),
                   reinterpret_cast<vm_size_t*>(size), VM_REGION_BASIC_INFO_64,
                   reinterpret_cast<vm_region_info_t>(info), &info_count,
                   mac::ScopedMachSendRight::Receiver(object_name).get());
#endif
  return ParseOutputFromMachVMRegion(kr);
}

MachVMRegionResult GetBasicInfo(mach_port_t task,
                                mach_vm_size_t* size,
                                mach_vm_address_t* address,
                                vm_region_basic_info_64* info) {
  mach_msg_type_number_t info_count = VM_REGION_BASIC_INFO_COUNT_64;
  // The kernel always returns a null object for VM_REGION_BASIC_INFO_64, but
  // balance it with a deallocate in case this ever changes. See 10.9.2
  // xnu-2422.90.20/osfmk/vm/vm_map.c vm_map_region.
  mac::ScopedMachSendRight object_name;

  kern_return_t kr =
#if BUILDFLAG(IS_MAC)
      mach_vm_region(task, address, size, VM_REGION_BASIC_INFO_64,
                     reinterpret_cast<vm_region_info_t>(info), &info_count,
                     mac::ScopedMachSendRight::Receiver(object_name).get());

#else
      vm_region_64(task, reinterpret_cast<vm_address_t*>(address),
                   reinterpret_cast<vm_size_t*>(size), VM_REGION_BASIC_INFO_64,
                   reinterpret_cast<vm_region_info_t>(info), &info_count,
                   mac::ScopedMachSendRight::Receiver(object_name).get());
#endif
  return ParseOutputFromMachVMRegion(kr);
}

int ProcessMetrics::GetOpenFdSoftLimit() const {
  return checked_cast<int>(GetMaxFds());
}

}  // namespace base
