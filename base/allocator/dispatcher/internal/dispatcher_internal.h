// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_DISPATCHER_INTERNAL_DISPATCHER_INTERNAL_H_
#define BASE_ALLOCATOR_DISPATCHER_INTERNAL_DISPATCHER_INTERNAL_H_

#include "base/allocator/dispatcher/configuration.h"
#include "base/allocator/dispatcher/internal/dispatch_data.h"
#include "base/allocator/dispatcher/internal/tools.h"
#include "base/allocator/dispatcher/memory_tagging.h"
#include "base/allocator/dispatcher/notification_data.h"
#include "base/allocator/dispatcher/subsystem.h"
#include "base/check.h"
#include "base/compiler_specific.h"
#include "partition_alloc/partition_alloc_buildflags.h"

#if BUILDFLAG(USE_PARTITION_ALLOC)
#include "partition_alloc/partition_alloc_allocation_data.h"
#endif

#if BUILDFLAG(USE_ALLOCATOR_SHIM)
#include "partition_alloc/shim/allocator_shim.h"
#endif

#include <tuple>

namespace base::allocator::dispatcher::internal {

#if BUILDFLAG(USE_ALLOCATOR_SHIM)
using allocator_shim::AllocatorDispatch;
#endif

template <typename CheckObserverPredicate,
          typename... ObserverTypes,
          size_t... Indices>
void inline PerformObserverCheck(const std::tuple<ObserverTypes...>& observers,
                                 std::index_sequence<Indices...>,
                                 CheckObserverPredicate check_observer) {
  ([](bool b) { DCHECK(b); }(check_observer(std::get<Indices>(observers))),
   ...);
}

template <typename... ObserverTypes, size_t... Indices>
ALWAYS_INLINE void PerformAllocationNotification(
    const std::tuple<ObserverTypes...>& observers,
    std::index_sequence<Indices...>,
    const AllocationNotificationData& notification_data) {
  ((std::get<Indices>(observers)->OnAllocation(notification_data)), ...);
}

template <typename... ObserverTypes, size_t... Indices>
ALWAYS_INLINE void PerformFreeNotification(
    const std::tuple<ObserverTypes...>& observers,
    std::index_sequence<Indices...>,
    const FreeNotificationData& notification_data) {
  ((std::get<Indices>(observers)->OnFree(notification_data)), ...);
}

// DispatcherImpl provides hooks into the various memory subsystems. These hooks
// are responsible for dispatching any notification to the observers.
// In order to provide as many information on the exact type of the observer and
// prevent any conditional jumps in the hot allocation path, observers are
// stored in a std::tuple. DispatcherImpl performs a CHECK at initialization
// time to ensure they are valid.
template <typename... ObserverTypes>
struct DispatcherImpl {
  using AllObservers = std::index_sequence_for<ObserverTypes...>;

  template <std::enable_if_t<
                internal::LessEqual(sizeof...(ObserverTypes),
                                    configuration::kMaximumNumberOfObservers),
                bool> = true>
  static DispatchData GetNotificationHooks(
      std::tuple<ObserverTypes*...> observers) {
    s_observers = std::move(observers);

    PerformObserverCheck(s_observers, AllObservers{}, IsValidObserver{});

    return CreateDispatchData();
  }

 private:
  static DispatchData CreateDispatchData() {
    return DispatchData()
#if BUILDFLAG(USE_PARTITION_ALLOC)
        .SetAllocationObserverHooks(&PartitionAllocatorAllocationHook,
                                    &PartitionAllocatorFreeHook)
#endif
#if BUILDFLAG(USE_ALLOCATOR_SHIM)
        .SetAllocatorDispatch(&allocator_dispatch_)
#endif
        ;
  }

#if BUILDFLAG(USE_PARTITION_ALLOC)
  static void PartitionAllocatorAllocationHook(
      const partition_alloc::AllocationNotificationData& pa_notification_data) {
    AllocationNotificationData dispatcher_notification_data(
        pa_notification_data.address(), pa_notification_data.size(),
        pa_notification_data.type_name(),
        AllocationSubsystem::kPartitionAllocator);

#if BUILDFLAG(HAS_MEMORY_TAGGING)
    dispatcher_notification_data.SetMteReportingMode(
        ConvertToMTEMode(pa_notification_data.mte_reporting_mode()));
#endif

    DoNotifyAllocation(dispatcher_notification_data);
  }

