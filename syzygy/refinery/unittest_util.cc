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

#include "syzygy/refinery/unittest_util.h"

#include "gtest/gtest.h"
#include "syzygy/core/unittest_util.h"

namespace testing {

const base::FilePath TestMinidumps::GetNotepad32Dump() {
  return GetSrcRelativePath(L"syzygy\\refinery\\test_data\\notepad-32bit.dmp");
}

const base::FilePath TestMinidumps::GetNotepad64Dump() {
  return GetSrcRelativePath(L"syzygy\\refinery\\test_data\\notepad-64bit.dmp");
}

}  // namespace testing
