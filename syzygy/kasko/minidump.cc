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

#include "syzygy/kasko/minidump.h"

#include <Windows.h>  // NOLINT
#include <DbgHelp.h>

#include "base/files/file.h"
#include "base/process/process_handle.h"
#include "base/win/scoped_handle.h"

#include "syzygy/common/com_utils.h"
#include "syzygy/kasko/minidump_request.h"

namespace kasko {

namespace {

// Minidump with stacks, PEB, TEB, and unloaded module list.
const MINIDUMP_TYPE kSmallDumpType = static_cast<MINIDUMP_TYPE>(
    MiniDumpWithProcessThreadData |  // Get PEB and TEB.
    MiniDumpWithUnloadedModules);  // Get unloaded modules when available.

// Minidump with all of the above, plus memory referenced from stack.
const MINIDUMP_TYPE kLargerDumpType = static_cast<MINIDUMP_TYPE>(
    MiniDumpWithProcessThreadData |  // Get PEB and TEB.
    MiniDumpWithUnloadedModules |  // Get unloaded modules when available.
    MiniDumpWithIndirectlyReferencedMemory);  // Get memory referenced by stack.

// Large dump with all process memory.
const MINIDUMP_TYPE kFullDumpType = static_cast<MINIDUMP_TYPE>(
    MiniDumpWithFullMemory |  // Full memory from process.
    MiniDumpWithProcessThreadData |  // Get PEB and TEB.
    MiniDumpWithHandleData |  // Get all handle information.
    MiniDumpWithUnloadedModules);  // Get unloaded modules when available.

class MinidumpCallbackHandler {
 public:
  explicit MinidumpCallbackHandler(
      const std::vector<MinidumpRequest::MemoryRange>* memory_ranges);

  const MINIDUMP_CALLBACK_INFORMATION* GetMINIDUMP_CALLBACK_INFORMATION() {
    return &minidump_callback_information_;
  }

 private:
  BOOL MemoryCallback(ULONG64* memory_base, ULONG* memory_size);

  static BOOL CALLBACK
  CallbackRoutine(PVOID context,
                  const PMINIDUMP_CALLBACK_INPUT callback_input,
                  PMINIDUMP_CALLBACK_OUTPUT callback_output);

  const std::vector<MinidumpRequest::MemoryRange>* memory_ranges_;
  size_t next_memory_range_index_;
  MINIDUMP_CALLBACK_INFORMATION minidump_callback_information_;

  DISALLOW_COPY_AND_ASSIGN(MinidumpCallbackHandler);
};

MinidumpCallbackHandler::MinidumpCallbackHandler(
    const std::vector<MinidumpRequest::MemoryRange>* memory_ranges)
    : memory_ranges_(memory_ranges),
      next_memory_range_index_(0),
      minidump_callback_information_() {
  minidump_callback_information_.CallbackRoutine =
      &MinidumpCallbackHandler::CallbackRoutine;
  minidump_callback_information_.CallbackParam = reinterpret_cast<void*>(this);
}

BOOL MinidumpCallbackHandler::MemoryCallback(ULONG64* memory_base,
                                             ULONG* memory_size) {
  for (; next_memory_range_index_ < memory_ranges_->size();
       ++next_memory_range_index_) {
    // A zero-length range will terminate memory callbacks. If there is one in
    // our input vector, skip it.
    if ((*memory_ranges_)[next_memory_range_index_].length == 0)
      continue;

    // Include the specified memory region.
    *memory_base = (*memory_ranges_)[next_memory_range_index_].base_address;
    *memory_size = (*memory_ranges_)[next_memory_range_index_].length;
    ++next_memory_range_index_;
    return TRUE;
  }
  return FALSE;
}

// static
BOOL CALLBACK MinidumpCallbackHandler::CallbackRoutine(
    PVOID context,
    const PMINIDUMP_CALLBACK_INPUT callback_input,
    PMINIDUMP_CALLBACK_OUTPUT callback_output) {
  MinidumpCallbackHandler* self =
      reinterpret_cast<MinidumpCallbackHandler*>(context);
  switch (callback_input->CallbackType) {
    case ::MemoryCallback:
      return self->MemoryCallback(&callback_output->MemoryBase,
                                  &callback_output->MemorySize);

    // Include all modules.
    case IncludeModuleCallback:
    case ModuleCallback:
      return TRUE;

    // Include all threads.
    case IncludeThreadCallback:
    case ThreadCallback:
      return TRUE;

    // Stop receiving cancel callbacks.
    case CancelCallback:
      callback_output->CheckCancel = FALSE;
      callback_output->Cancel = FALSE;
      return TRUE;
  }
  // Ignore other callback types.
  return FALSE;
}

}  // namespace

bool GenerateMinidump(const base::FilePath& destination,
                      base::ProcessId target_process_id,
                      base::PlatformThreadId thread_id,
                      const MinidumpRequest& request) {
  base::win::ScopedHandle target_process_handle(
      ::OpenProcess(GENERIC_ALL, FALSE, target_process_id));
  if (!target_process_handle.IsValid()) {
    LOG(ERROR) << "Failed to open target process: " << ::common::LogWe() << ".";
    return false;
  }
  MINIDUMP_EXCEPTION_INFORMATION* dump_exception_pointers = nullptr;
  MINIDUMP_EXCEPTION_INFORMATION dump_exception_info;

  if (request.exception_info_address) {
    dump_exception_info.ThreadId = thread_id;
    dump_exception_info.ExceptionPointers =
        reinterpret_cast<PEXCEPTION_POINTERS>(request.exception_info_address);
    dump_exception_info.ClientPointers = true;

    dump_exception_pointers = &dump_exception_info;
  }

  base::File destination_file(destination, static_cast<base::File::Flags>(
                                               base::File::FLAG_CREATE_ALWAYS |
                                               base::File::FLAG_WRITE));
  if (!destination_file.IsValid()) {
    LOG(ERROR) << "Failed to create destination file: " << destination.value();
    return false;
  }

  MINIDUMP_TYPE platform_minidump_type = kSmallDumpType;

  switch (request.type) {
    case MinidumpRequest::SMALL_DUMP_TYPE:
      platform_minidump_type = kSmallDumpType;
      break;
    case MinidumpRequest::LARGER_DUMP_TYPE:
      platform_minidump_type = kLargerDumpType;
      break;
    case MinidumpRequest::FULL_DUMP_TYPE:
      platform_minidump_type = kFullDumpType;
      break;
    default:
      NOTREACHED();
      break;
  }

  std::vector<MINIDUMP_USER_STREAM> user_streams;
  for (const auto& custom_stream : request.custom_streams) {
    MINIDUMP_USER_STREAM user_stream = {custom_stream.type,
                                        custom_stream.length,
                                        const_cast<void*>(custom_stream.data)};
    user_streams.push_back(user_stream);
  }

  MINIDUMP_USER_STREAM_INFORMATION
  user_stream_information = {user_streams.size(), user_streams.data()};

  MinidumpCallbackHandler callback_handler(
      &request.user_selected_memory_ranges);

  if (::MiniDumpWriteDump(
          target_process_handle.Get(), target_process_id,
          destination_file.GetPlatformFile(), platform_minidump_type,
          dump_exception_pointers, &user_stream_information,
          const_cast<MINIDUMP_CALLBACK_INFORMATION*>(
              callback_handler.GetMINIDUMP_CALLBACK_INFORMATION())) == FALSE) {
    LOG(ERROR) << "MiniDumpWriteDump failed: " << ::common::LogWe() << ".";
    return false;
  }

  return true;
}

}  // namespace kasko