  static void PartitionAllocatorFreeHook(
      const partition_alloc::FreeNotificationData& pa_notification_data) {
    FreeNotificationData dispatcher_notification_data(
        pa_notification_data.address(),
        AllocationSubsystem::kPartitionAllocator);

#if BUILDFLAG(HAS_MEMORY_TAGGING)
    dispatcher_notification_data.SetMteReportingMode(
        ConvertToMTEMode(pa_notification_data.mte_reporting_mode()));
#endif

    DoNotifyFree(dispatcher_notification_data);
  }
#endif  // BUILDFLAG(USE_PARTITION_ALLOC)

#if BUILDFLAG(USE_ALLOCATOR_SHIM)
  static void* AllocFn(const AllocatorDispatch* self,
                       size_t size,
                       void* context) {
    void* const address = self->next->alloc_function(self->next, size, context);

    DoNotifyAllocationForShim(address, size);

    return address;
  }

  static void* AllocUncheckedFn(const AllocatorDispatch* self,
                                size_t size,
                                void* context) {
    void* const address =
        self->next->alloc_unchecked_function(self->next, size, context);

    DoNotifyAllocationForShim(address, size);

    return address;
  }

  static void* AllocZeroInitializedFn(const AllocatorDispatch* self,
                                      size_t n,
                                      size_t size,
                                      void* context) {
    void* const address = self->next->alloc_zero_initialized_function(
        self->next, n, size, context);

    DoNotifyAllocationForShim(address, n * size);

    return address;
  }

  static void* AllocAlignedFn(const AllocatorDispatch* self,
                              size_t alignment,
                              size_t size,
                              void* context) {
    void* const address = self->next->alloc_aligned_function(
        self->next, alignment, size, context);

    DoNotifyAllocationForShim(address, size);

    return address;
  }

  static void* ReallocFn(const AllocatorDispatch* self,
                         void* address,
                         size_t size,
                         void* context) {
    // Note: size == 0 actually performs free.
    DoNotifyFreeForShim(address);
    void* const reallocated_address =
        self->next->realloc_function(self->next, address, size, context);

    DoNotifyAllocationForShim(reallocated_address, size);

    return reallocated_address;
  }

  static void FreeFn(const AllocatorDispatch* self,
                     void* address,
                     void* context) {
    // Note: DoNotifyFree should be called before free_function (here and in
    // other places). That is because observers need to handle the allocation
    // being freed before calling free_function, as once the latter is executed
    // the address becomes available and can be allocated by another thread.
    // That would be racy otherwise.
    DoNotifyFreeForShim(address);
    self->next->free_function(self->next, address, context);
  }

  static size_t GetSizeEstimateFn(const AllocatorDispatch* self,
                                  void* address,
                                  void* context) {
    return self->next->get_size_estimate_function(self->next, address, context);
  }

  static size_t GoodSizeFn(const AllocatorDispatch* self,
                           size_t size,
                           void* context) {
    return self->next->good_size_function(self->next, size, context);
  }

  static bool ClaimedAddressFn(const AllocatorDispatch* self,
                               void* address,
                               void* context) {
    return self->next->claimed_address_function(self->next, address, context);
  }

  static unsigned BatchMallocFn(const AllocatorDispatch* self,
                                size_t size,
                                void** results,
                                unsigned num_requested,
                                void* context) {
    unsigned const num_allocated = self->next->batch_malloc_function(
        self->next, size, results, num_requested, context);
    for (unsigned i = 0; i < num_allocated; ++i) {
      DoNotifyAllocationForShim(results[i], size);
    }
    return num_allocated;
  }

  static void BatchFreeFn(const AllocatorDispatch* self,
                          void** to_be_freed,
                          unsigned num_to_be_freed,
                          void* context) {
    for (unsigned i = 0; i < num_to_be_freed; ++i) {
      DoNotifyFreeForShim(to_be_freed[i]);
    }

    self->next->batch_free_function(self->next, to_be_freed, num_to_be_freed,
                                    context);
  }

  static void FreeDefiniteSizeFn(const AllocatorDispatch* self,
                                 void* address,
                                 size_t size,
                                 void* context) {
    DoNotifyFreeForShim(address);
    self->next->free_definite_size_function(self->next, address, size, context);
  }

