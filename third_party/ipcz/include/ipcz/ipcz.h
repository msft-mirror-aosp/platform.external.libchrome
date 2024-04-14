#ifndef IPCZ_INCLUDE_IPCZ_IPCZ_H_
#define IPCZ_INCLUDE_IPCZ_IPCZ_H_

#include <stddef.h>
#include <stdint.h>

#define IPCZ_NO_FLAGS ((uint32_t)0)

#define IPCZ_FLAG_BIT(bit) ((uint32_t)(1u << bit))

typedef uintptr_t IpczHandle;

#define IPCZ_INVALID_HANDLE ((IpczHandle)0)

typedef int IpczResult;

#define IPCZ_RESULT_OK ((IpczResult)0)
#define IPCZ_RESULT_CANCELLED ((IpczResult)1)
#define IPCZ_RESULT_UNKNOWN ((IpczResult)2)
#define IPCZ_RESULT_INVALID_ARGUMENT ((IpczResult)3)
#define IPCZ_RESULT_DEADLINE_EXCEEDED ((IpczResult)4)
#define IPCZ_RESULT_NOT_FOUND ((IpczResult)5)
#define IPCZ_RESULT_ALREADY_EXISTS ((IpczResult)6)
#define IPCZ_RESULT_PERMISSION_DENIED ((IpczResult)7)
#define IPCZ_RESULT_RESOURCE_EXHAUSTED ((IpczResult)8)
#define IPCZ_RESULT_FAILED_PRECONDITION ((IpczResult)9)
#define IPCZ_RESULT_ABORTED ((IpczResult)10)
#define IPCZ_RESULT_OUT_OF_RANGE ((IpczResult)11)
#define IPCZ_RESULT_UNIMPLEMENTED ((IpczResult)12)
#define IPCZ_RESULT_INTERNAL ((IpczResult)13)
#define IPCZ_RESULT_UNAVAILABLE ((IpczResult)14)
#define IPCZ_RESULT_DATA_LOSS ((IpczResult)15)


// Helper to specify explicit struct alignment across C and C++ compilers.
#if defined(__cplusplus)
#define IPCZ_ALIGN(alignment) alignas(alignment)
#elif defined(__GNUC__)
#define IPCZ_ALIGN(alignment) __attribute__((aligned(alignment)))
#elif defined(_MSC_VER)
#define IPCZ_ALIGN(alignment) __declspec(align(alignment))
#else
#error "IPCZ_ALIGN() is not defined for your compiler."
#endif

#if defined(IPCZ_API_OVERRIDE)
#define IPCZ_API IPCZ_API_OVERRIDE
#elif defined(_WIN32)
#define IPCZ_API __cdecl
#else
#define IPCZ_API
#endif

typedef uintptr_t IpczDriverHandle;

#define IPCZ_INVALID_DRIVER_HANDLE ((IpczDriverHandle)0)

typedef uintptr_t IpczTransaction;

typedef uint32_t IpczTransportActivityFlags;

#define IPCZ_TRANSPORT_ACTIVITY_ERROR IPCZ_FLAG_BIT(0)
#define IPCZ_TRANSPORT_ACTIVITY_DEACTIVATED IPCZ_FLAG_BIT(1)

typedef uint32_t IpczMemoryFlags;

#define IPCZ_MEMORY_FIXED_PARCEL_CAPACITY (1 << 0)

// Feature identifiers which may be passed through IpczCreateNodeOptions to
// control dynamic runtime features.
typedef uint32_t IpczFeature;

// When this feature is enabled, ipcz will use alternative shared memory layout
// and allocation behavior intended to be more efficient than the v1 scheme.
#define IPCZ_FEATURE_MEM_V2 ((IpczFeature)0xA110C002)

// Options given to CreateNode() to configure the new node's behavior.
struct IPCZ_ALIGN(8) IpczCreateNodeOptions {
  // The exact size of this structure in bytes. Must be set accurately before
  // passing the structure to CreateNode().
  size_t size;

  // If set to true, this node will not attempt to allocate parcel data storage
  // within shared memory.
  bool disable_parcel_memory_expansion;

  IpczMemoryFlags memory_flags;

  // List of features to enable for this node.
  const IpczFeature* enabled_features;
  size_t num_enabled_features;

  // List of features to disable for this node. Note that if a feature is listed
  // both in `enabled_features` and `disabled_features`, it is disabled.
  const IpczFeature* disabled_features;
  size_t num_disabled_features;
};


typedef uint32_t IpczCreateNodeFlags;

