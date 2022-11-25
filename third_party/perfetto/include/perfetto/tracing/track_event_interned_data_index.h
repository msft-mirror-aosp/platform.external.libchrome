// Copyright 2022 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef THIRD_PARTY_PERFETTO_INCLUDE_PERFETTO_TRACING_TRACK_EVENT_INTERNED_DATA_INDEX_H_
#define THIRD_PARTY_PERFETTO_INCLUDE_PERFETTO_TRACING_TRACK_EVENT_INTERNED_DATA_INDEX_H_

namespace perfetto {

class EventContext;

struct SmallInternedDataTraits {
  template <typename ValueType>
  class Index {
   public:
    bool LookUpOrInsert(size_t* iid, const ValueType& value) {
      return false;
    }
  };
};

struct BigInternedDataTraits {
  template <typename ValueType>
  class Index {
   public:
    bool LookUpOrInsert(size_t* iid, const ValueType& value) {
      return false;
    }
  };
};

template <typename InternedDataType, size_t FieldNumber, typename ValueType,
           typename Traits =
              typename std::conditional<(std::is_pointer<ValueType>::value),
                                        SmallInternedDataTraits,
                                        BigInternedDataTraits>::type>
class TrackEventInternedDataIndex {
public:
  template <typename... Args>
  static size_t Get(EventContext *, const ValueType &value,
                    Args &&...add_args) {
    return 0;
  }

protected:
  static InternedDataType* GetOrCreateIndexForField(
      internal::TrackEventIncrementalState* incremental_state) {
    return nullptr;
  }

  typename Traits::template Index<ValueType> index_;
};

} // namespace perfetto

#endif
