// Copyright (C) 2016 iNuron NV
//
// This file is part of Open vStorage Open Source Edition (OSE),
// as available from
//
//      http://www.openvstorage.org and
//      http://www.openvstorage.com.
//
// This file is free software; you can redistribute it and/or modify it
// under the terms of the GNU Affero General Public License v3 (GNU AGPLv3)
// as published by the Free Software Foundation, in version 3 as it comes in
// the LICENSE.txt file of the Open vStorage OSE distribution.
// Open vStorage is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY of any kind.

#ifndef FILE_UTILS_H_
#define FILE_UTILS_H_

#include "Catchers.h"
#include "CheckSum.h"
#include "Logging.h"
#include "IOException.h"
#include "BooleanEnum.h"
#include <boost/filesystem.hpp>
#include <boost/scope_exit.hpp>

#include <string>

struct statvfs;

namespace youtils
{
namespace fs = boost::filesystem;
BOOLEAN_ENUM(SyncFileBeforeRename)
BOOLEAN_ENUM(ForceFileSize)

class FileUtils
{
public:
    MAKE_EXCEPTION(Exception, fungi::IOException);
    MAKE_EXCEPTION(CouldNotCreateDirectoryException, FileUtils::Exception);
    MAKE_EXCEPTION(NotADirectoryException, FileUtils::Exception);
    MAKE_EXCEPTION(NotAFileException, FileUtils::Exception);
    MAKE_EXCEPTION(CopyException, FileUtils::Exception);
    MAKE_EXCEPTION(CopyNoSourceException, FileUtils::CopyException);

    DECLARE_LOGGER("FileUtils");

    // Return a path that should be used as temp directory
    static fs::path
    temp_path();

    // Return a path underneath $TMP with the given subdir
    static boost::filesystem::path
    temp_path(const boost::filesystem::path&);

    // Returns a path that points to an existing file file
    static fs::path
    create_temp_file(const fs::path& path,
                     const std::string& name);

    static fs::path
    create_temp_file(const fs::path& path);

    static fs::path
    create_temp_file_in_temp_dir(const std::string& name = "tmpfile");

    static fs::path
    create_temp_dir(const fs::path& path,
                    const std::string& name);

    static fs::path
    create_temp_dir_in_temp_dir(const std::string& name);

    // creates a file if it doesn't exist. Does *not* change times for the
    // file if it already exists.
    static void
    touch(const fs::path& path);

    static void
    make_paths(const std::vector<std::string>& ,
               std::vector<fs::path>& out);

    // new_size must be < than current size, i.e. sparse files will not be
    // created -- it'll throw instead!
    static void
    truncate(const fs::path &,
             const uintmax_t new_size);

    // Make sure directory exists, throws an exception if it there is a problem
    static void
    ensure_directory(const fs::path&);

    static void
    ensure_file(const fs::path&,
                const uint64_t size = 0,
                const ForceFileSize = ForceFileSize::F);


    static void
    safe_copy(const fs::path& src,
              const fs::path& dst,
              const SyncFileBeforeRename = SyncFileBeforeRename::T);

    static void
    safe_copy(const std::string& istr,
              const fs::path& dst,
              const SyncFileBeforeRename = SyncFileBeforeRename::T);

    static uint64_t
    filesystem_size(const fs::path& path);

    static uint64_t
    filesystem_free_size(const fs::path& path);

    static CheckSum
    calculate_checksum(const fs::path& path);

    static CheckSum
    calculate_checksum(const fs::path& path,
                        off_t offset);

    static void
    removeFileNoThrow(const fs::path& path);

    static void
    removeAllNoThrow(const fs::path& path);

    static void
    checkDirectoryEmptyOrNonExistant(const fs::path& path);

    static void
    syncAndRename(const fs::path& old_name,
                  const fs::path& new_name);

    template<typename Op>
    static void
    with_temp_dir(const fs::path& p, Op&& op)
    {
        checkDirectoryEmptyOrNonExistant(p);
        BOOST_SCOPE_EXIT_TPL((&p))
        {
            try
            {
                fs::remove_all(p);
            }
            CATCH_STD_ALL_LOG_IGNORE("Failed to remove temp directory " << p);
        }
        BOOST_SCOPE_EXIT_END;

        op(p);
    }

private:
    static void
    statvfs_(const fs::path& path,
             struct statvfs* st);
};

struct CleanedUpFileDeleter
{
    void
    operator()(boost::filesystem::path* p) const
    {
        youtils::FileUtils::removeFileNoThrow(*p);
        delete p;
    }
};


typedef std::unique_ptr<boost::filesystem::path,
                        CleanedUpFileDeleter> CleanedUpFile;

MAKE_EXCEPTION(CheckSumFileNotLargeEnough, fungi::IOException);
}

#define ALWAYS_CLEANUP_FILE(file)                               \
    BOOST_SCOPE_EXIT((&file))                                   \
    {                                                           \
        youtils::FileUtils::removeFileNoThrow(file);            \
    } BOOST_SCOPE_EXIT_END                                      \

#define ALWAYS_CLEANUP_FILE_TPL(file)                           \
    BOOST_SCOPE_EXIT_TPL((&file))                               \
    {                                                           \
        youtils::FileUtils::removeFileNoThrow(file);            \
    } BOOST_SCOPE_EXIT_END                                      \


#define ALWAYS_CLEANUP_DIRECTORY(dir)                           \
    BOOST_SCOPE_EXIT((&dir))                                    \
    {                                                           \
        youtils::FileUtils::removeAllNoThrow(dir);              \
    } BOOST_SCOPE_EXIT_END

#define ALWAYS_CLEANUP_DIRECTORY_TPL(dir)                       \
    BOOST_SCOPE_EXIT_TPL((&dir))                                \
    {                                                           \
        youtils::FileUtils::removeAllNoThrow(dir);              \
    } BOOST_SCOPE_EXIT_END

#endif // FILE_UTILS_H_

// Local Variables: **
// mode: c++ **
// End: **
