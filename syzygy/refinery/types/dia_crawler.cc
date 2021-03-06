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

#include "syzygy/refinery/types/dia_crawler.h"

#include "base/strings/stringprintf.h"
#include "base/win/scoped_bstr.h"
#include "syzygy/pe/dia_util.h"
#include "syzygy/refinery/types/type_repository.h"

namespace refinery {

namespace {

bool GetSymBaseTypeName(IDiaSymbol* symbol, base::string16* type_name) {
  DWORD base_type = 0;
  HRESULT hr = symbol->get_baseType(&base_type);
  if (hr != S_OK)
    return false;

  ULONGLONG length = 0;
  hr = symbol->get_length(&length);
  if (hr != S_OK)
    return false;

  // TODO(siggi): What to do for these basic type names?
  //     One idea is to standardize on stdint.h types?
  switch (base_type) {
    case btNoType:
      *type_name = L"btNoType";
      break;
    case btVoid:
      *type_name = L"void";
      break;
    case btChar:
      *type_name = L"char";
      break;
    case btWChar:
      *type_name = L"wchar_t";
      break;
    case btInt:
    case btLong: {
      switch (length) {
        case 1:
          *type_name = L"int8_t";
          break;
        case 2:
          *type_name = L"int16_t";
          break;
        case 4:
          *type_name = L"int32_t";
          break;
        case 8:
          *type_name = L"int64_t";
          break;

        default:
          return false;
      }
      break;
    }
    case btUInt:
    case btULong: {
      switch (length) {
        case 1:
          *type_name = L"uint8_t";
          break;
        case 2:
          *type_name = L"uint16_t";
          break;
        case 4:
          *type_name = L"uint32_t";
          break;
        case 8:
          *type_name = L"uint64_t";
          break;

        default:
          return false;
      }
      break;
    }

    case btFloat:
      *type_name = L"float";
      break;
    case btBCD:
      *type_name = L"BCD";
      break;
    case btBool:
      *type_name = L"bool";
      break;
    case btCurrency:
      *type_name = L"Currency";
      break;
    case btDate:
      *type_name = L"Date";
      break;
    case btVariant:
      *type_name = L"Variant";
      break;
    case btComplex:
      *type_name = L"Complex";
      break;
    case btBit:
      *type_name = L"Bit";
      break;
    case btBSTR:
      *type_name = L"BSTR";
      break;
    case btHresult:
      *type_name = L"HRESULT";
      break;
    default:
      return false;
  }

  return true;
}

bool GetSymTag(IDiaSymbol* symbol, uint32_t* sym_tag) {
  DCHECK(symbol); DCHECK(sym_tag);

  DWORD tmp = 0;
  HRESULT hr = symbol->get_symTag(&tmp);
  if (hr != S_OK)
    return false;

  *sym_tag = static_cast<uint32_t>(tmp);
  return true;
}

bool IsSymTag(IDiaSymbol* symbol, enum SymTagEnum sym_tag) {
  DCHECK(symbol);

  uint32_t tag = SymTagNull;
  if (!GetSymTag(symbol, &tag))
    return false;

  return static_cast<enum SymTagEnum>(tag) == sym_tag;
}

bool GetSymFlags(IDiaSymbol* symbol, Type::Flags* flags) {
  DCHECK(symbol); DCHECK(flags);
  *flags = 0;
  BOOL is_const = FALSE;
  HRESULT hr = symbol->get_constType(&is_const);
  if (hr != S_OK)
    return false;

  BOOL is_volatile = FALSE;
  hr = symbol->get_volatileType(&is_volatile);
  if (hr != S_OK)
    return false;

  if (is_const)
    *flags |= UserDefinedType::FLAG_CONST;
  if (is_volatile)
    *flags |= UserDefinedType::FLAG_VOLATILE;

  return true;
}

bool GetSymLocType(IDiaSymbol* symbol, uint32_t* loc_type) {
  DCHECK(symbol); DCHECK(loc_type);

  DWORD temp = 0;
  HRESULT hr = symbol->get_locationType(&temp);
  if (hr != S_OK)
    return false;

  *loc_type = static_cast<uint32_t>(temp);
  return true;
}

bool GetSymName(IDiaSymbol* symbol, base::string16* name) {
  DCHECK(symbol); DCHECK(name);
  base::win::ScopedBstr tmp;
  HRESULT hr = symbol->get_name(tmp.Receive());
  if (hr != S_OK)
    return false;

  *name = tmp ? tmp : L"";
  return true;
}

bool GetSymSize(IDiaSymbol* symbol, size_t* size) {
  DCHECK(symbol); DCHECK(size);

  ULONGLONG length = 0;
  HRESULT hr = symbol->get_length(&length);
  if (hr != S_OK)
    return false;

  *size = static_cast<size_t>(length);
  return true;
}

bool GetSymBitPos(IDiaSymbol* symbol, size_t* bit_position) {
  DCHECK(symbol); DCHECK(bit_position);

  DWORD temp = 0;
  HRESULT hr = symbol->get_bitPosition(&temp);
  if (hr != S_OK)
    return false;

  *bit_position = static_cast<size_t>(temp);
  return true;
}

bool GetSymOffset(IDiaSymbol* symbol, ptrdiff_t* offset) {
  DCHECK(symbol); DCHECK(offset);

  LONG tmp = 0;
  HRESULT hr = symbol->get_offset(&tmp);
  if (hr != S_OK)
    return false;

  *offset = static_cast<ptrdiff_t>(tmp);
  return true;
}

bool GetSymDataKind(IDiaSymbol* symbol, DataKind* data_kind) {
  DCHECK(symbol); DCHECK(data_kind);

  DWORD tmp = 0;
  HRESULT hr = symbol->get_dataKind(&tmp);
  if (hr != S_OK)
    return false;

  *data_kind = static_cast<DataKind>(tmp);
  return true;
}

bool GetSymType(IDiaSymbol* symbol,
                base::win::ScopedComPtr<IDiaSymbol>* type) {
  DCHECK(symbol); DCHECK(type);
  base::win::ScopedComPtr<IDiaSymbol> tmp;
  HRESULT hr = symbol->get_type(tmp.Receive());
  if (hr != S_OK)
    return false;

  *type = tmp;
  return true;
}

bool GetSymIndexId(IDiaSymbol* symbol, DWORD* index_id) {
  DCHECK(symbol); DCHECK(index_id);
  DWORD tmp = 0;
  HRESULT hr = symbol->get_symIndexId(&tmp);
  if (!SUCCEEDED(hr))
    return false;
  *index_id = tmp;
  return true;
}

class TypeCreator {
 public:
  explicit TypeCreator(TypeRepository* repository);

