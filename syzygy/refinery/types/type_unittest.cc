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

#include "syzygy/refinery/types/type.h"

#include "gtest/gtest.h"
#include "syzygy/refinery/types/type_repository.h"

namespace refinery {

namespace {

TypePtr CreateUDT(const wchar_t* name,
                  size_t size,
                  const UserDefinedType::Fields& fields) {
  UserDefinedTypePtr udt = new UserDefinedType(name, size);
  udt->Finalize(fields);
  return udt;
}

TypePtr CreatePointerType(const wchar_t* name,
                          size_t size,
                          Type::Flags flags,
                          TypeId content_type_id) {
  PointerTypePtr ptr = new PointerType(size);
  ptr->Finalize(flags, content_type_id);
  ptr->SetName(name);
  return ptr;
}

}  // namespace

TEST(TypesTest, BasicType) {
  // Create a BasicType and store in a supertype pointer.
  TypePtr type = new BasicType(L"foo", 10);

  ASSERT_TRUE(type.get());
  // Verify the kind and fields.
  EXPECT_EQ(Type::BASIC_TYPE_KIND, type->kind());
  EXPECT_EQ(L"foo", type->name());
  EXPECT_EQ(10U, type->size());

  // Down-cast it.
  BasicTypePtr basic_type;
  ASSERT_TRUE(type->CastTo(&basic_type));
  ASSERT_TRUE(basic_type);

  // Verify that it can't be cast to a PointerType.
  PointerTypePtr ptr;
  EXPECT_FALSE(basic_type->CastTo(&ptr));
  EXPECT_FALSE(ptr.get());
}

TEST(TypesTest, UserDefineType) {
  // Build a UDT instance.
  UserDefinedType::Fields fields;
  TypeRepository repo;

  const TypeId kBasicTypeId = repo.AddType(new BasicType(L"int", 4));
  fields.push_back(
      UserDefinedType::Field(L"one", 0, Type::FLAG_CONST, 0, 0, kBasicTypeId));
  fields.push_back(UserDefinedType::Field(L"two", 4, Type::FLAG_VOLATILE, 0, 0,
                                          kBasicTypeId));
  const TypeId kShortTypeId = repo.AddType(new BasicType(L"short", 2));
  fields.push_back(UserDefinedType::Field(L"three", 8, 0, 0, 0, kShortTypeId));
  UserDefinedTypePtr udt =
      new UserDefinedType(L"foo", 10);
  udt->Finalize(fields);

  repo.AddType(udt);

  // Up-cast it.
  TypePtr type(udt);
  udt = nullptr;

  ASSERT_EQ(Type::USER_DEFINED_TYPE_KIND, type->kind());
  EXPECT_EQ(L"foo", type->name());
  EXPECT_EQ(10, type->size());

  ASSERT_TRUE(type->CastTo(&udt));
  ASSERT_EQ(type.get(), udt.get());

  // Verify the fields set up above.
  ASSERT_EQ(3U, udt->fields().size());

  EXPECT_EQ(0U, udt->fields()[0].offset());
  EXPECT_TRUE(udt->fields()[0].is_const());
  EXPECT_FALSE(udt->fields()[0].is_volatile());
  EXPECT_EQ(kBasicTypeId, udt->fields()[0].type_id());
  BasicTypePtr basic_type;
  ASSERT_TRUE(udt->GetFieldType(0)->CastTo(&basic_type));
  EXPECT_EQ(L"int", basic_type->name());
  EXPECT_EQ(4, basic_type->size());

  EXPECT_EQ(4U, udt->fields()[1].offset());
  EXPECT_FALSE(udt->fields()[1].is_const());
  EXPECT_TRUE(udt->fields()[1].is_volatile());
  EXPECT_EQ(kBasicTypeId, udt->fields()[1].type_id());
  ASSERT_TRUE(udt->GetFieldType(1)->CastTo(&basic_type));
  EXPECT_EQ(L"int", basic_type->name());
  EXPECT_EQ(4, basic_type->size());

  EXPECT_EQ(8U, udt->fields()[2].offset());
  EXPECT_FALSE(udt->fields()[2].is_const());
  EXPECT_FALSE(udt->fields()[2].is_volatile());
  EXPECT_EQ(kShortTypeId, udt->fields()[2].type_id());
  ASSERT_TRUE(udt->GetFieldType(2)->CastTo(&basic_type));
  EXPECT_EQ(L"short", basic_type->name());
  EXPECT_EQ(2, basic_type->size());
}

TEST(TypesTest, PointerType) {
  // Build a Pointer instance.
  TypeRepository repo;
  const TypeId kPtrTypeId = repo.AddType(new BasicType(L"void", 0));
  TypePtr type =
      CreatePointerType(L"void*", 4, Type::FLAG_VOLATILE, kPtrTypeId);
  repo.AddType(type);

  // Test the basic properties.
  ASSERT_TRUE(type);
  EXPECT_EQ(L"void*", type->name());
  EXPECT_EQ(4U, type->size());

  EXPECT_EQ(Type::POINTER_TYPE_KIND, type->kind());

  // Downcast and test its fields.
  PointerTypePtr pointer;
  ASSERT_TRUE(type->CastTo(&pointer));
  ASSERT_TRUE(pointer);
  EXPECT_FALSE(pointer->is_const());
  EXPECT_TRUE(pointer->is_volatile());
  ASSERT_EQ(kPtrTypeId, pointer->content_type_id());

  ASSERT_TRUE(pointer->GetContentType());
  EXPECT_EQ(L"void", pointer->GetContentType()->name());
  EXPECT_EQ(0U, pointer->GetContentType()->size());
}

TEST(TypesTest, WildcardType) {
  // Build a wildcard instance.
  TypeRepository repo;
  TypePtr type = new WildcardType(L"Array", 4);
  repo.AddType(type);

  // Test the basic properties.
  ASSERT_TRUE(type);
  EXPECT_EQ(L"Array", type->name());
  EXPECT_EQ(4U, type->size());

  // Downcast and test its fields.
  WildcardTypePtr wildcard;
  ASSERT_TRUE(type->CastTo(&wildcard));
  ASSERT_TRUE(wildcard);
}

}  // namespace refinery
