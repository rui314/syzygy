// Copyright 2015 Google Inc. All Rights Reserved.
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

#include "syzygy/refinery/analyzers/exception_analyzer.h"

#include <dbghelp.h>

#include "base/memory/scoped_ptr.h"
#include "syzygy/refinery/process_state/process_state_util.h"
#include "syzygy/refinery/process_state/refinery.pb.h"

namespace refinery {

Analyzer::AnalysisResult ExceptionAnalyzer::Analyze(
    const Minidump& minidump,
    ProcessState* process_state) {
  DCHECK(process_state != nullptr);

  // Retrieve the unique exception stream.
  Minidump::Stream exception_stream =
      minidump.FindNextStream(nullptr, ExceptionStream);
  if (!exception_stream.IsValid()) {
    // Minidump has no exception data.
    return ANALYSIS_COMPLETE;
  }
  Minidump::Stream offending_stream =
      minidump.FindNextStream(&exception_stream, ExceptionStream);
  if (offending_stream.IsValid())
    return ANALYSIS_ERROR;

  MINIDUMP_EXCEPTION_STREAM minidump_exception_stream = {};
  if (!exception_stream.ReadElement(&minidump_exception_stream))
    return ANALYSIS_ERROR;
  const MINIDUMP_EXCEPTION& exception_record =
      minidump_exception_stream.ExceptionRecord;

  // TODO(manzagop): Read the exception's thread context. Reuse code from
  // ThreadAnalyzer.

  // TODO(manzagop): Consider chained exceptions
  // (exception_record.ExceptionRecord).

  // Retrieve and populate the exception information from the stack layer.
  StackRecordPtr stack_record;
  if (!process_state->FindStackRecord(minidump_exception_stream.ThreadId,
                                      &stack_record)) {
    return ANALYSIS_ERROR;
  }
  DCHECK(stack_record->mutable_data());
  ThreadInformation* thread_info =
      stack_record->mutable_data()->mutable_thread_info();
  DCHECK(!thread_info->has_exception());
  Exception* exception = thread_info->mutable_exception();

  exception->set_thread_id(minidump_exception_stream.ThreadId);
  exception->set_exception_code(exception_record.ExceptionCode);
  exception->set_exception_flags(exception_record.ExceptionFlags);
  exception->set_exception_record(exception_record.ExceptionRecord);
  exception->set_exception_address(exception_record.ExceptionAddress);
  for (int i = 0; i < exception_record.NumberParameters; ++i) {
    exception->add_exception_information(
        exception_record.ExceptionInformation[i]);
  }

  return ANALYSIS_COMPLETE;
}

}  // namespace refinery