  // Crawls @p global, creates all types and assigns names to pointers.
  bool CreateTypes(IDiaSymbol* global);

 private:
  bool CreateTypesOfKind(enum SymTagEnum kind, IDiaSymbol* global);

  // Assigns names to all pointer types that have been created.
  bool AssignPointerNames();
  bool AssignPointerName(PointerTypePtr ptr);

  // Finds or creates the type corresponding to @p symbol.
  // The type will be registered by a unique name in @p existing_types_.
  TypePtr FindOrCreateType(IDiaSymbol* symbol);

  TypePtr CreateType(IDiaSymbol* symbol);
  TypePtr CreateUDT(IDiaSymbol* symbol);
  TypePtr CreateEnum(IDiaSymbol* symbol);
  TypePtr CreateFunctionType(IDiaSymbol* symbol);
  TypePtr CreateBaseType(IDiaSymbol* symbol);
  TypePtr CreatePointerType(IDiaSymbol* symbol);
  TypePtr CreateTypedefType(IDiaSymbol* symbol);
  TypePtr CreateArrayType(IDiaSymbol* symbol);

  bool FinalizeUDT(IDiaSymbol* symbol, UserDefinedTypePtr udt);
  bool FinalizePointer(IDiaSymbol* symbol, PointerTypePtr ptr);
  bool FinalizeType(IDiaSymbol* symbol, TypePtr type);

  struct CreatedType {
    CreatedType() : type_id(kNoTypeId), is_finalized(false) {
    }

    TypeId type_id;
    bool is_finalized;
  };
  typedef base::hash_map<DWORD, CreatedType> CreatedTypeMap;