  static void TryFreeDefaultFn(const AllocatorDispatch* self,
                               void* address,
                               void* context) {
    DoNotifyFreeForShim(address);
    self->next->try_free_default_function(self->next, address, context);
  }

  static void* AlignedMallocFn(const AllocatorDispatch* self,
                               size_t size,
                               size_t alignment,
                               void* context) {
    void* const address = self->next->aligned_malloc_function(
        self->next, size, alignment, context);

    DoNotifyAllocationForShim(address, size);

    return address;
  }

  static void* AlignedReallocFn(const AllocatorDispatch* self,
                                void* address,
                                size_t size,
                                size_t alignment,
                                void* context) {
    // Note: size == 0 actually performs free.
    DoNotifyFreeForShim(address);
    address = self->next->aligned_realloc_function(self->next, address, size,
                                                   alignment, context);

    DoNotifyAllocationForShim(address, size);

    return address;
  }

  static void AlignedFreeFn(const AllocatorDispatch* self,
                            void* address,
                            void* context) {
    DoNotifyFreeForShim(address);
    self->next->aligned_free_function(self->next, address, context);
  }

  ALWAYS_INLINE static void DoNotifyAllocationForShim(void* address,
                                                      size_t size) {
    AllocationNotificationData notification_data(
        address, size, nullptr, AllocationSubsystem::kAllocatorShim);

    DoNotifyAllocation(notification_data);
  }

  ALWAYS_INLINE static void DoNotifyFreeForShim(void* address) {
    FreeNotificationData notification_data(address,
                                           AllocationSubsystem::kAllocatorShim);

    DoNotifyFree(notification_data);
  }

  static AllocatorDispatch allocator_dispatch_;
#endif  // BUILDFLAG(USE_ALLOCATOR_SHIM)

  ALWAYS_INLINE static void DoNotifyAllocation(
      const AllocationNotificationData& notification_data) {
    PerformAllocationNotification(s_observers, AllObservers{},
                                  notification_data);
  }

  ALWAYS_INLINE static void DoNotifyFree(
      const FreeNotificationData& notification_data) {
    PerformFreeNotification(s_observers, AllObservers{}, notification_data);
  }

  static std::tuple<ObserverTypes*...> s_observers;
};

template <typename... ObserverTypes>
std::tuple<ObserverTypes*...> DispatcherImpl<ObserverTypes...>::s_observers;

#if BUILDFLAG(USE_ALLOCATOR_SHIM)
template <typename... ObserverTypes>
AllocatorDispatch DispatcherImpl<ObserverTypes...>::allocator_dispatch_ = {
    &AllocFn,
    &AllocUncheckedFn,
    &AllocZeroInitializedFn,
    &AllocAlignedFn,
    &ReallocFn,
    &FreeFn,
    &GetSizeEstimateFn,
    &GoodSizeFn,
    &ClaimedAddressFn,
    &BatchMallocFn,
    &BatchFreeFn,
    &FreeDefiniteSizeFn,
    &TryFreeDefaultFn,
    &AlignedMallocFn,
    &AlignedReallocFn,
    &AlignedFreeFn,
    nullptr};
#endif  // BUILDFLAG(USE_ALLOCATOR_SHIM)

// Specialization of DispatcherImpl in case we have no observers to notify. In
// this special case we return a set of null pointers as the Dispatcher must not
// install any hooks at all.
template <>
struct DispatcherImpl<> {
  static DispatchData GetNotificationHooks(std::tuple<> /*observers*/) {
    return DispatchData()
#if BUILDFLAG(USE_PARTITION_ALLOC)
        .SetAllocationObserverHooks(nullptr, nullptr)
#endif
#if BUILDFLAG(USE_ALLOCATOR_SHIM)
        .SetAllocatorDispatch(nullptr)
#endif
        ;
  }
};

// A little utility function that helps using DispatcherImpl by providing
// automated type deduction for templates.
template <typename... ObserverTypes>
inline DispatchData GetNotificationHooks(
    std::tuple<ObserverTypes*...> observers) {
  return DispatcherImpl<ObserverTypes...>::GetNotificationHooks(
      std::move(observers));
}

}  // namespace base::allocator::dispatcher::internal

#endif  // BASE_ALLOCATOR_DISPATCHER_INTERNAL_DISPATCHER_INTERNAL_H_
