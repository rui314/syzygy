// Copyright 2012 Google Inc. All Rights Reserved.
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

#include <windows.h>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/synchronization/lock.h"
#include "syzygy/agent/asan/iat_patcher.h"
#include "syzygy/agent/asan/memory_interceptors.h"
#include "syzygy/agent/asan/rtl_impl.h"
#include "syzygy/agent/asan/runtime.h"
#include "syzygy/agent/asan/runtime_util.h"
#include "syzygy/agent/common/agent.h"
#include "syzygy/common/logging.h"

namespace agent {
namespace asan {
namespace {

// This lock guards against IAT patching on multiple threads concurrently.
base::Lock patch_lock;

// Our AtExit manager required by base.
base::AtExitManager* at_exit = nullptr;

// The asan runtime manager.
AsanRuntime* asan_runtime = nullptr;

void SetUpAtExitManager() {
  DCHECK_EQ(static_cast<base::AtExitManager*>(nullptr), at_exit);
  at_exit = new base::AtExitManager();
  CHECK_NE(static_cast<base::AtExitManager*>(nullptr), at_exit);
}

void TearDownAtExitManager() {
  DCHECK_NE(static_cast<base::AtExitManager*>(nullptr), at_exit);
  delete at_exit;
  at_exit = nullptr;
}

MemoryAccessorMode OnRedirectStubEntry(const void* caller_address) {
  // This grabs the loader's lock, which could be a problem. If there are
  // multiple instrumented DLLs, or a single one executing on multiple threads,
  // there could be lock inversion here. The possibility seems remote, though.
  // Maybe locating the module associated with the caller_address can be done
  // with a VirtualQuery, with a fallback to the loader for an additional pair
  // of belt-and-suspenders...
  const DWORD kFlags = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                       GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
  HMODULE calling_module = nullptr;
  BOOL success = ::GetModuleHandleEx(
      kFlags, reinterpret_cast<LPCWSTR>(caller_address), &calling_module);
  CHECK_EQ(TRUE, success);

  // Build the IAT patch map.
  IATPatchMap patch_map;
  for (size_t i = 0; i < kNumMemoryAccessorVariants; ++i) {
    patch_map.insert(std::make_pair(kMemoryAccessorVariants[i].name,
                                    kMemoryAccessorVariants[i].accessor_2G));
  }

  // Grab the patching lock only while patching the caller's IAT. Assuming no
  // other parties are patching this IAT, this is sufficient to make
  // double-patching due to multiple threads invoking on instrumentation
  // concurrently idempotent.
  base::AutoLock lock(patch_lock);
  CHECK(PatchIATForModule(calling_module, patch_map));

  return MEMORY_ACCESSOR_MODE_2G;
}

}  // namespace

extern "C" {

BOOL WINAPI DllMain(HMODULE instance, DWORD reason, LPVOID reserved) {
  agent::common::InitializeCrt();

  switch (reason) {
    case DLL_PROCESS_ATTACH: {
      // Create the At-Exit manager.
      SetUpAtExitManager();

      // Disable logging. In the case of Chrome this is running in a sandboxed
      // process where logging to file doesn't help us any. In other cases the
      // log output will still go to console.
      base::CommandLine::Init(0, NULL);
      ::common::InitLoggingForDll(L"asan");

      SetUpAsanRuntime(&asan_runtime);

      // Hookup IAT patching on redirector stub entry.
      agent::asan::SetRedirectEntryCallback(base::Bind(OnRedirectStubEntry));
      break;
    }

    case DLL_THREAD_ATTACH: {
      agent::asan::AsanRuntime* runtime = agent::asan::AsanRuntime::runtime();
      DCHECK_NE(static_cast<agent::asan::AsanRuntime*>(nullptr), runtime);
      runtime->AddThreadId(::GetCurrentThreadId());
      break;
    }

    case DLL_THREAD_DETACH:
      // Nothing to do here.
      break;

    case DLL_PROCESS_DETACH: {
      base::CommandLine::Reset();
      // This should be the last thing called in the agent DLL before it
      // gets unloaded. Everything should otherwise have been initialized
      // and we're now just cleaning it up again.
      agent::asan::TearDownRtl();
      TearDownAsanRuntime(&asan_runtime);
      TearDownAtExitManager();
      break;
    }

    default:
      NOTREACHED();
      break;
  }

  return TRUE;
}

}  // extern "C"

}  // namespace asan
}  // namespace agent