  // Maps from DIA symbol index ID to the created TypeId. Also keeps a flag
  // that's set when a type is finalized, as DIA has a nasty habit of
  // enumerating the same type multiple times.
  CreatedTypeMap created_types_;
  TypeRepository* repository_;
};

TypeCreator::TypeCreator(TypeRepository* repository)
    : repository_(repository) {
  DCHECK(repository);
}

bool TypeCreator::CreateTypesOfKind(enum SymTagEnum kind,
                                    IDiaSymbol* global) {
  base::win::ScopedComPtr<IDiaEnumSymbols> matching_types;
  HRESULT hr = global->findChildren(kind,
                                    nullptr,
                                    nsNone,
                                    matching_types.Receive());
  if (!SUCCEEDED(hr))
    return false;

  LONG count = 0;
  hr = matching_types->get_Count(&count);
  if (!SUCCEEDED(hr))
    return false;

  for (LONG i = 0; i < count; ++i) {
    base::win::ScopedComPtr<IDiaSymbol> symbol;

    ULONG received = 0;
    hr = matching_types->Next(1, symbol.Receive(), &received);
    if (!SUCCEEDED(hr))
      return false;

    scoped_refptr<Type> type = FindOrCreateType(symbol.get());
    if (!type)
      return false;

    if (!FinalizeType(symbol.get(), type))
      return false;
  }

  return true;
}

bool TypeCreator::FinalizeType(IDiaSymbol* symbol, TypePtr type) {
  // See whether this type needs finalizing.
  DWORD index_id = 0;
  if (!GetSymIndexId(symbol, &index_id))
    return false;

  DCHECK_EQ(type->type_id(), created_types_[index_id].type_id);
  if (created_types_[index_id].is_finalized) {
    // This is a re-visit of the same type. DIA has a nasty habit of doing
    // this, e.g. yielding the same type multiple times in an iteration.
    return true;
  }

  created_types_[index_id].is_finalized = true;

  switch (type->kind()) {
    case Type::USER_DEFINED_TYPE_KIND: {
      UserDefinedTypePtr udt;
      if (!type->CastTo(&udt))
        return false;

      return FinalizeUDT(symbol, udt);
    }

    case Type::POINTER_TYPE_KIND: {
      PointerTypePtr ptr;
      if (!type->CastTo(&ptr))
        return false;

      return FinalizePointer(symbol, ptr);
    }

    default:
      return true;
  }
}

bool TypeCreator::CreateTypes(IDiaSymbol* global) {
  if (!CreateTypesOfKind(SymTagUDT, global) ||
      !CreateTypesOfKind(SymTagEnum, global) ||
      !CreateTypesOfKind(SymTagTypedef, global) ||
      !CreateTypesOfKind(SymTagPointerType, global)) {
    return false;
  }

  return AssignPointerNames();
}

bool TypeCreator::AssignPointerNames() {
  for (auto it : *repository_) {
    if (it->kind() == Type::POINTER_TYPE_KIND && it->name().empty()) {
      PointerTypePtr ptr;
      if (!it->CastTo(&ptr))
        return false;

      if (!AssignPointerName(ptr))
        return false;

      DCHECK_NE(L"", ptr->name());
    }
  }

  return true;
}

bool TypeCreator::AssignPointerName(PointerTypePtr ptr) {
  base::string16 name;
  TypePtr content_type = ptr->GetContentType();
  if (content_type) {
    // Recurse on the content type if it's a pointer with an unassigned name.
    if (content_type->name().empty() &&
        content_type->kind() == Type::POINTER_TYPE_KIND) {
      PointerTypePtr contained_ptr;
      if (!content_type->CastTo(&contained_ptr) ||
          !AssignPointerName(contained_ptr)) {
        return false;
      }
    }

    name = content_type->name();
  }
  if (ptr->is_const())
    name.append(L" const");
  if (ptr->is_volatile())
    name.append(L" volatile");
  name.append(L"*");

  ptr->SetName(name);
  return true;
}

TypePtr TypeCreator::FindOrCreateType(IDiaSymbol* symbol) {
  DCHECK(symbol);

  DWORD index_id = 0;
  if (!GetSymIndexId(symbol, &index_id))
    return false;

  auto it = created_types_.find(index_id);
  if (it != created_types_.end())
    return repository_->GetType(it->second.type_id);

  // Note that this will recurse on pointer types, but the recursion should
  // terminate on a basic type or a UDT at some point - assuming the type
  // graph is sane.
  // TODO(siggi): It'd be better never to recurse, and this can be avoided for
  //    pointers by doing two-phase construction on them as for UDTs. To assign
  //    unique, human-readable names to pointers requires another pass yet.
  TypePtr created = CreateType(symbol);
  DCHECK(created);
  CreatedType& entry = created_types_[index_id];
  entry.type_id = repository_->AddType(created);
  entry.is_finalized = false;

  return created;
}

TypePtr TypeCreator::CreateType(IDiaSymbol* symbol) {
  DCHECK(symbol);

  uint32_t sym_tag = SymTagNull;
  if (!GetSymTag(symbol, &sym_tag))
    return nullptr;

  switch (sym_tag) {
    case SymTagUDT:
      return CreateUDT(symbol);
    case SymTagEnum:
      return CreateEnum(symbol);
    case SymTagBaseType:
      return CreateBaseType(symbol);
    case SymTagFunctionType:
      return CreateFunctionType(symbol);
    case SymTagPointerType:
      return CreatePointerType(symbol);
    case SymTagTypedef:
      return CreateTypedefType(symbol);
    case SymTagArrayType:
      return CreateArrayType(symbol);
    case SymTagVTableShape:
      return new WildcardType(L"VTableShape", 0);
    case SymTagVTable:
      return new WildcardType(L"VTable", 0);
    default:
      return nullptr;
  }
}

TypePtr TypeCreator::CreateUDT(IDiaSymbol* symbol) {
  DCHECK(symbol);
  DCHECK(IsSymTag(symbol, SymTagUDT));

  base::string16 name;
  size_t size = 0;
  if (!GetSymName(symbol, &name) || !GetSymSize(symbol, &size))
    return nullptr;

  return new UserDefinedType(name, size);
}

TypePtr TypeCreator::CreateEnum(IDiaSymbol* symbol) {
  DCHECK(symbol);
  DCHECK(IsSymTag(symbol, SymTagEnum));

  base::string16 name;
  size_t size = 0;
  if (!GetSymName(symbol, &name) || !GetSymSize(symbol, &size))
    return nullptr;

  // TODO(siggi): Implement an enum type.
  return new WildcardType(name, size);
}

bool TypeCreator::FinalizeUDT(IDiaSymbol* symbol, UserDefinedTypePtr udt) {
  DCHECK(symbol);
  DCHECK(udt);
  DCHECK(IsSymTag(symbol, SymTagUDT));

  // Enumerate the fields and add them.
  base::win::ScopedComPtr<IDiaEnumSymbols> enum_children;
  HRESULT hr = symbol->findChildren(
      SymTagNull, NULL, nsNone, enum_children.Receive());
  if (!SUCCEEDED(hr))
    return false;

  LONG count = 0;
  hr = enum_children->get_Count(&count);
  if (!SUCCEEDED(hr))
    return false;

  UserDefinedType::Fields fields;
  for (LONG i = 0; i < count; ++i) {
    base::win::ScopedComPtr<IDiaSymbol> field_sym;
    hr = enum_children->Item(i, field_sym.Receive());
    if (!SUCCEEDED(hr))
      return false;

    uint32_t sym_tag = 0;
    if (!GetSymTag(field_sym.get(), &sym_tag))
      return false;

    // We only care about data.
    if (sym_tag != SymTagData)
      continue;

    // TODO(siggi): Also process VTables?
    DataKind data_kind = DataIsUnknown;
    if (!GetSymDataKind(field_sym.get(), &data_kind))
      return false;
    // We only care about member data.
    if (data_kind != DataIsMember)
      continue;

    // The location udt and the symbol udt are a little conflated in the case
    // of bitfields. For bitfieds, the bit length and bit offset of the udt
    // are stored against the data symbol, and not its udt.
    uint32_t loc_type = LocIsNull;
    if (!GetSymLocType(field_sym.get(), &loc_type))
      return false;
    DCHECK(loc_type == LocIsThisRel || loc_type == LocIsBitField);

    base::win::ScopedComPtr<IDiaSymbol> field_type_sym;
    base::string16 field_name;
    ptrdiff_t field_offset = 0;
    size_t field_size = 0;
    Type::Flags field_flags = 0;
    if (!GetSymType(field_sym.get(), &field_type_sym) ||
        !GetSymName(field_sym.get(), &field_name) ||
        !GetSymOffset(field_sym.get(), &field_offset) ||
        !GetSymSize(field_type_sym.get(), &field_size) ||
        !GetSymFlags(field_type_sym.get(), &field_flags)) {
      return false;
    }

    TypePtr field_type;
    field_type = FindOrCreateType(field_type_sym.get());
    size_t bit_length = 0;
    size_t bit_pos = 0;
    if (loc_type == LocIsBitField) {
      // For bitfields we need the bit size and length.
      if (!GetSymSize(field_sym.get(), &bit_length) ||
          !GetSymBitPos(field_sym.get(), &bit_pos)) {
        return false;
      }
    } else if (loc_type != LocIsThisRel) {
      NOTREACHED() << "Impossible location udt!";
    }

    fields.push_back(UserDefinedType::Field(field_name, field_offset,
                                            field_flags, bit_pos, bit_length,
                                            field_type->type_id()));
  }

  DCHECK_EQ(0UL, udt->fields().size());
  udt->Finalize(fields);
  return true;
}

bool TypeCreator::FinalizePointer(IDiaSymbol* symbol, PointerTypePtr ptr) {
  DCHECK(symbol);
  DCHECK(ptr);
  DCHECK(IsSymTag(symbol, SymTagPointerType));

  base::win::ScopedComPtr<IDiaSymbol> contained_type_sym;
  if (!GetSymType(symbol, &contained_type_sym))
    return false;

  Type::Flags flags = 0;
  if (!GetSymFlags(contained_type_sym.get(), &flags))
    return false;

  TypePtr contained_type = FindOrCreateType(contained_type_sym.get());
  if (!contained_type)
    return false;

  ptr->Finalize(flags, contained_type->type_id());
  return true;
}

TypePtr TypeCreator::CreateBaseType(IDiaSymbol* symbol) {
  // Note that the void base type has zero size.
  DCHECK(symbol);
  DCHECK(IsSymTag(symbol, SymTagBaseType));

  base::string16 base_type_name;
  size_t size = 0;
  if (!GetSymBaseTypeName(symbol, &base_type_name) ||
      !GetSymSize(symbol, &size)) {
    return nullptr;
  }

  return new BasicType(base_type_name, size);
}

TypePtr TypeCreator::CreateFunctionType(IDiaSymbol* symbol) {
  DCHECK(symbol);
  DCHECK(IsSymTag(symbol, SymTagFunctionType));

  return new WildcardType(L"Function", 0);
}

TypePtr TypeCreator::CreatePointerType(IDiaSymbol* symbol) {
  // Note that the void base type has zero size.
  DCHECK(symbol);
  DCHECK(IsSymTag(symbol, SymTagPointerType));

  size_t size = 0;
  if (!GetSymSize(symbol, &size))
    return nullptr;

  return new PointerType(size);
}

TypePtr TypeCreator::CreateTypedefType(IDiaSymbol* symbol) {
  DCHECK(symbol);
  DCHECK(IsSymTag(symbol, SymTagTypedef));

  base::string16 name;
  if (!GetSymName(symbol, &name))
    return nullptr;

  // TODO(siggi): Implement a typedef type.
  return new WildcardType(name, 0);
}

TypePtr TypeCreator::CreateArrayType(IDiaSymbol* symbol) {
  DCHECK(symbol);
  DCHECK(IsSymTag(symbol, SymTagArrayType));

  base::string16 name;
  size_t size = 0;
  if (!GetSymName(symbol, &name) ||
      !GetSymSize(symbol, &size)) {
    return nullptr;
  }

  // TODO(siggi): Implement an array type.
  return new WildcardType(name, size);
}

}  // namespace

DiaCrawler::DiaCrawler() {
}

DiaCrawler::~DiaCrawler() {
}

bool DiaCrawler::InitializeForFile(const base::FilePath& path) {
  base::win::ScopedComPtr<IDiaDataSource> source;
  if (!pe::CreateDiaSource(source.Receive()))
    return false;

  base::win::ScopedComPtr<IDiaSession> session;
  if (!pe::CreateDiaSession(path, source.get(), session.Receive()))
    return false;

  HRESULT hr = session->get_globalScope(global_.Receive());
  if (!SUCCEEDED(hr) || !global_)
    return false;

  source_ = source;
  session_ = session;

  return true;
}

bool DiaCrawler::GetTypes(TypeRepository* types) {
  DCHECK(types); DCHECK(global_);

  // For each type in the PDB:
  //   Create a unique name for the type.
  //   Find or create the type by its unique name.
  //   Finalize the type, e.g.
  //     For each relevant "child" of the type.
  //       Create a unique name for the child.
  //       Find or create the child by its unique name.
  TypeCreator creator(types);

  return creator.CreateTypes(global_.get());
}

}  // namespace refinery
