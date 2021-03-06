// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_ZLIB_GOOGLE_ZIP_READER_H_
#define THIRD_PARTY_ZLIB_GOOGLE_ZIP_READER_H_

#if defined(OS_WIN)
#include <Windows.h>
#endif

#include <string>

#if defined(USE_SYSTEM_MINIZIP)
#include <minizip/unzip.h>
#else
#include "third_party/zlib/contrib/minizip/unzip.h"
#endif

#include "srdtk/string/string_macros.h"
#include "third_party/boost/boost/scoped_ptr.hpp"

#if defined(__LP64__) && !defined(OS_MACOSX) && !defined(OS_OPENBSD)
typedef long                int64;
#else
typedef long long           int64;
#endif

namespace base {

// PLATFORM_FILE_ERROR_ACCESS_DENIED is returned when a call fails because of
// a filesystem restriction. PLATFORM_FILE_ERROR_SECURITY is returned when a
// browser policy doesn't allow the operation to be executed.
enum PlatformFileError {
  PLATFORM_FILE_OK = 0,
  PLATFORM_FILE_ERROR_FAILED = -1,
  PLATFORM_FILE_ERROR_IN_USE = -2,
  PLATFORM_FILE_ERROR_EXISTS = -3,
  PLATFORM_FILE_ERROR_NOT_FOUND = -4,
  PLATFORM_FILE_ERROR_ACCESS_DENIED = -5,
  PLATFORM_FILE_ERROR_TOO_MANY_OPENED = -6,
  PLATFORM_FILE_ERROR_NO_MEMORY = -7,
  PLATFORM_FILE_ERROR_NO_SPACE = -8,
  PLATFORM_FILE_ERROR_NOT_A_DIRECTORY = -9,
  PLATFORM_FILE_ERROR_INVALID_OPERATION = -10,
  PLATFORM_FILE_ERROR_SECURITY = -11,
  PLATFORM_FILE_ERROR_ABORT = -12,
  PLATFORM_FILE_ERROR_NOT_A_FILE = -13,
  PLATFORM_FILE_ERROR_NOT_EMPTY = -14,
  PLATFORM_FILE_ERROR_INVALID_URL = -15,
  PLATFORM_FILE_ERROR_IO = -16,
  // Put new entries here and increment PLATFORM_FILE_ERROR_MAX.
  PLATFORM_FILE_ERROR_MAX = -17
};


#if defined(OS_WIN)
typedef HANDLE PlatformFile;
const PlatformFile kInvalidPlatformFileValue = INVALID_HANDLE_VALUE;
PlatformFileError LastErrorToPlatformFileError(DWORD saved_errno);
#elif defined(OS_POSIX)
typedef int PlatformFile;
const PlatformFile kInvalidPlatformFileValue = -1;
PlatformFileError ErrnoToPlatformFileError(int saved_errno);
#endif



} // namespace base

namespace zip {

// This class is used for reading zip files. A typical use case of this
// class is to scan entries in a zip file and extract them. The code will
// look like:
//
//   ZipReader reader;
//   reader.Open(zip_file_path);
//   while (reader.HasMore()) {
//     reader.OpenCurrentEntryInZip();
//     reader.ExtractCurrentEntryToDirectory(output_directory_path);
//     reader.AdvanceToNextEntry();
//   }
//
// For simplicty, error checking is omitted in the example code above. The
// production code should check return values from all of these functions.
//
// This calls can also be used for random access of contents in a zip file
// using LocateAndOpenEntry().
//
class ZipReader {
 public:
  // This class represents information of an entry (file or directory) in
  // a zip file.
  class EntryInfo {
   public:
    EntryInfo(const std::string& filename_in_zip,
              const unz_file_info& raw_file_info);

    // Returns the file path. The path is usually relative like
    // "foo/bar.txt", but if it's absolute, is_unsafe() returns true.
    const srdtk::tstring& file_path() const { return file_path_; }

    // Returns the size of the original file (i.e. after uncompressed).
    // Returns 0 if the entry is a directory.
    int64 original_size() const { return original_size_; }

    // Returns the last modified time.
    uLong last_modified() const { return last_modified_; }

    // Returns true if the entry is a directory.
    bool is_directory() const { return is_directory_; }