#define IPCZ_CREATE_NODE_AS_BROKER IPCZ_FLAG_BIT(0)

typedef uint32_t IpczConnectNodeFlags;

#define IPCZ_CONNECT_NODE_TO_BROKER IPCZ_FLAG_BIT(0)
#define IPCZ_CONNECT_NODE_INHERIT_BROKER IPCZ_FLAG_BIT(1)
#define IPCZ_CONNECT_NODE_SHARE_BROKER IPCZ_FLAG_BIT(2)
#define IPCZ_CONNECT_NODE_TO_ALLOCATION_DELEGATE IPCZ_FLAG_BIT(3)

typedef uint32_t IpczTrapConditionFlags;

#define IPCZ_TRAP_REMOVED IPCZ_FLAG_BIT(0)
#define IPCZ_TRAP_PEER_CLOSED IPCZ_FLAG_BIT(1)
#define IPCZ_TRAP_DEAD IPCZ_FLAG_BIT(2)
#define IPCZ_TRAP_ABOVE_MIN_LOCAL_PARCELS IPCZ_FLAG_BIT(3)
#define IPCZ_TRAP_ABOVE_MIN_LOCAL_BYTES IPCZ_FLAG_BIT(4)
#define IPCZ_TRAP_BELOW_MAX_REMOTE_PARCELS IPCZ_FLAG_BIT(5)
#define IPCZ_TRAP_BELOW_MAX_REMOTE_BYTES IPCZ_FLAG_BIT(6)
#define IPCZ_TRAP_NEW_LOCAL_PARCEL IPCZ_FLAG_BIT(7)
#define IPCZ_TRAP_CONSUMED_REMOTE_PARCEL IPCZ_FLAG_BIT(8)
#define IPCZ_TRAP_WITHIN_API_CALL IPCZ_FLAG_BIT(9)

struct IPCZ_ALIGN(8) IpczPutLimits {
  size_t size;
  size_t max_queued_parcels;
  size_t max_queued_bytes;
};

typedef uint32_t IpczBeginPutFlags;

#define IPCZ_BEGIN_PUT_ALLOW_PARTIAL IPCZ_FLAG_BIT(0)

struct IPCZ_ALIGN(8) IpczBeginPutOptions {
  size_t size;
  const struct IpczPutLimits* limits;
};

typedef uint32_t IpczEndPutFlags;
#define IPCZ_END_PUT_ABORT IPCZ_FLAG_BIT(0)

typedef uint32_t IpczPortalStatusFlags;

#define IPCZ_PORTAL_STATUS_PEER_CLOSED IPCZ_FLAG_BIT(0)
#define IPCZ_PORTAL_STATUS_DEAD IPCZ_FLAG_BIT(1)

struct IPCZ_ALIGN(8) IpczPortalStatus {
  size_t size;
  IpczPortalStatusFlags flags;
  size_t num_local_parcels;
  size_t num_local_bytes;
  size_t num_remote_parcels;
  size_t num_remote_bytes;
};

typedef uint32_t IpczGetFlags;

#define IPCZ_GET_PARTIAL IPCZ_FLAG_BIT(0)
#define IPCZ_GET_PARCEL_ONLY IPCZ_FLAG_BIT(1)

typedef uint32_t IpczBeginGetFlags;

typedef uint32_t IpczEndGetFlags;

typedef uint32_t IpczBoxType;

#define IPCZ_BOX_TYPE_DRIVER_OBJECT ((IpczBoxType)0)
#define IPCZ_BOX_TYPE_APPLICATION_OBJECT ((IpczBoxType)1)
#define IPCZ_BOX_TYPE_SUBPARCEL ((IpczBoxType)2)

typedef IpczResult (*IpczApplicationObjectSerializer)(uintptr_t object,
                                                      uint32_t flags,
                                                      const void* options,
                                                      volatile void* data,
                                                      size_t* num_bytes,
                                                      IpczHandle* handles,
                                                      size_t* num_handles);

typedef void (*IpczApplicationObjectDestructor)(uintptr_t object,
                                                uint32_t flags,
                                                const void* options);

struct IPCZ_ALIGN(8) IpczBoxContents {
  size_t size;
  IpczBoxType type;
  union {
    IpczDriverHandle driver_object;
    uintptr_t application_object;
    IpczHandle subparcel;
  } object;
  IpczApplicationObjectSerializer serializer;
  IpczApplicationObjectDestructor destructor;
};

#define IPCZ_END_GET_ABORT IPCZ_FLAG_BIT(0)

typedef uint32_t IpczUnboxFlags;

