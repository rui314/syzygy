// Copyright 2014 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include "syzygy/agent/asan/memory_interceptors.h"

#include <stdint.h>

#include "base/logging.h"
#include "base/macros.h"
#include "syzygy/agent/asan/rtl_utils.h"
#include "syzygy/agent/asan/shadow.h"

using agent::asan::Shadow;

namespace agent {
namespace asan {
namespace {

RedirectEntryCallback redirect_entry_callback;

}  // namespace

const MemoryAccessorVariants kMemoryAccessorVariants[] = {
#define ENUM_MEM_INTERCEPT_FUNCTION_VARIANTS(access_size, \
                                             access_mode_str, \
                                             access_mode_value) \
  { \
    "asan_check_" #access_size "_byte_" #access_mode_str, \
    asan_redirect_ ## access_size ## _byte_ ## access_mode_str, \
    asan_no_check, \
    asan_check_ ## access_size ## _byte_ ## access_mode_str \
  }, { \
    "asan_check_" #access_size "_byte_" #access_mode_str "_no_flags", \
    asan_redirect_ ## access_size ## _byte_ ## access_mode_str ## _no_flags, \
    asan_no_check, \
    asan_check_ ## access_size ## _byte_ ## access_mode_str ## _no_flags, \
  },

  ASAN_MEM_INTERCEPT_FUNCTIONS(ENUM_MEM_INTERCEPT_FUNCTION_VARIANTS)

#undef ENUM_MEM_INTERCEPT_FUNCTION_VARIANTS

#define ENUM_STRING_INTERCEPT_FUNCTION_VARIANTS( \
    func, prefix, counter, dst_mode, src_mode, access_size, compare) \
  { \
    "asan_check" #prefix #access_size "_byte_" #func "_access", \
    asan_redirect ## prefix ## access_size ## _byte_ ## func ## _access, \
    asan_string_no_check, \
    asan_check ## prefix ## access_size ## _byte_ ## func ## _access, \
  },

  ASAN_STRING_INTERCEPT_FUNCTIONS(ENUM_STRING_INTERCEPT_FUNCTION_VARIANTS)

#undef ENUM_STRING_INTERCEPT_FUNCTION_VARIANTS
};

const size_t kNumMemoryAccessorVariants = arraysize(kMemoryAccessorVariants);

void SetRedirectEntryCallback(const RedirectEntryCallback& callback) {
  redirect_entry_callback = callback;
}

// Check if the memory location is accessible and report an error on bad memory
// accesses.
// @param location The memory address of the access.
// @param access_mode The mode of the access.
// @param access_size The size of the access.
// @param context The registers context of the access.
void CheckMemoryAccess(void* location,
                       AccessMode access_mode,
                       size_t access_size,
                       const AsanContext& context) {
  if (!StaticShadow::shadow.IsAccessible(location))
    ReportBadMemoryAccess(location, access_mode, access_size, context);
}

// Check if the memory accesses done by a string instructions are valid.
// @param dst The destination memory address of the access.
// @param dst_access_mode The destination mode of the access.
// @param src The source memory address of the access.
// @param src_access_mode The source mode of the access.
// @param length The number of memory accesses.
// @param access_size The size of each the access in byte.
// @param increment The increment to move dst/src after each access.
// @param compare Flag to activate shortcut of the execution on difference.
// @param context The registers context of the access.
void CheckStringsMemoryAccesses(
    uint8* dst, AccessMode dst_access_mode,
    uint8* src, AccessMode src_access_mode,
    uint32 length, size_t access_size, int32 increment, bool compare,
    const AsanContext& context) {
  int32 offset = 0;

  for (uint32 i = 0; i < length; ++i) {
    // Check next memory location at src[offset].
    if (src_access_mode != agent::asan::ASAN_UNKNOWN_ACCESS)
      CheckMemoryAccess(&src[offset], src_access_mode, access_size, context);

    // Check next memory location at dst[offset].
    if (dst_access_mode != agent::asan::ASAN_UNKNOWN_ACCESS)
      CheckMemoryAccess(&dst[offset], dst_access_mode, access_size, context);

    // For CMPS instructions, we shortcut the execution of prefix REPZ when
    // memory contents differ.
    if (compare) {
      uint32 src_content = 0;
      uint32 dst_content = 0;
      switch (access_size) {
      case 4:
        src_content = *reinterpret_cast<uint32*>(&src[offset]);
        dst_content = *reinterpret_cast<uint32*>(&dst[offset]);
        break;
      case 2:
        src_content = *reinterpret_cast<uint16*>(&src[offset]);
        dst_content = *reinterpret_cast<uint16*>(&dst[offset]);
        break;
      case 1:
        src_content = *reinterpret_cast<uint8*>(&src[offset]);
        dst_content = *reinterpret_cast<uint8*>(&dst[offset]);
        break;
      default:
        NOTREACHED() << "Unexpected access_size.";
        break;
      }

      if (src_content != dst_content)
        return;
    }

    // Increments offset of dst/src to the next memory location.
    offset += increment;
  }
}

MemoryAccessorFunction RedirectStubEntry(
    const void* caller_address, MemoryAccessorFunction called_redirect) {
  MemoryAccessorMode mode = MEMORY_ACCESSOR_MODE_NOOP;

  // TODO(siggi): Does it make sense to CHECK on this?
  if (!redirect_entry_callback.is_null())
    mode = redirect_entry_callback.Run(caller_address);

  for (size_t i = 0; i < arraysize(kMemoryAccessorVariants); ++i) {
    if (kMemoryAccessorVariants[i].redirect_accessor == called_redirect) {
      switch (mode) {
        case MEMORY_ACCESSOR_MODE_NOOP:
          return kMemoryAccessorVariants[i].accessor_noop;
        case MEMORY_ACCESSOR_MODE_2G:
          return kMemoryAccessorVariants[i].accessor_2G;
        default:
          NOTREACHED();
      }
    }
  }

  NOTREACHED();
  return NULL;
}

}  // namespace asan
}  // namespace agent

// Redefine some enums to make them accessible in the inlined assembly.
// @{
enum AccessMode {
  AsanReadAccess = agent::asan::ASAN_READ_ACCESS,
  AsanWriteAccess = agent::asan::ASAN_WRITE_ACCESS,
  AsanUnknownAccess = agent::asan::ASAN_UNKNOWN_ACCESS,
};
// @}

// The slow path relies on the fact that the shadow memory non accessible byte
// mask has its upper bit set to 1.
COMPILE_ASSERT((agent::asan::kHeapNonAccessibleMarkerMask & (1 << 7)) != 0,
               asan_shadow_mask_upper_bit_is_0);

// Pull in the actual implementation of the accessor stubs.
#include "syzygy/agent/asan/gen/memory_interceptors_impl.cc"
