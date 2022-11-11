#ifndef PERFETTO_PROTOS_PROTOS_PERFETTO_TRACE_PROFILING_PROFILE_COMMON_PROTO_H_
#define PERFETTO_PROTOS_PROTOS_PERFETTO_TRACE_PROFILING_PROFILE_COMMON_PROTO_H_

namespace perfetto {
namespace protos {
namespace pbzero {

class Mapping : public ::protozero::Message {
 public:
  void set_iid(uint64_t value) {}
  void set_build_id(uint64_t value) {}
  void add_path_string_ids(uint64_t value) {}
};

class InternedString : public ::protozero::Message {
 public:
  void set_iid(uint64_t value) {}
  void set_str(std::string value) {}
};

}
}
}

#endif // PERFETTO_PROTOS_PROTOS_PERFETTO_TRACE_PROFILING_PROFILE_COMMON_PROTO_H_