    // Returns true if the entry is unsafe, like having ".." or invalid
    // UTF-8 characters in its file name, or the file path is absolute.
    bool is_unsafe() const { return is_unsafe_; }

    const tm_unz& tmu_date() { return tmu_date_; }

   private:
    srdtk::tstring file_path_;
    int64 original_size_;
    uLong last_modified_;
    bool is_directory_;
    bool is_unsafe_;
    tm_unz tmu_date_;

    EntryInfo(const EntryInfo&);
    void operator=(const EntryInfo&);
  };

  ZipReader();
  ~ZipReader();

  // Opens the zip file specified by |zip_file_path|. Returns true on
  // success.
  bool Open(const srdtk::tstring& zip_file_path);

  // Opens the zip file referred to by the platform file |zip_fd|.
  // Returns true on success.
  bool OpenFromPlatformFile(base::PlatformFile zip_fd);

  // Opens the zip data stored in |data|. This class uses a weak reference to
  // the given sring while extracting files, i.e. the caller should keep the
  // string until it finishes extracting files.
  bool OpenFromString(const std::string& data);

  // Closes the currently opened zip file. This function is called in the
  // destructor of the class, so you usually don't need to call this.
  void Close();

  // Returns true if there is at least one entry to read. This function is
  // used to scan entries with AdvanceToNextEntry(), like:
  //
  // while (reader.HasMore()) {
  //   // Do something with the current file here.
  //   reader.AdvanceToNextEntry();
  // }
  bool HasMore();

  // Advances the next entry. Returns true on success.
  bool AdvanceToNextEntry();

  // Opens the current entry in the zip file. On success, returns true and
  // updates the the current entry state (i.e. current_entry_info() is
  // updated). This function should be called before operations over the
  // current entry like ExtractCurrentEntryToFile().
  //
  // Note that there is no CloseCurrentEntryInZip(). The the current entry
  // state is reset automatically as needed.
  bool OpenCurrentEntryInZip();

  // Locates an entry in the zip file and opens it. Returns true on
  // success. This function internally calls OpenCurrentEntryInZip() on
  // success. On failure, current_entry_info() becomes NULL.
  bool LocateAndOpenEntry(const srdtk::tstring& path_in_zip);

  // Extracts the current entry to the given output file path. If the
  // current file is a directory, just creates a directory
  // instead. Returns true on success. OpenCurrentEntryInZip() must be
  // called beforehand.
  //
  // This function does not preserve the timestamp of the original entry.
  bool ExtractCurrentEntryToFilePath(const srdtk::tstring& output_file_path);

  // Extracts the current entry to the given output directory path using
  // ExtractCurrentEntryToFilePath(). Sub directories are created as needed
  // based on the file path of the current entry. For example, if the file
  // path in zip is "foo/bar.txt", and the output directory is "output",
  // "output/foo/bar.txt" will be created.
  //
  // Returns true on success. OpenCurrentEntryInZip() must be called
  // beforehand.
  bool ExtractCurrentEntryIntoDirectory(
      const srdtk::tstring& output_directory_path);

#if defined(OS_POSIX)
  // Extracts the current entry by writing directly to a file descriptor.
  // Does not close the file descriptor. Returns true on success.
  bool ExtractCurrentEntryToFd(int fd);
#endif

  // Returns the current entry info. Returns NULL if the current entry is
  // not yet opened. OpenCurrentEntryInZip() must be called beforehand.
  EntryInfo* current_entry_info() const {
    return current_entry_info_.get();
  }

  // Returns the number of entries in the zip file.
  // Open() must be called beforehand.
  int num_entries() const { return num_entries_; }

  // Returns the current entry index based 1
  int current_entry_index() const { return current_entry_index_; }

 private:
  // Common code used both in Open and OpenFromFd.
  bool OpenInternal();

  // Resets the internal state.
  void Reset();

  unzFile zip_file_;
  int num_entries_;
  int current_entry_index_;
  bool reached_end_;
  boost::scoped_ptr<EntryInfo> current_entry_info_;

  ZipReader(const ZipReader&);
  void operator=(const ZipReader&);
};

}  // namespace zip

#endif  // THIRD_PARTY_ZLIB_GOOGLE_ZIP_READER_H_
