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

#define INSIDE_FILE_CPP
#include "File.h"
#include <youtils/IOException.h>
#include "ByteArray.h"

#include <cstring>
#include <string>
#include <cerrno>
#include <iostream>
#include <limits>
#include <cstdlib>
#include <cerrno>

#include <unistd.h>
#ifndef _WIN32
#include <dirent.h>
#endif

#include <sys/stat.h>
#include <sys/types.h>
#include <cassert>
#ifndef _WIN32
#include <sys/statvfs.h> //For statvfs()
#endif


namespace fungi {

File::File(const std::string &filename, FILE *wrap) :
	filename_(filename), f_(wrap), closed_(false), noclose_(true) {
}

std::unique_ptr<File> File::getStdin()
{
    std::unique_ptr<File> f(new File("stdin", stdin));
    return f;
}

std::unique_ptr<File> File::getStdout()
{
    std::unique_ptr<File> f(new File("stdout", stdout));
    return f;
}

std::unique_ptr<File> File::getStderr()
{
    std::unique_ptr<File> f(new File("stderr", stderr));
    return f;
}

File::File(const std::string &filename, int mode) :
    filename_(filename),
    f_(0),
    closed_(true),
    noclose_(false),
    mode_(mode)
{
}

void File::open()
{
    // we may want to get rid of this LOG/throw anti-pattern in the future
    try
    {
        try_open();
    }
    catch (IOException& )
    {
        LOG_ERROR("Could not open file " << filename_.c_str() << " "<< strerror(errno));
        throw;
    }
}


void File::try_open() {
	if (!closed_) {
		//throw IOException("File::open: already open", filename_.c_str(), 0);
        return;
	}
	// the reason for the "F" below is that in Solaris 32-bit this is needed
	// to allow files to have an underlying filedescriptor >= 256
	// this is for legacy reasons and is explained more in detail in the
	// manual page of fopen on solaris.
	int mode = mode_;
	const char *modes = 0;

        switch (mode)
        {
        case Append:
#ifdef _WIN32
            modes = "ab";
#else
            modes = "abF";
#endif
            break;
        case (Append|Read):
#ifdef _WIN32
            modes = "a+b";
#else
            modes = "a+bF";
#endif
            break;
        case Read:
#ifdef _WIN32
            modes = "rb";
#else
            modes = "rbF";
#endif
            break;
        case Write:
#ifdef _WIN32
            modes = "wb";
#else
            modes = "wbF";
#endif
            break;
        case (Read|Write):
#ifdef _WIN32
            modes = "w+b";
#else
            modes = "w+bF";
#endif
            break;
        case Update:
#ifdef _WIN32
            modes = "r+b";
#else
            modes = "r+bF";
#endif
            break;
        default:
            throw IOException("File::open: unsupported mode", filename_.c_str(), 0);
	}
#ifdef _WIN32
	if (fopen_s(&f_, filename_.c_str(), modes)!=0) {
		throw IOException("File::open", filename_.c_str(), errno);
	}
#else
	f_ = fopen(filename_.c_str(), modes);
#endif
	if (f_ == 0) {
        throw IOException("File::open", filename_.c_str(), errno);
	}
	closed_ = false;
	//std::cout << "File " << filename_ << " (re)opened in mode " << modes << ":" << mode_ <<  std::endl;
}

File::~File() {
	if (!closed_ && !noclose_) {
		closeNoThrow();
	}
}

void File::reopen(int mode) {
	if (!closed_) {
		close();
	}
	mode_ = mode;
	open();
}

void File::close() {
	//std::cout << "File::close(" << filename_ << ")" << std::endl;
	if (noclose_) {
		LOG_WARN("File::close; ignoring close for noclose_ File.");
		return;
	}
	if (closed_) {
		LOG_DEBUG("File::close; ignoring close for closed_ File.");
	    f_ = 0;
		return;
	}
	if (fclose(f_) == EOF) {
        f_ = 0;
		throw IOException("File::close", filename_.c_str(), errno);
	}
	f_ = 0;
	closed_ = true;
}

void File::closeNoThrow() {
	//std::cout << "File::close_no_throw(" << filename_ << ")" << std::endl;
	if (noclose_) {
		LOG_WARN("File::close; ignoring close for noclose_ File.");
		return;
	}
    if (f_ != 0) {
	    fclose(f_);
    }
	f_ = 0;
	closed_ = true;
}

#ifndef _WIN32
void File::setRO() {
	::fchmod(fileno(), S_IRUSR| S_IRGRP | S_IROTH);
}

void File::setRW() {
	::fchmod(fileno(), S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR | S_IWGRP | S_IWOTH);
}

void File::setRWByName(const std::string &filename) {
	::chmod(filename.c_str(), S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR | S_IWGRP | S_IWOTH);
}

bool File::isWriteable(const std::string &filename) {
	struct stat st;
	::stat(filename.c_str(), &st);
	return (st.st_mode & S_IWUSR) != 0;
}
#endif

size_t
File::saneRead(void* ptr,
               size_t size,
               size_t count)
{
    assert(f_);

    size_t res = fread(ptr, size, count, f_);
    if(ferror(f_))
    {
        clearerr(f_);
        throw IOException("File::read", filename_.c_str(), errno);
    }
    return res;
}

int32_t
File::read(byte *ptr,
           const int32_t count)
{
    // Will take somebody with a larger supply of hallicunogenics to understand this code.
    int32_t res = fread(ptr, sizeof(byte), (size_t)count, f_);


    if (feof(f_) && res == 0 && count == 0)
    {
        throw IOException("File::read EOF", filename_.c_str(), 0);
    }
    int err = ferror(f_);

    if (err != 0)
    {
        clearerr(f_);
        throw IOException("File::read", filename_.c_str(), err);
    }

    return res;
}

byte File::peek() {
    const int res = fgetc(f_);
    if (res == EOF) {
        throw IOException("File::peek EOF", filename_.c_str(), 0);
    }
    const byte bres = (byte)res;
    const int res2 = ungetc(res, f_);
    if (res2 == EOF) {
        throw IOException("File::peek EOF on ungetc", filename_.c_str(), 0);
    }
    return bres;
}

bool File::eof() const {
    const int res = fgetc(f_);
    if (res == EOF) {
        return true;
    }
    const int res2 = ungetc(res, f_);
    if (res2 == EOF) {
        throw IOException("File::eof EOF on ungetc", filename_.c_str(), 0);
    }
    return false;
}

int32_t
File::write(const byte *ptr,
            int32_t count)
{
    assert(!closed_);
    // LOG_WARN("XXX writing " << count << "bytes");
    // here count is casted to size_t, giving a possible loss
    // however we can safely assume that no-one wants to read more then 2**31-1 bytes at once
    int32_t res = fwrite(ptr, sizeof(byte), (size_t)count, f_);
    int err = ferror(f_);
    // Dont pass the err into the error here since it is apparently meaningless.
    if (err != 0) {
        clearerr(f_);
        throw IOException("File::write", filename_.c_str());
    }

    return res;

}

int32_t File::write(const ByteArray &data) {
	assert(!closed_);
	return write(data, data.size());
}

int File::fileno() const {
	if (f_ == 0) {
		throw IOException("File::fileno: file handle is 0");
	}
#ifndef _WIN32
	int no = ::fileno(f_);
#else
	int no = ::_fileno(f_);
#endif
	if (no == -1) {
		throw IOException("File::fileno", filename_.c_str(), errno);
	}
	return no;
}

void File::flush() {
	if (f_ == 0) {
		throw IOException("File::flush, no file handle available");
	}
    int err = fflush(f_);
    if (err != 0) {
        throw IOException("File::flush", filename_.c_str(), errno);
    }
}

void File::sync() {
#ifndef _WIN32
    if (fsync(fileno()) < 0) {
    	throw IOException("File::sync", filename_.c_str(), errno);
    }
#endif
}

void File::datasync() {
#ifndef _WIN32
    if (fdatasync(fileno()) < 0) {
    	throw IOException("File::datasync", filename_.c_str(), errno);
    }
#endif
}

FILE *File::getHandle() const {
	return f_;
}


int64_t File::length() const
{
	int64_t length, cur_pos;
	cur_pos = tell();
	if (fseeko(f_, 0, SEEK_END) == -1) {
		throw IOException("File::length", filename_.c_str(), errno);
	}
	length = tell();
    if(fseeko(f_,cur_pos,SEEK_SET))
    {
        throw IOException("seek", filename_.c_str(), errno);
    }
	return length;
}

int64_t File::length(const std::string &filename) {
	File file(filename);
	file.open();
	int64_t l = file.length();
	file.close();
	return l;
}

void File::rewind() {
	assert(!closed_);
	::rewind(f_);
}

int64_t File::tell() const
{
	if (closed_) {
		return File::length(filename_);
	}
	const int64_t cur_pos = ftello(f_);
	if (cur_pos == -1) {
		throw IOException("length", filename_.c_str(), errno);
	}
	return cur_pos;
}

void File::seek(int64_t offset, int WHENCE) {
    assert(!closed_);
    if (fseeko(f_, offset, WHENCE) != 0) {
        throw IOException("seek", filename_.c_str(), errno);
    }
}

void File::mkdir(const std::string &dir) {
	//std::cout << "mkdir " << dir << std::endl;
    int res = createDirectory_(dir);
	if (res != 0) {
		throw IOException("mkdir", dir.c_str(), errno);
	}
}

std::string File::trimLastDirectory(const std::string &dir) {
    return dir.substr(0, dir.find_last_of('/'));
}

int32_t File::recursiveMkDir(const std::string &base,
                             const std::string &path) {
    int32_t returnValue = createDirectory_(base + path);
    if (returnValue == 1) {
        returnValue = recursiveMkDir(base, trimLastDirectory(path));
        if (returnValue == 0) {
            returnValue = createDirectory_(base + path);
        }
    } else if (returnValue > 1) {
        returnValue = 0;
    }

    return returnValue;
}

