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

#include "syzygy/kasko/testing/launch_python_process.h"

#include <windows.h>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/files/file_path.h"
#include "base/process/launch.h"
#include "base/process/process_handle.h"
#include "syzygy/core/unittest_util.h"

namespace kasko {
namespace testing {
namespace {

base::win::ScopedHandle DuplicateStdHandleForInheritance(DWORD std_handle) {
  HANDLE original = ::GetStdHandle(std_handle);
  HANDLE duplicate = nullptr;

  if (original && original != INVALID_HANDLE_VALUE) {
    BOOL result = ::DuplicateHandle(base::GetCurrentProcessHandle(), original,
                                    base::GetCurrentProcessHandle(), &duplicate,
                                    0, TRUE, DUPLICATE_SAME_ACCESS);
    CHECK(result);
  }

  return base::win::ScopedHandle(duplicate);
}

}  // namespace

base::Process LaunchPythonProcess(
    const base::FilePath& src_relative_path,
    const base::CommandLine& args) {
  base::CommandLine python_command(args);
  python_command.SetProgram(
      ::testing::GetSrcRelativePath(src_relative_path.value().c_str()));
  python_command.PrependWrapper(
      ::testing::GetSrcRelativePath(L"third_party/python_26/python.exe")
          .value());

  base::win::ScopedHandle stdout_dup =
      DuplicateStdHandleForInheritance(STD_OUTPUT_HANDLE);
  base::win::ScopedHandle stderr_dup =
      DuplicateStdHandleForInheritance(STD_ERROR_HANDLE);
  base::win::ScopedHandle stdin_dup =
      DuplicateStdHandleForInheritance(STD_INPUT_HANDLE);

  base::LaunchOptions launch_options;
  launch_options.inherit_handles = true;
  launch_options.stdin_handle =
      stdin_dup.Get() ? stdin_dup.Get() : INVALID_HANDLE_VALUE;
  launch_options.stdout_handle =
      stdout_dup.Get() ? stdout_dup.Get() : INVALID_HANDLE_VALUE;
  launch_options.stderr_handle =
      stderr_dup.Get() ? stderr_dup.Get() : INVALID_HANDLE_VALUE;

  return base::LaunchProcess(python_command, launch_options);
}

}  // namespace testing
}  // namespace kasko
