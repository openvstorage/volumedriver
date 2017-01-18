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

#ifndef _FILE_H
#define	_FILE_H

#include "Readable.h"
#include "Streamable.h"
#include "Writable.h"

#include <string>
#include <memory>
#include <list>

#include <boost/optional.hpp>

// TODO AR: get rid of this - use youtils::FileDescriptor instead for example.
namespace fungi {

    class ByteArray;

    // losely based on RandomAccessFile from java
class File
    : public Streamable
    , public Readable
    , public Writable
{
    public:
        enum {
            Read = 1,
            Write = 2,
            Append = 4,
            /**
             * updates an already existing file(with read and write support);
             * no new file will be created if no one already exists
             */
            Update = 8
        };

        explicit File(const std::string &filename, int mode = Read);

        File(const File&) = delete;

        File& operator=(const File&) = delete;

        virtual ~File();

        void open(boost::optional<size_t> bufsize = boost::none); // system default

        void close() override final; // throw IOException
        void closeNoThrow() override final;

        bool isClosed() const override final
        {
            return f_ == nullptr;
        }

        //just for compatibility with Sockets. (Made to make abstraction on IOBaseStream)
        void setCork() override final {}
        void clearCork() override final {}
        void getCork() override final {}
        void setRequestTimeout(double) override final {}

        /** @exception IOException */
        int32_t read(byte *ptr,
                     const int32_t count) override final;

        /** @exception IOException */
        byte peek();

        // AR: which one is this? Writable? Streamable? Sigh.
        /** @exception IOException */
        int32_t write(const byte *ptr, int32_t count) override final;

        /** @exception IOException */
        int32_t write(const ByteArray &data);

        /** @exception IOException */
        int64_t length() const;

        /** @exception IOException */
        void flush();

        /** @exception IOException */
        void sync();

        /** @exception IOException */
        void datasync();

        /** @exception IOException */
        static int64_t length(const std::string &filename);

        int fileno() const;
        FILE *getHandle() const;

        bool eof() const;

        /** @exception IOException */
        void rewind();

        /** @exception IOException */
        void seek(int64_t offset,
                  int WHENCE = SEEK_SET);

        /** @exception IOException */
        int64_t tell() const;

        /** @exception IOException */
        void truncate(int64_t length);

        const std::string &name() const;

        size_t
        saneRead(void* ptr, size_t size, size_t count);

    private:
        DECLARE_LOGGER("File");

        const std::string filename_;
        FILE *f_;
        int mode_;

        std::unique_ptr<char[]> buf_;
    };
}


#endif	/* _FILE_H */

// Local Variables: **
// End: **
