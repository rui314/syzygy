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

#include "syzygy/refinery/types/type_repository.h"

#include "gtest/gtest.h"
#include "syzygy/refinery/types/type.h"

namespace refinery {

TEST(TypeRepositoryTest, AddType) {
  TypeRepository repo;
  EXPECT_EQ(0U, repo.size());

  // Returns a NULL type for unknown TypeId.
  EXPECT_FALSE(repo.GetType(1));

  for (auto type : repo)
    FAIL() << "Non-empty enumeration in an empty TypeRepository";

  TypePtr t1 = new BasicType(L"uint", 4);
  TypePtr t2 = new BasicType(L"int", 4);
  EXPECT_EQ(nullptr, t1->repository());
  EXPECT_EQ(nullptr, t2->repository());

  TypeId id1 = repo.AddType(t1);
  TypeId id2 = repo.AddType(t2);
  EXPECT_EQ(2U, repo.size());

  EXPECT_NE(id1, id2);

  EXPECT_EQ(&repo, t1->repository());
  EXPECT_EQ(&repo, t2->repository());

  EXPECT_EQ(t1, repo.GetType(id1));
  EXPECT_EQ(t2, repo.GetType(id2));
}

TEST(TypeRepositoryTest, Iteration) {
  TypeRepository repo;

  repo.AddType(new BasicType(L"one", 4));
  repo.AddType(new BasicType(L"two", 4));
  repo.AddType(new BasicType(L"three", 4));
  EXPECT_EQ(3U, repo.size());

  size_t iterated = 0;
  for (auto type : repo) {
    ++iterated;

    ASSERT_TRUE(type);
    EXPECT_EQ(Type::BASIC_TYPE_KIND, type->kind());
    EXPECT_TRUE(type->name() == L"one" ||
                type->name() == L"two" ||
                type->name() == L"three");
  }

  EXPECT_EQ(3U, iterated);
}

}  // namespace refinery
