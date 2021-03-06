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
//
// Declares a class for writing an archive of COFF object files to a .lib
// file. See ar_reader.h for details of MSVS version of the file format.

#include "syzygy/ar/ar_writer.h"

#include <windows.h>
#include <sys/stat.h>

#include "base/logging.h"
#include "base/sys_byteorder.h"
#include "base/files/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "syzygy/common/align.h"
#include "syzygy/common/buffer_parser.h"
#include "syzygy/common/buffer_writer.h"
#include "syzygy/core/file_util.h"

namespace ar {

namespace {

typedef ArWriter::FileVector FileVector;

// Contains a list of file offsets at which each file starts in the archive.
typedef std::vector<uint32> FileOffsets;

// Determines if a symbol should be added to the symbol table. The rules as to
// what symbols should be exported has been derived by observation of inputs
// and outputs to lib.exe, guided by available documentation.
bool ShouldAddSymbolToTable(const IMAGE_SYMBOL& symbol, bool* is_weak) {
  DCHECK_NE(reinterpret_cast<bool*>(NULL), is_weak);

  *is_weak = false;

  switch (symbol.StorageClass) {
    case IMAGE_SYM_CLASS_EXTERNAL: {
      if (symbol.SectionNumber == 0 && symbol.Type == 0 && symbol.Value > 0) {
        *is_weak = true;
        return true;
      }
      if ((symbol.SectionNumber == -1 || symbol.SectionNumber > 0) &&
          symbol.NumberOfAuxSymbols == 0) {
        return true;
      }
      break;
    }

    case IMAGE_SYM_CLASS_WEAK_EXTERNAL: {
      if (symbol.SectionNumber == 0 && symbol.Type == 0 &&
          symbol.NumberOfAuxSymbols == 1) {
        *is_weak = true;
        return true;
      }
      break;
    }

    default:
      break;
  }

  return false;
}

// Updates the symbol table. Returns true if the symbol was a duplicate entry,
// false otherwise. Updates the symbol table with the following logic:
//
// - The first non-weak definition of a symbol wins.
// - The first weak definition of a symbol with no non-weak definitions wins.
bool UpdateSymbolTable(uint32 file_index,
                       const base::StringPiece& name,
                       bool is_weak,
                       SymbolIndexMap* symbols,
                       SymbolIndexMap* weak_symbols) {
  SymbolIndexMap::value_type value = std::make_pair(name.as_string(),
                                                    file_index);
  SymbolIndexMap::iterator it = symbols->find(value.first);
  SymbolIndexMap::iterator weak_it = weak_symbols->find(value.first);

  if (is_weak) {
    // Case 1: Weak symbol appears, nothing with same name in table.
    //         Symbol should be added to table.
    if (it == symbols->end()) {
      DCHECK(weak_it == weak_symbols->end());
      CHECK(symbols->insert(value).second);
      CHECK(weak_symbols->insert(value).second);
      return false;
    }

    // Case 2: Weak symbol appears, weak symbol already in table.
    //         Symbol should be ignored, marked as duplicate.
    if (weak_it != weak_symbols->end()) {
      DCHECK(it != symbols->end());
      DCHECK(*it == *weak_it);
      return true;
    }

    // Case 3: Weak symbol appears, non-weak symbol already in table.
    //         Symbol should be ignored, marked as duplicate.
    DCHECK(it != symbols->end());
    DCHECK(weak_it == weak_symbols->end());
    return true;
  }
  DCHECK(!is_weak);

  // Case 4: Non-weak symbol appears, nothing with same name in table.
  //         Symbol should be added to table.
  if (it == symbols->end()) {
    DCHECK(weak_it == weak_symbols->end());
    CHECK(symbols->insert(value).second);
    return false;
  }

  // Case 5: Non-weak symbol appears, weak symbol already in table.
  //         Symbol should replace weak symbol, marked as duplicate.
  if (weak_it != weak_symbols->end()) {
    DCHECK(it != symbols->end());
    DCHECK(*it == *weak_it);
    it->second = file_index;
    weak_symbols->erase(weak_it);
    return true;
  }

  // Case 6: Non-weak symbol appears, non-weak symbol already in table.
  //         Symbol should be ignored, marked as duplicate.
  DCHECK(it != symbols->end());
  DCHECK(weak_it == weak_symbols->end());
  return true;
}

// Extracts exported symbol names from the given COFF object file, adding them
// to |symbols|. Returns true on success, false otherwise. This contains some
// code that is similar to what is found in CoffImage or CoffDecomposer, but
// using those classes is a little overkill for our purposes.
bool ExtractSymbolsCoff(uint32 file_index,
                        const ParsedArFileHeader& header,
                        const DataBuffer& file_contents,
                        SymbolIndexMap* symbols,
                        SymbolIndexMap* weak_symbols) {
  DCHECK_NE(reinterpret_cast<SymbolIndexMap*>(NULL), symbols);
  DCHECK_NE(reinterpret_cast<SymbolIndexMap*>(NULL), weak_symbols);

  common::BinaryBufferReader reader(file_contents.data(),
                                    file_contents.size());
  const IMAGE_FILE_HEADER* file_header = NULL;
  if (!reader.Read(&file_header))
    return false;

  // Object files should never contain an optional header.
  if (file_header->SizeOfOptionalHeader != 0) {
    LOG(ERROR) << "Unrecognized object file: " << header.name;
    return false;
  }

  // If there are no symbols then there's no work to be done.
  if (file_header->NumberOfSymbols == 0)
    return true;

  // Get the string table offset.
  size_t string_table_offset = file_header->PointerToSymbolTable +
      file_header->NumberOfSymbols * sizeof(IMAGE_SYMBOL);

  // Keep track of how many symbols have already been defined.
  size_t duplicate_symbols = 0;

  // Parse the symbols.
  reader.set_pos(file_header->PointerToSymbolTable);
  const IMAGE_SYMBOL* symbol = NULL;
  for (size_t i = 0; i < file_header->NumberOfSymbols;
       i += 1 + symbol->NumberOfAuxSymbols) {
    if (!reader.Read(&symbol) ||
        !reader.Consume(sizeof(IMAGE_AUX_SYMBOL) *
                            symbol->NumberOfAuxSymbols)) {
      LOG(ERROR) << "Failed to read symbol " << i << " of object file: "
                 << header.name;
      return false;
    }

    // Filter out symbols that don't belong in the symbol table.
    bool is_weak = false;
    if (!ShouldAddSymbolToTable(*symbol, &is_weak))
      continue;

    // Get the symbol name.
    base::StringPiece name;
    {
      const char* s = NULL;
      size_t max_len = 0;
      if (symbol->N.Name.Short == 0) {
        if (symbol->N.Name.Long >= file_contents.size()) {
          LOG(ERROR) << "Invalid symbol name pointer in object file: "
                     << header.name;
          return false;
        }
        size_t offset = string_table_offset + symbol->N.Name.Long;
        s = reinterpret_cast<const char*>(file_contents.data()) +
            offset;
        max_len = file_contents.size() - offset;
      } else {
        s = reinterpret_cast<const char*>(symbol->N.ShortName);
        max_len = sizeof(symbol->N.ShortName);
      }
      size_t len = ::strnlen(s, max_len);
      name = base::StringPiece(s, len);
    }

    // Update the symbol tables with this symbol, keeping track of whether or
    // not this was a duplicate symbol name.
    if (UpdateSymbolTable(file_index, name, is_weak, symbols, weak_symbols))
      ++duplicate_symbols;
  }

  if (duplicate_symbols) {
    LOG(INFO) << "Ignored " << duplicate_symbols
              << " duplicate symbols in object file: " << header.name;
  }

  return true;
}

// Extracts the symbol name from the given COFF import definition, adding it to
// |symbols|. Returns true on success, false otherwise.
bool ExtractSymbolsImportDef(uint32 file_index,
                             const ParsedArFileHeader& header,
                             const DataBuffer& file_contents,
                             SymbolIndexMap* symbols,
                             SymbolIndexMap* weak_symbols) {
  DCHECK_NE(reinterpret_cast<SymbolIndexMap*>(NULL), symbols);
  DCHECK_NE(reinterpret_cast<SymbolIndexMap*>(NULL), weak_symbols);

  common::BinaryBufferReader reader(file_contents.data(),
                                    file_contents.size());
  const IMPORT_OBJECT_HEADER* import = NULL;
  if (!reader.Read(&import))
    return false;

  const char* name = NULL;
  size_t size = 0;
  if (!reader.ReadString(&name, &size))
    return false;

  std::string imp_name("__imp_");
  imp_name += name;

  bool is_duplicate = false;
  if (UpdateSymbolTable(file_index, name, false, symbols, weak_symbols))
    is_duplicate = true;
  if (UpdateSymbolTable(file_index, imp_name, false, symbols, weak_symbols))
    is_duplicate = true;

  if (is_duplicate) {
    LOG(INFO) << "Ignored duplicate symbol \"" << name
              << "\" from import definition file: " << header.name;
  }

  return true;
}

// Extracts symbols from the given file. If the file is not of a recognized
// type, then this does nothing.
bool ExtractSymbols(uint32 file_index,
                    const ParsedArFileHeader& header,
                    const DataBuffer& file_contents,
                    SymbolIndexMap* symbols,
                    SymbolIndexMap* weak_symbols) {
  core::FileType file_type = core::kUnknownFileType;
  if (!core::GuessFileType(file_contents.data(), file_contents.size(),
                           &file_type)) {
    LOG(ERROR) << "Unable to determine file type: " << header.name;
    return false;
  }

  switch (file_type) {
    case core::kCoffFileType:
    case core::kCoff64FileType: {
      if (!ExtractSymbolsCoff(file_index, header, file_contents, symbols,
                              weak_symbols)) {
        return false;
      }
      break;
    }

    case core::kImportDefinitionFileType: {
      if (!ExtractSymbolsImportDef(file_index, header, file_contents, symbols,
                                   weak_symbols)) {
        return false;
      }
      break;
    }

    // Files that we recognize but don't have to process.
    case core::kResourceFileType: {
      break;
    }

    // We don't know how to process anonymous COFF files, so can't extract
    // symbol information.
    case core::kAnonymousCoffFileType: {
      LOG(ERROR) << "Unable to extract symbols from anonymous COFF object: "
                 << header.name;
      return false;
    }

    case core::kUnknownFileType: {
      LOG(ERROR) << "Unable to add file of unknown type to archive: "
                 << header.name;
      return false;
    }

    default: {
      LOG(ERROR) << "Unable to add file of invalid type to archive: "
                 << header.name;
      return false;
    }
  }

  return true;
}

// Fills in a raw ArFileHeader with the data from |parsed_header|.
bool PopulateArFileHeader(const ParsedArFileHeader& parsed_header,
                          ArFileHeader* raw_header) {
  DCHECK_NE(reinterpret_cast<ArFileHeader*>(NULL), raw_header);

  // Convert value types.
  std::string timestamp = base::StringPrintf(
      "%lld", static_cast<uint64>(parsed_header.timestamp.ToDoubleT()));
  std::string mode = base::StringPrintf("%d", parsed_header.mode);
  std::string size = base::StringPrintf("%lld", parsed_header.size);

  // Validate sizes of inputs.
  if (parsed_header.name.size() > sizeof(raw_header->name)) {
    LOG(ERROR) << "Filename too long for ArFileHeader: " << parsed_header.name;
    return false;
  }
  if (timestamp.size() > sizeof(raw_header->timestamp)) {
    LOG(ERROR) << "Timestamp too large for ArFileHeader: " << timestamp;
    return false;
  }
  if (mode.size() > sizeof(raw_header->mode)) {
    LOG(ERROR) << "Mode too large for ArFileHeader: " << mode;
    return false;
  }
  if (size.size() > sizeof(raw_header->size)) {
    LOG(ERROR) << "Size too large for ArFileHeader: " << size;
    return false;
  }

  // Fill the header with spaces.
  ::memset(raw_header, ' ', sizeof(*raw_header));

  // Populate the various fields.
  ::memcpy(raw_header->name, parsed_header.name.c_str(),
           parsed_header.name.size());
  ::memcpy(raw_header->timestamp, timestamp.c_str(), timestamp.size());
  ::memcpy(raw_header->mode, mode.c_str(), mode.size());
  ::memcpy(raw_header->size, size.c_str(), size.size());
  ::memcpy(raw_header->magic, kArFileMagic, sizeof(kArFileMagic));

  return true;
}

// Writes the given file to an archive, prepended by its header.
bool WriteFile(const ArFileHeader& header,
               const DataBuffer& contents,
               FILE* file) {
  DCHECK_NE(reinterpret_cast<FILE*>(NULL), file);

  // Write the header.
  if (::fwrite(&header, sizeof(header), 1, file) != 1) {
    LOG(ERROR) << "Failed to write file header.";
    return false;
  }

  // Write the contents.
  if (::fwrite(contents.data(), 1, contents.size(), file) !=
               contents.size()) {
    LOG(ERROR) << "Failed to write file contents.";
    return false;
  }

  return true;
}

// Writes a primary symbol table using the legacy symbol table format.
bool WritePrimarySymbolTable(const base::Time& timestamp,
                             const SymbolIndexMap& symbols,
                             const FileOffsets& offsets,
                             FILE* file) {
  DCHECK_NE(reinterpret_cast<FILE*>(NULL), file);

  // Invert the symbol map. We require the symbols sorted by
  // increasing offset and not by name.
  typedef std::pair<size_t, std::string> SymbolPair;
  typedef std::vector<SymbolPair> SymbolVector;
  SymbolVector syms;
  syms.reserve(symbols.size());
  SymbolIndexMap::const_iterator sym_it = symbols.begin();
  for (; sym_it != symbols.end(); ++sym_it)
    syms.push_back(std::make_pair(sym_it->second, sym_it->first));
  std::sort(syms.begin(), syms.end());

  // Generate the content.
  DataBuffer buffer;
  common::VectorBufferWriter writer(&buffer);
  CHECK(writer.Write<uint32>(base::ByteSwap(symbols.size())));
  for (size_t i = 0; i < syms.size(); ++i) {
    DCHECK_LE(syms[i].first, offsets.size());
    uint32 offset = offsets[syms[i].first];
    CHECK(writer.Write<uint32>(base::ByteSwap(offset)));
  }
  for (size_t i = 0; i < syms.size(); ++i) {
    CHECK(writer.Write<char>(syms[i].second.size() + 1,
                             syms[i].second.data()));
  }

  // Generate the header and write the content.
  ParsedArFileHeader header;
  header.name = "/";
  header.timestamp = timestamp;
  header.mode = 0;
  header.size = buffer.size();
  ArFileHeader raw_header;
  if (!PopulateArFileHeader(header, &raw_header))
    return false;
  if (!WriteFile(raw_header, buffer, file))
    return false;

  return true;
}

// Writes an MSVS-style symbol table.
bool WriteSecondarySymbolTable(const base::Time& timestamp,
                               const SymbolIndexMap& symbols,
                               const FileOffsets& offsets,
                               FILE* file) {
  DCHECK_NE(reinterpret_cast<FILE*>(NULL), file);

  // Generate the content.
  DataBuffer buffer;
  common::VectorBufferWriter writer(&buffer);
  CHECK(writer.Write<uint32>(offsets.size()));
  CHECK(writer.Write<uint32>(offsets.size(), offsets.data()));
  CHECK(writer.Write<uint32>(symbols.size()));
  SymbolIndexMap::const_iterator sym_it = symbols.begin();
  // File indices are 1 based.
  for (; sym_it != symbols.end(); ++sym_it)
    CHECK(writer.Write<uint16>(sym_it->second + 1));
  for (sym_it = symbols.begin(); sym_it != symbols.end(); ++sym_it) {
    CHECK(writer.Write<char>(sym_it->first.size() + 1,
                             sym_it->first.data()));
  }

  // Generate the header and write the content.
  ParsedArFileHeader header;
  header.name = "/";
  header.timestamp = timestamp;
  header.mode = 0;
  header.size = buffer.size();
  ArFileHeader raw_header;
  if (!PopulateArFileHeader(header, &raw_header))
    return false;
  if (!WriteFile(raw_header, buffer, file))
    return false;

  return true;
}

// Writes an extended name table.
bool WriteNameTable(const base::Time& timestamp,
                    const DataBuffer& names,
                    FILE* file) {
  DCHECK_NE(reinterpret_cast<FILE*>(NULL), file);

  // Populate the header.
  ParsedArFileHeader header;
  header.name = "//";
  header.timestamp = timestamp;
  header.mode = 0;
  header.size = names.size();

  ArFileHeader raw_header;
  if (!PopulateArFileHeader(header, &raw_header))
    return false;

  if (!WriteFile(raw_header, names, file))
    return false;

  return true;
}

// Aligns the file cursor to the alignment required by the archive file
// and returns the aligned cursor position.
uint32 AlignAndGetPosition(FILE* file) {
  DCHECK_NE(reinterpret_cast<FILE*>(NULL), file);

  uint32 pos = ::ftell(file);
  uint32 aligned_pos = common::AlignUp(pos, kArFileAlignment);
  if (aligned_pos != pos) {
    for (size_t i = 0; i < aligned_pos - pos; ++i)
      ::fputc(0, file);
  }

  return aligned_pos;
}

}  // namespace

ArWriter::ArWriter() {
}

bool ArWriter::AddFile(const base::StringPiece& filename,
                       const base::Time& timestamp,
                       uint32 mode,
                       const DataBuffer* contents) {
  DCHECK_NE(reinterpret_cast<DataBuffer*>(NULL), contents);

  if (contents->size() == 0) {
    LOG(ERROR) << "Unable to add empty file to archive: " << filename;
    return false;
  }

  // Try to insert the file into the map. If this fails then there's a
  // collision.
  std::string name = filename.as_string();
  std::pair<FileIndexMap::const_iterator, bool> result =
      file_index_map_.insert(std::make_pair(name, files_.size()));
  CHECK(result.second);
  DCHECK_EQ(files_.size(), result.first->second);

  // Build the file header.
  ParsedArFileHeader header;
  header.name = name;
  header.timestamp = timestamp;
  header.mode = mode;
  header.size = contents->size();

  // Try to parse the symbols from the file. We keep a copy of the
  // symbol tables so as not to corrupt them if the operation fails.
  SymbolIndexMap symbols = symbols_;
  SymbolIndexMap weak_symbols = weak_symbols_;
  if (!ExtractSymbols(files_.size(), header, *contents, &symbols,
                      &weak_symbols)) {
    return false;
  }

  // If all goes well then commit the file to the archive.
  std::swap(symbols_, symbols);
  std::swap(weak_symbols_, weak_symbols);
  files_.push_back(std::make_pair(header, contents));
  return true;
}

bool ArWriter::AddFile(const base::FilePath& path) {
  std::string name = base::WideToUTF8(path.value());

  // Get the mode of the file.
  struct _stat stat;
  if (_wstat(path.value().c_str(), &stat) != 0) {
    LOG(ERROR) << "Unable to get file status: " << path.value();
    return false;
  }
  if (stat.st_size == 0) {
    LOG(ERROR) << "Unable to add empty file to archive: " << path.value();
    return false;
  }

  scoped_ptr<DataBuffer> buffer(new DataBuffer(stat.st_size));
  int read = base::ReadFile(
      path,
      reinterpret_cast<char*>(buffer->data()),
      static_cast<int>(stat.st_size));
  if (read != static_cast<int>(stat.st_size)) {
    LOG(ERROR) << "Unable to read file: " << path.value();
    return false;
  }

  base::Time timestamp = base::Time::FromTimeT(stat.st_mtime);
  uint32 mode = stat.st_mode;
  if (!AddFile(name, timestamp, mode, buffer.get()))
    return false;

  // Transfer ownership of the buffer to the object.
  buffers_.push_back(buffer.release());

  return true;
}

bool ArWriter::Write(const base::FilePath& path) {
  if (files_.empty()) {
    LOG(ERROR) << "Unable to write an empty archive.";
    return false;
  }

  std::vector<ArFileHeader> raw_headers(files_.size());
  DataBuffer names;
  for (size_t i = 0; i < files_.size(); ++i) {
    // Grab a copy of the header because we are going to modify it.
    ParsedArFileHeader header = files_[i].first;
    ArFileHeader& raw_header = raw_headers[i];

    // Translate the filename.
    if (header.name.size() >= sizeof(raw_header.name)) {
      // Copy the extended filename to the name table, with a terminating
      // null.
      size_t offset = names.size();
      names.resize(offset + header.name.size() + 1);
      ::memcpy(names.data() + offset, header.name.data(),
               header.name.size() + 1);

      // Name the file with a reference to the name table.
      header.name = base::StringPrintf("/%d", offset);
    } else {
      // Simply append a trailing '/' to the name.
      header.name += "/";
    }

    // Fill in the raw file header.
    if (!PopulateArFileHeader(header, &raw_header))
      return false;
  }

  // Open the file and write the global header.
  base::ScopedFILE file(base::OpenFile(path, "w+b"));
  if (file.get() == NULL) {
    LOG(ERROR) << "Unable to open file for writing: " << path.value();
    return false;
  }
  if (::fwrite(kArGlobalMagic, sizeof(kArGlobalMagic), 1, file.get()) != 1) {
    LOG(ERROR) << "Failed to write global archive header.";
    return false;
  }

  // Write the symbol tables. We initially use a set of dummy offsets, and
  // reach back and write the actual offsets once we've laid out the object
  // files.
  FileOffsets offsets(files_.size());
  base::Time timestamp = base::Time::Now();
  uint32 symbols1_pos = AlignAndGetPosition(file.get());
  if (!WritePrimarySymbolTable(timestamp, symbols_, offsets, file.get()))
    return false;
  uint32 symbols2_pos = AlignAndGetPosition(file.get());
  if (!WriteSecondarySymbolTable(timestamp, symbols_, offsets, file.get()))
    return false;

  // Write the name table.
  AlignAndGetPosition(file.get());
  if (!WriteNameTable(timestamp, names, file.get()))
    return false;

  // Write the files, keeping track of their offsets.
  for (size_t i = 0; i < files_.size(); ++i) {
    const DataBuffer& buffer = *files_[i].second;
    const ArFileHeader& raw_header = raw_headers[i];

    offsets[i] = AlignAndGetPosition(file.get());
    if (!WriteFile(raw_header, buffer, file.get()))
      return false;
  }

  // Rewrite the symbol streams using the actual file offsets this time around.
  if (::fseek(file.get(), symbols1_pos, SEEK_SET) != 0) {
    LOG(ERROR) << "Failed to seek to primary symbol stream.";
    return false;
  }
  if (!WritePrimarySymbolTable(timestamp, symbols_, offsets, file.get()))
    return false;
  if (::fseek(file.get(), symbols2_pos, SEEK_SET) != 0) {
    LOG(ERROR) << "Failed to seek to secondary symbol stream.";
    return false;
  }
  if (!WriteSecondarySymbolTable(timestamp, symbols_, offsets, file.get()))
    return false;

  return true;
}

}  // namespace ar