    int32_t File::createDirectory_(const std::string &path) {
#ifndef _WIN32
        int stat = ::mkdir(path.c_str(), 0777);
#else
        int stat = ::_mkdir(path.c_str());
#endif
        int ret = -1;
        if (stat < 0) {
            switch (errno) {
                case EEXIST:
                    //directory already exists, cool.
                    ret = 0;
                    break;
                case ENOENT:
                    //directory has a component that need to be created first.
                    ret = 1;
                    break;
            default:
#pragma warning ( push )
#pragma warning ( disable: 4996 )
                LOG_ERROR("Cannot create directory " << path.c_str() << ": " << strerror(errno));
#pragma warning ( pop )
                    break;
            }
        } else {
            ret = 0;
        }
        return ret;
    }

void File::rmdir(const std::string &dir) {
#ifndef _WIN32
	int res = ::rmdir(dir.c_str());
#else
	int res = ::_rmdir(dir.c_str());
#endif
	if (res != 0) {
		throw IOException("rmdir", dir.c_str(), errno);
	}
}

void File::unlink(const std::string &filename, bool ignoreIfNotExists) {
	//std::cout << "File::unlink(" << filename << ")" << std::endl;
#ifndef _WIN32
	int res = ::unlink(filename.c_str());
#else
	int res = ::_unlink(filename.c_str());
#endif
	if (errno == ENOENT && ignoreIfNotExists) {
		return;
	}
	if (res != 0) {
            throw IOException("unlink", filename.c_str(), errno);
	}
}

const std::string &File::name() const {
	return filename_;
}

void File::truncate(int64_t length) {
	flush();
#ifndef _WIN32
	int res = ftruncate(fileno(), length);
#else
	int res = _chsize(fileno(), (long)length);
#endif
	if (res < 0) {
		throw IOException("ftruncate", filename_.c_str(), errno);
	}
}

void File::rename(const std::string &oldname, const std::string &newname) {
	if (::rename(oldname.c_str(), newname.c_str()) < 0) {
		throw IOException("rename", oldname.c_str(), errno);
	}
}

bool File::exists(const std::string &name) {
#ifndef _WIN32
	struct stat s;
	return stat(name.c_str(), &s) == 0;
#else
	FILE* file = fopen(name.c_str(),"r");
	if(file)
	{
		fclose(file);
		return true;
	}
	return false;
#endif
}

void File::copy_(const std::string &oldname, const std::string &newname) {
  //fuut(); Should be tracing
	byte buf[4096];
	File f1(oldname, File::Read);
	File f2(newname, File::Write);
	f1.open();
	f2.open();
	int32_t res;
	while ((res = f1.read(buf, 4096)) > 0) {
		f2.write(buf, res);
	}
	f2.close();
	f1.close();
}

uint64_t File::diskFreeSpace(const std::string &path) {
#ifndef _WIN32
	struct statvfs buf;
	int res = statvfs(path.c_str(), &buf);
	if (res != 0)
		throw IOException(strerror(errno));
	return (uint64_t) buf.f_bavail * (uint64_t) buf.f_bsize;
#else
	ULARGE_INTEGER freeBytesAvailable;
	ULARGE_INTEGER totalNumberOfBytes;
	ULARGE_INTEGER totalNumberOfFreeBytes;
	BOOL res = GetDiskFreeSpaceExA(
			path.c_str(),
			&freeBytesAvailable,
			&totalNumberOfBytes,
			&totalNumberOfFreeBytes
		);

    return totalNumberOfFreeBytes.QuadPart;
#endif
}

int32_t
File::check_dir_error_(const std::string &path, int errorCode)
{
    int32_t ret = 0;

	switch (errorCode) {
		case EACCES:
#pragma warning ( push )
#pragma warning ( disable: 4996 )
            LOG_WARN("Write access to the directory containing pathname was not allowed " << path.c_str() << ": " << strerror(errorCode));
#pragma warning ( pop )
			ret = 1;
			break;

		case EBUSY:
#pragma warning ( push )
#pragma warning ( disable: 4996 )
            LOG_WARN("pathname is currently in use by the system or some process that prevents its removal. " << path.c_str() << ": " << strerror(errorCode));
#pragma warning ( pop )
			ret = 2;
			break;

		case EFAULT:
#pragma warning ( push )
#pragma warning ( disable: 4996 )
            LOG_WARN("pathname points outside your accessible address space " << path.c_str() << ": " << strerror(errorCode));
#pragma warning ( pop )
			ret = 3;
			break;

		case EINVAL:
#pragma warning ( push )
#pragma warning ( disable: 4996 )
            LOG_WARN("pathname has .  as last component. " << path.c_str() << ": " << strerror(errorCode));
#pragma warning ( pop )
			ret = 4;
			break;

		case ELOOP:
#pragma warning ( push )
#pragma warning ( disable: 4996 )
            LOG_WARN("Too many symbolic links were encountered in resolving pathname. " << path.c_str() << ": " << strerror(errorCode));
#pragma warning ( pop )
			ret = 5;
			break;

		case EROFS:
#pragma warning ( push )
#pragma warning ( disable: 4996 )
            LOG_WARN("pathname refers to a directory on a read-only file system. " << path.c_str() << ": " << strerror(errorCode));
#pragma warning ( pop )
			ret = 6;
			break;
		case EPERM:
#pragma warning ( push )
#pragma warning ( disable: 4996 )
            LOG_WARN("The file system containing pathname does not support the removal of directories. " << path.c_str() << ": " << strerror(errorCode));
#pragma warning ( pop )
			ret = 7;
			break;

		case ENOENT:
			ret = 0;
			break;

        default:
            LOG_WARN("Other error for " << path.c_str() << strerror(errorCode));
            ret = 8;
            break;
    }
	return ret;
}

int32_t File::removeDirectory(const std::string &path) {
#ifdef _WIN32

	std::string winpath = getWinPath(path);
	int removetrial = 0;
	char* cpath = new char[winpath.size()+2];
	memset(cpath, 0, winpath.size()+2);
	memcpy(cpath, winpath.c_str(),winpath.size());

	DWORD ret;
	SHFILEOPSTRUCTA file_op = {
        NULL,
        FO_DELETE,
		cpath,
        "",
        FOF_NOCONFIRMATION |
        FOF_NOERRORUI |
        FOF_SILENT,
        false,
        0,
        "" };

		ret = SHFileOperationA(&file_op);
		// the file or directory to be deleted may have sharing violation so wait untill other handles are closed
		while(ret == ERROR_SHARING_VIOLATION || removetrial > 10)
		{
			LOG_FWARN("File","Cannot remove Directory" << path.c_str() <<" Error = " << ret<< ". trying after 200 ms");
			Sleep(200);
			ret = SHFileOperationA(&file_op);
			removetrial++;
		}
		delete[] cpath;
		return ret;

#else
	using namespace std;
	int32_t ret = 0;
	struct stat s;
	if (stat(path.c_str(), &s)) {
		return 0;
	}
	int32_t hasErrors = 0;
	int32_t errorCode = 0;
	if (!S_ISDIR(s.st_mode)) {
		// it's a file...
		File::unlink(path, true);
		return 0;

	} else {
		//directory, scan if it has contents
		struct dirent *dirp;
		DIR *myDirectory = opendir(path.c_str());
        if (myDirectory == 0)
        {
            int err = errno;
            return check_dir_error_(path, err);
        }
		while ((dirp = readdir(myDirectory)) != NULL) {
			if (!(strcmp(dirp->d_name, ".") == 0 || strcmp(dirp->d_name, "..") == 0)) {
                int32_t ret = removeDirectory(path + "/" + dirp->d_name);
				if (ret != 0) {
					closedir(myDirectory); /* retval ignored, is ok */
					return ret;
				}
			}
		}
		closedir(myDirectory); /* retval ignored, is ok */
	}
	try {
		rmdir(path.c_str());
	}catch(IOException &exception) {
		hasErrors = 1;
		errorCode = exception.getErrorCode();
	}
	if (hasErrors) {
        ret = check_dir_error_(path, errorCode);
	}
	return ret;
#endif
}
void
File::ls(const std::string &dir, std::list<std::string> &res)
{
#ifdef _WIN32
   WIN32_FIND_DATAA FindFileData;
   HANDLE hFind = INVALID_HANDLE_VALUE;
   DWORD dwError= ERROR_SUCCESS;
   LPSTR searchfilepath = "*";

   // Find the first file in the directory.
   hFind = FindFirstFileA(searchfilepath, &FindFileData);

   if (hFind != INVALID_HANDLE_VALUE)
   {
	   res.push_front(FindFileData.cFileName);
      while (FindNextFileA(hFind, &FindFileData) != 0)
      {
         res.push_front(FindFileData.cFileName);
      }

      FindClose(hFind);
      if (dwError != ERROR_NO_MORE_FILES)
      {
         return;
      }
   }
#else
    struct dirent **namelist;
    int n = scandir(dir.c_str(), &namelist, 0, alphasort);
    if (n < 0)
    {
        throw IOException("scandir", dir.c_str(), errno);
    }
    while(n--) {
        std::string name(namelist[n]->d_name);
        free(namelist[n]);
        if (name != "." and name != "..")
        {
            res.push_front(name);
        }
    }
    free(namelist);
#endif
}

uint64_t
File::directoryRegularFileContentsSize(const std::string& dir)
{
#ifndef _WIN32
    struct dirent ** namelist;
    int n = scandir(dir.c_str(), & namelist,0, 0);
    if(n < 0)
    {
        throw IOException("scandir", dir.c_str(), errno);
    }
    uint64_t res = 0;
    struct stat st;
    while(n--)
    {
        const std::string tmp = dir + "/" + namelist[n]->d_name;
        free(namelist[n]);
        if(stat(tmp.c_str(), &st) == 0)
        {
            if(S_ISREG(st.st_mode))
            {
                res += st.st_size;
            }
        }
        else
        {
            while (n--) {
                free(namelist[n]);
            }
            free(namelist);
            throw IOException("stat", tmp.c_str(), errno);
        }
    }
    free(namelist);
    return res;
#else
	throw IOException("Not Implemented ");
	return 0;
#endif

}

std::string
File::createTempFile(const std::string& filename)
{

    std::string s(filename + std::string("XXXXXX"));
    if (s.size() >= PATH_MAX)
    {
        throw fungi::IOException("Filename too long",
                                 s.c_str(),
                                 ENAMETOOLONG);
    }

    char buf[PATH_MAX];
    strcpy(buf, s.c_str());
    int ret = ::mkstemp(buf);
    if (ret < 0)
    {
        ret = errno;
        LOG_ERROR("Failed to create temp file " << buf << ": " << strerror(ret));
        throw fungi::IOException("Failed to create temp file",
                                 buf,
                                 ret);
    }

    ::close(ret);
    return buf;

}

void
File::copy(const std::string& src,
           const std::string& dst)
{
    const std::string tmp = createTempFile(dst);
    try
    {
        copy_(src, tmp);
        rename(tmp, dst);
    }
    catch (...)
    {
        unlink(tmp);
        throw;
    }
}

}
// Local Variables: **
// End: **
