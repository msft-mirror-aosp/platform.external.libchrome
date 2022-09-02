// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/memory_infra_background_allowlist.h"

#include <ctype.h>
#include <string.h>

#include <string>

#include "base/allocator/buildflags.h"
#include "base/containers/fixed_flat_set.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"

namespace base {
namespace trace_event {
namespace {

// The names of dump providers allowed to perform background tracing. Dump
// providers can be added here only if the background mode dump has very
// little processor and memory overhead.
// TODO(ssid): Some dump providers do not create ownership edges on background
// dump. So, the effective size will not be correct.
constexpr auto kDumpProviderAllowlist =
    base::MakeFixedFlatSet<base::StringPiece>({
      // clang-format off
        "android::ResourceManagerImpl",
        "AutocompleteController",
        "BlinkGC",
        "BlinkObjectCounters",
        "BlobStorageContext",
        "Canvas",
        "cc::ResourcePool",
        "ClientDiscardableSharedMemoryManager",
        "DevTools",
        "DiscardableSharedMemoryManager",
        "DOMStorage",
        "DownloadService",
        "ExtensionFunctions",
        "gpu::BufferManager",
        "gpu::RenderbufferManager",
        "gpu::ServiceDiscardableManager",
        "gpu::ServiceTransferCache",
        "gpu::SharedImageStub",
        "gpu::TextureManager",
        "GrShaderCache",
        "FontCaches",
        "HistoryReport",
#if BUILDFLAG(IS_MAC)
        "CommandBuffer",
#endif
        "IPCChannel",
        "IndexedDBBackingStore",
        "IndexedDBFactoryImpl",
        "InMemoryURLIndex",
        "JavaHeap",
        "LevelDB",
        "LeveldbValueStore",
        "LocalStorage",
        "MadvFreeDiscardableMemoryAllocator",
        "Malloc",
        "ManualFillingCache",
        "MemoryCache",
        "MojoHandleTable",
        "MojoLevelDB",
        "MojoMessages",
        "PartitionAlloc",
        "PartitionAlloc.AddressSpace",
        "ProcessMemoryMetrics",
        "SharedContextState",
        "SharedMemoryTracker",
        "Skia",
        "Sql",
        "URLRequestContext",
        "V8Isolate",
        "WebMediaPlayer_MainThread",
        "WebMediaPlayer_MediaThread",
        "TabRestoreServiceHelper",
      // clang-format on
    });

// A list of string names that are allowed for the memory allocator dumps in
// background mode.
constexpr auto kAllocatorDumpNameAllowlist = base::MakeFixedFlatSet<
    base::StringPiece>({
  // clang-format off
        // Some of the blink values vary based on compile time flags. The
        // compile timeflags are not in base, so all are listed here.
        "blink_gc/main/allocated_objects",
        "blink_gc/main/heap",
        "blink_gc/workers/heap/worker_0x?",
        "blink_gc/workers/worker_0x?/heap",
        "blink_gc/workers/worker_0x?/allocated_objects",
        "blink_objects/AdSubframe",
        "blink_objects/ArrayBufferContents",
        "blink_objects/AudioHandler",
        "blink_objects/ContextLifecycleStateObserver",
        "blink_objects/DetachedScriptState",
        "blink_objects/Document",
        "blink_objects/Frame",
        "blink_objects/JSEventListener",
        "blink_objects/LayoutObject",
        "blink_objects/MediaKeySession",
        "blink_objects/MediaKeys",
        "blink_objects/Node",
        "blink_objects/Resource",
        "blink_objects/RTCPeerConnection",
        "blink_objects/ScriptPromise",
        "blink_objects/V8PerContextData",
        "blink_objects/WorkerGlobalScope",
        "blink_objects/UACSSResource",
        "blink_objects/ResourceFetcher",
        "canvas/ResourceProvider/SkSurface",
        "canvas/ResourceProvider/SkSurface/0x?",
        "cc/tile_memory/provider_0x?",
        "components/download/controller_0x?",
        "devtools/file_watcher_0x?",
        "discardable",
        "discardable/madv_free_allocated",
        "discardable/child_0x?",
        "extensions/functions",
        "extensions/value_store/Extensions.Database.Open.Settings/0x?",
        "extensions/value_store/Extensions.Database.Open.Rules/0x?",
        "extensions/value_store/Extensions.Database.Open.State/0x?",
        "extensions/value_store/Extensions.Database.Open.Scripts/0x?",
        "extensions/value_store/Extensions.Database.Open.WebAppsLockScreen/0x?",
        "extensions/value_store/Extensions.Database.Open/0x?",
        "extensions/value_store/Extensions.Database.Restore/0x?",
        "extensions/value_store/Extensions.Database.Value.Restore/0x?",
        "font_caches/font_platform_data_cache",
        "font_caches/shape_caches",
        "gpu/discardable_cache/cache_0x?",
        "gpu/discardable_cache/cache_0x?/avg_image_size",
        "gpu/gl/buffers/context_group_0x?",
        "gpu/gl/renderbuffers/context_group_0x?",
        "gpu/gl/textures/context_group_0x?",
        "gpu/gr_shader_cache/cache_0x?",
        "gpu/shared_images/client_0x?",
        "gpu/transfer_cache/cache_0x?",
        "gpu/transfer_cache/cache_0x?/avg_image_size",
        "history/delta_file_service/leveldb_0x?",
        "history/usage_reports_buffer/leveldb_0x?",
#if BUILDFLAG(IS_MAC)
        "ioaccelerator",
        "iosurface",
#endif
        "java_heap",
        "java_heap/allocated_objects",
        "leveldatabase",
        "leveldatabase/block_cache/browser",
        "leveldatabase/block_cache/in_memory",
        "leveldatabase/block_cache/unified",
        "leveldatabase/block_cache/web",
        "leveldatabase/db_0x?",
        "leveldatabase/db_0x?/block_cache",
        "leveldatabase/memenv_0x?",
        "malloc",
        "malloc/allocated_objects",
        "malloc/metadata_fragmentation_caches",
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
        "malloc/partitions",
        "malloc/partitions/allocator",
        "malloc/partitions/allocator/thread_cache",
        "malloc/partitions/allocator/thread_cache/main_thread",
        "malloc/partitions/aligned",
        "malloc/partitions/original",
        "malloc/partitions/nonscannable",
        "malloc/partitions/nonquarantinable",
        "malloc/sys_malloc",
        "malloc/win_heap",
#endif
        "media/webmediaplayer/audio/player_0x?",
        "media/webmediaplayer/data_source/player_0x?",
        "media/webmediaplayer/demuxer/player_0x?",
        "media/webmediaplayer/video/player_0x?",
        "media/webmediaplayer/player_0x?",
        "mojo",
        "mojo/data_pipe_consumer",
        "mojo/data_pipe_producer",
        "mojo/invitation",
        "mojo/messages",
        "mojo/message_pipe",
        "mojo/platform_handle",
        "mojo/queued_ipc_channel_message/0x?",
        "mojo/shared_buffer",
        "mojo/unknown",
        "mojo/watcher",
        "omnibox/autocomplete_controller/0x?",
        "omnibox/in_memory_url_index/0x?",
        "passwords/manual_filling_controller/0x?",
        "web_cache/Image_resources",
        "web_cache/CSS stylesheet_resources",
        "web_cache/Script_resources",
        "web_cache/XSL stylesheet_resources",
        "web_cache/Font_resources",
        "web_cache/Code_cache",
        "web_cache/Encoded_size_duplicated_in_data_urls",
        "web_cache/Other_resources",
        "partition_alloc/allocated_objects",
        "partition_alloc/address_space",
        "partition_alloc/partitions",
        "partition_alloc/partitions/array_buffer",
        "partition_alloc/partitions/buffer",
        "partition_alloc/partitions/fast_malloc",
#if !BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
        "partition_alloc/partitions/fast_malloc/thread_cache",
        "partition_alloc/partitions/fast_malloc/thread_cache/main_thread",
#endif
        "partition_alloc/partitions/layout",
        "skia/gpu_resources/context_0x?",
        "skia/sk_glyph_cache",
        "skia/sk_resource_cache",
        "sqlite",
        "ui/resource_manager_0x?/default_resource/0x?",
        "ui/resource_manager_0x?/tinted_resource",
        "site_storage/blob_storage/0x?",
        "v8/main/code_stats",
        "v8/main/contexts/detached_context",
        "v8/main/contexts/native_context",
        "v8/main/global_handles",
        "v8/main/heap/code_space",
        "v8/main/heap/code_stats",
        "v8/main/heap/code_large_object_space",
        "v8/main/heap/large_object_space",
        "v8/main/heap/map_space",
        "v8/main/heap/new_large_object_space",
        "v8/main/heap/new_space",
        "v8/main/heap/old_space",
        "v8/main/heap/read_only_space",
        "v8/main/malloc",
        "v8/main/zapped_for_debug",
        "v8/utility/code_stats",
        "v8/utility/contexts/detached_context",
        "v8/utility/contexts/native_context",
        "v8/utility/global_handles",
        "v8/utility/heap/code_space",
        "v8/utility/heap/code_large_object_space",
        "v8/utility/heap/large_object_space",
        "v8/utility/heap/map_space",
        "v8/utility/heap/new_large_object_space",
        "v8/utility/heap/new_space",
        "v8/utility/heap/old_space",
        "v8/utility/heap/read_only_space",
        "v8/utility/malloc",
        "v8/utility/zapped_for_debug",
        "v8/workers/code_stats/isolate_0x?",
        "v8/workers/contexts/detached_context/isolate_0x?",
        "v8/workers/contexts/native_context/isolate_0x?",
        "v8/workers/global_handles/isolate_0x?",
        "v8/workers/heap/code_space/isolate_0x?",
        "v8/workers/heap/code_large_object_space/isolate_0x?",
        "v8/workers/heap/large_object_space/isolate_0x?",
        "v8/workers/heap/map_space/isolate_0x?",
        "v8/workers/heap/new_large_object_space/isolate_0x?",
        "v8/workers/heap/new_space/isolate_0x?",
        "v8/workers/heap/old_space/isolate_0x?",
        "v8/workers/heap/read_only_space/isolate_0x?",
        "v8/workers/malloc/isolate_0x?",
        "v8/workers/zapped_for_debug/isolate_0x?",
        "site_storage/index_db/db_0x?",
        "site_storage/index_db/memenv_0x?",
        "site_storage/index_db/in_flight_0x?",
        "site_storage/local_storage/0x?/cache_size",
        "site_storage/localstorage/0x?/cache_size",
        "site_storage/localstorage/0x?/leveldb",
        "site_storage/session_storage/0x?",
        "site_storage/session_storage/0x?/cache_size",
        "tab_restore/service_helper_0x?/entries",
        "tab_restore/service_helper_0x?/entries/group_0x?",
        "tab_restore/service_helper_0x?/entries/tab_0x?",
        "tab_restore/service_helper_0x?/entries/window_0x?",
        "tracing/heap_profiler_blink_gc/AllocationRegister",
        "tracing/heap_profiler_malloc/AllocationRegister",
        "tracing/heap_profiler_partition_alloc/AllocationRegister",
  // clang-format on
});

const char* const* g_dump_provider_allowlist_for_testing = nullptr;
const char* const* g_allocator_dump_name_allowlist_for_testing = nullptr;

bool IsNameInList(const char* name, const char* const* list) {
  for (size_t i = 0; list[i] != nullptr; ++i) {
    if (strcmp(name, list[i]) == 0)
      return true;
  }
  return false;
}

}  // namespace

bool IsMemoryDumpProviderInAllowlist(const char* mdp_name) {
  if (!g_dump_provider_allowlist_for_testing) {
    return kDumpProviderAllowlist.contains(mdp_name);
  } else {
    return IsNameInList(mdp_name, g_dump_provider_allowlist_for_testing);
  }
}

bool IsMemoryAllocatorDumpNameInAllowlist(const std::string& name) {
  // Global dumps that are of hex digits are all allowed for background use.
  if (base::StartsWith(name, "global/", CompareCase::SENSITIVE)) {
    for (size_t i = strlen("global/"); i < name.size(); i++)
      if (!base::IsHexDigit(name[i]))
        return false;
    return true;
  }

  if (base::StartsWith(name, "shared_memory/", CompareCase::SENSITIVE)) {
    for (size_t i = strlen("shared_memory/"); i < name.size(); i++)
      if (!base::IsHexDigit(name[i]))
        return false;
    return true;
  }

  // Remove special characters, numbers (including hexadecimal which are marked
  // by '0x') from the given string.
  const size_t length = name.size();
  std::string stripped_str;
  stripped_str.reserve(length);
  bool parsing_hex = false;
  for (size_t i = 0; i < length; ++i) {
    if (parsing_hex && isxdigit(name[i]))
      continue;
    parsing_hex = false;
    if (i + 1 < length && name[i] == '0' && name[i + 1] == 'x') {
      parsing_hex = true;
      stripped_str.append("0x?");
      ++i;
    } else {
      stripped_str.push_back(name[i]);
    }
  }

  if (!g_allocator_dump_name_allowlist_for_testing) {
    return kAllocatorDumpNameAllowlist.contains(stripped_str);
  } else {
    return IsNameInList(stripped_str.c_str(),
                        g_allocator_dump_name_allowlist_for_testing);
  }
}

void SetDumpProviderAllowlistForTesting(const char* const* list) {
  g_dump_provider_allowlist_for_testing = list;
}

void SetAllocatorDumpNameAllowlistForTesting(const char* const* list) {
  g_allocator_dump_name_allowlist_for_testing = list;
}

}  // namespace trace_event
}  // namespace base
