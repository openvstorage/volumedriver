// Copyright 2015 Open vStorage NV
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

#ifndef _FILE_H
#define	_FILE_H

#include "Streamable.h"
#include <string>
#include <memory>
#include <list>

#include "Readable.h"
#include "Writable.h"

namespace fungi {

    class ByteArray;

    // losely based on RandomAccessFile from java
    class File : public Streamable, public Readable, public Writable {
    public:

        DECLARE_LOGGER("File");

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

        File(const std::string &filename,FILE *wrap);
        File(const std::string &filename, int mode = Read);
        virtual ~File();

        static std::unique_ptr<File> getStdin();
        static std::unique_ptr<File> getStdout();
        static std::unique_ptr<File> getStderr();

        virtual void try_open();
        virtual void open();
        void reopen(int mode);
        void close(); // throw IOException
        void closeNoThrow();

        virtual bool isClosed() const { return closed_; }

#ifndef _WIN32
        void setRO();
        void setRW();
        static void setRWByName(const std::string &filename);
        static bool isWriteable(const std::string &filename);
#endif
        //just for compatibility with Sockets. (Made to make abstraction on IOBaseStream)
        void setCork() {}
		void clearCork() {}
        void getCork() {}
        void setRequestTimeout(double) {}

        /** @exception IOException */
        int32_t read(byte *ptr,
                     const int32_t count);

        /** @exception IOException */
        byte peek();

        /** @exception IOException */
        int32_t write(const byte *ptr, int32_t count);

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

        /** @exception IOException */
        static void mkdir(const std::string &dir);
        static std::string trimLastDirectory(const std::string &dir);
        static int32_t
            recursiveMkDir(const std::string &base, const std::string &path);

        /** @exception IOException */
        static void rmdir(const std::string &dir);

        /** @exception IOException */
        static void unlink(const std::string &filename, bool ignoreIfNotExists = false);

        /** @exception IOException */
        static void rename(const std::string &oldname, const std::string &newname);

        /** @exception IOException */
        static void copy(const std::string &oldname, const std::string &newname);

        static bool exists(const std::string &name);

		/** @exception IOException */
		static uint64_t diskFreeSpace(const std::string &path);

		/** @exception IOException */
		static int32_t removeDirectory(const std::string &path);

        static void ls(const std::string &dir, std::list<std::string> &res);

        static uint64_t
        directoryRegularFileContentsSize(const std::string& dir);

        const std::string &name() const;

        size_t
        saneRead(void* ptr, size_t size, size_t count);

        // creates a filename with a random suffix from the input name
        /** @exception IOException */
	static std::string
        createTempFile(const std::string& filename);

    private:
        static off_t length(const char *filename); // block usage with .c_str()
        File();
        File(const File &);
        static int32_t createDirectory_(const std::string &path);

        // performs non-safe copy:
        // - dst could be only partially updated in case of crash
        // - trailing garbage if dst > src
        /** @exception IOException */
        static void
        copy_(const std::string &oldname, const std::string &newname);

        static int32_t
        check_dir_error_(const std::string &path, int errorCode);


        File & operator=(File &);

        const std::string filename_;
        FILE *f_;
        bool closed_, noclose_;
        int mode_;
    };
}


#endif	/* _FILE_H */

// Local Variables: **
// End: **