#define IPCZ_UNBOX_PEEK IPCZ_FLAG_BIT(0)

struct IPCZ_ALIGN(8) IpczTrapConditions {
  size_t size;
  IpczTrapConditionFlags flags;
  size_t min_local_parcels;
  size_t min_local_bytes;
  size_t max_remote_parcels;
  size_t max_remote_bytes;
};

struct IPCZ_ALIGN(8) IpczTrapEvent {
  size_t size;
  uintptr_t context;
  IpczTrapConditionFlags condition_flags;
  const struct IpczPortalStatus* status;
};

typedef void(IPCZ_API* IpczTrapEventHandler)(const struct IpczTrapEvent* event);

#if defined(__cplusplus)
extern "C" {
#endif

typedef IpczResult(IPCZ_API* IpczTransportActivityHandler)(
    IpczHandle transport,                    // in
    const void* data,                        // in
    size_t num_bytes,                        // in
    const IpczDriverHandle* driver_handles,  // in
    size_t num_driver_handles,               // in
    IpczTransportActivityFlags flags,        // in
    const void* options);                    // in

struct IPCZ_ALIGN(8) IpczSharedMemoryInfo {
  size_t size;
  size_t region_num_bytes;
};

struct IPCZ_ALIGN(8) IpczDriver {
 size_t size;

  IpczResult(IPCZ_API* Close)(IpczDriverHandle handle,  // in
                              uint32_t flags,           // in
                              const void* options);     // in

  IpczResult(IPCZ_API* Serialize)(IpczDriverHandle handle,     // in
                                  IpczDriverHandle transport,  // in
                                  uint32_t flags,              // in
                                  const void* options,         // in
                                  volatile void* data,         // out
                                  size_t* num_bytes,           // in/out
                                  IpczDriverHandle* handles,   // out
                                  size_t* num_handles);        // in/out

  IpczResult(IPCZ_API* Deserialize)(
      const volatile void* data,               // in
      size_t num_bytes,                        // in
      const IpczDriverHandle* driver_handles,  // in
      size_t num_driver_handles,               // in
      IpczDriverHandle transport,              // in
      uint32_t flags,                          // in
      const void* options,                     // in
      IpczDriverHandle* handle);               // out

  IpczResult(IPCZ_API* CreateTransports)(
      IpczDriverHandle transport0,        // in
      IpczDriverHandle transport1,        // in
      uint32_t flags,                     // in
      const void* options,                // in
      IpczDriverHandle* new_transport0,   // out
      IpczDriverHandle* new_transport1);  // out

  IpczResult(IPCZ_API* ActivateTransport)(
      IpczDriverHandle driver_transport,              // in
      IpczHandle transport,                           // in
      IpczTransportActivityHandler activity_handler,  // in
      uint32_t flags,                                 // in
      const void* options);                           // in

  IpczResult(IPCZ_API* DeactivateTransport)(
      IpczDriverHandle driver_transport,  // in
      uint32_t flags,                     // in
      const void* options);               // in

  IpczResult(IPCZ_API* Transmit)(IpczDriverHandle driver_transport,       // in
                                 const void* data,                        // in
                                 size_t num_bytes,                        // in
                                 const IpczDriverHandle* driver_handles,  // in
                                 size_t num_driver_handles,               // in
                                 uint32_t flags,                          // in
                                 const void* options);                    // in

  IpczResult(IPCZ_API* ReportBadTransportActivity)(IpczDriverHandle transport,
                                                   uintptr_t context,
                                                   uint32_t flags,
                                                   const void* options);

  IpczResult(IPCZ_API* AllocateSharedMemory)(
      size_t num_bytes,                  // in
      uint32_t flags,                    // in
      const void* options,               // in
      IpczDriverHandle* driver_memory);  // out

  IpczResult(IPCZ_API* GetSharedMemoryInfo)(
      IpczDriverHandle driver_memory,      // in
      uint32_t flags,                      // in
      const void* options,                 // in
      struct IpczSharedMemoryInfo* info);  // out

  IpczResult(IPCZ_API* DuplicateSharedMemory)(
      IpczDriverHandle driver_memory,        // in
      uint32_t flags,                        // in
      const void* options,                   // in
      IpczDriverHandle* new_driver_memory);  // out

  IpczResult(IPCZ_API* MapSharedMemory)(
      IpczDriverHandle driver_memory,     // in
      uint32_t flags,                     // in
      const void* options,                // in
      volatile void** address,            // out
      IpczDriverHandle* driver_mapping);  // out

  IpczResult(IPCZ_API* GenerateRandomBytes)(size_t num_bytes,     // in
                                            uint32_t flags,       // in
                                            const void* options,  // in
                                            void* buffer);        // out
};

struct IPCZ_ALIGN(8) IpczAPI {
  size_t size;
  IpczResult(IPCZ_API* Close)(IpczHandle handle,     // in
                              uint32_t flags,        // in
                              const void* options);  // in

  IpczResult(IPCZ_API* CreateNode)(const struct IpczDriver* driver,  // in
                                   IpczCreateNodeFlags flags,        // in
                                   const void* options,              // in
                                   IpczHandle* node);                // out

  IpczResult(IPCZ_API* ConnectNode)(IpczHandle node,                    // in
                                    IpczDriverHandle driver_transport,  // in
                                    size_t num_initial_portals,         // in
                                    IpczConnectNodeFlags flags,         // in
                                    const void* options,                // in
                                    IpczHandle* initial_portals);       // out

  IpczResult(IPCZ_API* OpenPortals)(IpczHandle node,       // in
                                    uint32_t flags,        // in
                                    const void* options,   // in
                                    IpczHandle* portal0,   // out
                                    IpczHandle* portal1);  // out

  IpczResult(IPCZ_API* QueryPortalStatus)(
      IpczHandle portal,                 // in
      uint32_t flags,                    // in
      const void* options,               // in
      struct IpczPortalStatus* status);  // out

  IpczResult(IPCZ_API* Put)(IpczHandle portal,                      // in
                            const void* data,                       // in
                            size_t num_bytes,                       // in
                            const IpczHandle* handles,              // in
                            size_t num_handles,                     // in
                            uint32_t flags,                         // in
                            const struct IpczPutOptions* options);  // in

  IpczResult(IPCZ_API* BeginPut)(
      IpczHandle portal,                          // in
      IpczBeginPutFlags flags,                    // in
      const struct IpczBeginPutOptions* options,  // in
      size_t* num_bytes,                          // out
      void** data);                               // out

  IpczResult(IPCZ_API* EndPut)(IpczHandle portal,          // in
                               size_t num_bytes_produced,  // in
                               const IpczHandle* handles,  // in
                               size_t num_handles,         // in
                               IpczEndPutFlags flags,      // in
                               const void* options);       // in

  IpczResult(IPCZ_API* Get)(IpczHandle portal,       // in
                            IpczGetFlags flags,      // in
                            const void* options,     // in
                            void* data,              // out
                            size_t* num_bytes,       // in/out
                            IpczHandle* handles ,    // out
                            size_t* num_handles,     // in/out
                            IpczHandle* validator);  // out

  IpczResult(IPCZ_API* BeginGet)(IpczHandle source,              // in
                                 IpczBeginGetFlags flags,        // in
                                 const void* options,            // in
                                 const volatile void** data,     // out
                                 size_t* num_bytes,              // out
                                 IpczHandle* handles,            // out
                                 size_t* num_handles,            // in/out
                                 IpczTransaction* transaction);  // out

  IpczResult(IPCZ_API* EndGet)(IpczHandle source,
                               IpczTransaction transaction,  // in
                               IpczEndGetFlags flags,        // in
                               const void* options,          // in
                               IpczHandle* parcel);          // in

  IpczResult(IPCZ_API* MergePortals)(IpczHandle first,      // in
                                     IpczHandle second,     // in
                                     uint32_t flags,        // in
                                     const void* options);  // out

  IpczResult(IPCZ_API* Trap)(
      IpczHandle portal,                                  // in
      const struct IpczTrapConditions* conditions,        // in
      IpczTrapEventHandler handler,                       // in
      uintptr_t context,                                  // in
      uint32_t flags,                                     // in
      const void* options,                                // in
      IpczTrapConditionFlags* satisfied_condition_flags,  // out
      struct IpczPortalStatus* status);                   // out

  IpczResult(IPCZ_API* Reject)(IpczHandle validator,
                               uintptr_t context,
                               uint32_t flags,
                               const void* options);

  IpczResult(IPCZ_API* Box)(IpczHandle node,                   // in
                             const IpczBoxContents* contents,  // in
                            uint32_t flags,                    // in
                            const void* options,               // in
                            IpczHandle* handle);               // out

  IpczResult(IPCZ_API* Unbox)(IpczHandle handle,           // in
                              IpczUnboxFlags flags,        // in
                              const void* options,         // in
                              IpczBoxContents* contents);  // out
};

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif  // IPCZ_INCLUDE_IPCZ_IPCZ_H_
