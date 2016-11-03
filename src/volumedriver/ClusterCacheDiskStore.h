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

#ifndef READ_CACHE_DISK_STORE
#define READ_CACHE_DISK_STORE

#include "Types.h"

#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/mount.h>

#include <youtils/Assert.h>
#include <youtils/IOException.h>
#include <youtils/Logging.h>
#include <youtils/Md5.h>

namespace volumedriver
{

class ClusterCacheDiskStore
{

public:
    MAKE_EXCEPTION(VerificationFailedException, fungi::IOException);

    ClusterCacheDiskStore()
        : cluster_size_(0)
        , total_size_(0)
        , device_fd_(-1)
    {}

    ClusterCacheDiskStore(const fs::path& path,
                          const uint64_t size,
                          const size_t cluster_size)
        : path_(path)
        , cluster_size_(cluster_size)
    {
        device_fd_ = open(path_.string().c_str(),
                          O_RDWR);

        if(device_fd_ < 0)
        {
            LOG_ERROR("Could not open device " << path << ", " << strerror(errno));
            throw fungi::IOException("Could not open ClusterCache device");
        }

        if(size == 0)
        {
            ioctl(device_fd_, BLKGETSIZE64, &total_size_);
        }
        else
        {
            if( size % cluster_size_ != 0)
            {
                LOG_ERROR("Specified file size " << size << " for "
                          << path.string() << " should be a multiple of clustersize" );
                throw fungi::IOException("Specified file size must be a multiple of clustersize");
            }

            if((errno=posix_fallocate(device_fd_, 0,size))!=0)
            {
                LOG_ERROR("Could not allocate file " << path.string() << " with size " << size << ", " << strerror(errno));
                throw fungi::IOException("Could not allocate file ");
            }
            total_size_ = size;
        }
        ASSERT(total_size_ > cluster_size_);
        ASSERT(total_size_ % cluster_size_ == 0);
        total_size_ -= cluster_size_;
    }

    ~ClusterCacheDiskStore()
    {
        if(device_fd_ >= 0)
        {
            int ret = ::close(device_fd_);
            if (ret < 0)
            {
                int err = errno;
                LOG_ERROR(path_ << ": failed to close device: " << ::strerror(err) <<
                          " - ignoring");
            }
        }
    }


    void
    write_guid(const UUID& uuid) throw ()
    {
        VERIFY(device_fd_ >= 0);
        std::string uuid_string = uuid.str();
        ssize_t res = pwrite(device_fd_, uuid_string.c_str(), uuid_string.size(), 0);
        if(res == -1)
        {
            LOG_ERROR("Could not write guid to " << path_.string() <<
                      ", errno " << errno << ": " << strerror(errno));
        }
        if(res != (ssize_t)UUID::getUUIDStringSize())
        {
            LOG_ERROR("Short write detected while writing guid to " <<
                      path_.string() << ": expected " << UUID::getUUIDStringSize() <<
                      ", written " << res);
        }
    }

    bool
    check_guid(const UUID& uuid) throw()
    {
        VERIFY(device_fd_ >= 0);
        std::vector<char> vec(UUID::getUUIDStringSize());
        ssize_t res = pread(device_fd_,
                            &vec[0],
                            UUID::getUUIDStringSize(), 0);
        if(res == -1)
        {
            LOG_ERROR("Could not read guid from " << path_.string() <<
                      ", errno " << errno << ": " << strerror(errno));
            return false;
        }
        if(res != (ssize_t)UUID::getUUIDStringSize())
        {
            LOG_ERROR("Short read detected while reading guid from " <<
                      path_.string() << ": expected " << UUID::getUUIDStringSize() <<
                      ", read " << res);
            return false;
        }

        if(UUID::isUUIDString(&vec[0]))
        {
            return uuid == UUID(&vec[0]);
        }
        else
        {
            return false;
        }
    }

    // For Testing
    void
    fail_fd_forread()
    {
        if(device_fd_ >= 0)
        {
            int ret = ::close(device_fd_);
            device_fd_ = -1;
            VERIFY(ret == 0);
        }

        device_fd_ = open(path_.string().c_str(),
                          O_WRONLY);
        ASSERT(device_fd_ >= 0);
    }

    void
    fail_fd_forwrite()
    {
        if(device_fd_ >= 0)
        {
            int ret = ::close(device_fd_);
            device_fd_ = -1;
            VERIFY(ret == 0);
        }

        device_fd_ = open(path_.string().c_str(),
                          O_RDONLY);
        ASSERT(device_fd_ >= 0);
    }

    ssize_t
    read(uint8_t* buf,
         uint32_t index)
    {
        VERIFY(device_fd_ >= 0);
        return pread(device_fd_, buf, cluster_size_, (index+1) * cluster_size_);
    }

    ssize_t
    write(const uint8_t* buf,
          uint32_t index)
    {
        VERIFY(device_fd_ >= 0);
        return pwrite(device_fd_, buf, cluster_size_, (index+1)*cluster_size_);
    }

    void
    sync()
    {
        VERIFY(device_fd_ >= 0);
        int ret = ::fsync(device_fd_);
        if (ret < 0)
        {
            LOG_ERROR(path_ << ": failed to sync: " << strerror(errno));
            throw fungi::IOException("Could not sync file",
                                     path_.string().c_str());
        }
    }

    void
    check(const youtils::Weed& key,
          uint32_t index)
    {
        VERIFY(device_fd_ >= 0);
        std::vector<uint8_t> vec(cluster_size_);
        VERIFY(pread(device_fd_, &vec[0], cluster_size_, (index+1) * cluster_size_) == (ssize_t)cluster_size_);
        youtils::Weed w (vec);
        if (w != key)
        {
            LOG_ERROR("MD5 mismatch detected: path_ " << path_ << " index " << index);
            throw VerificationFailedException("MD5 mismatch detected",
                                              path_.string().c_str());
        }
    }

    uint64_t
    total_size() const
    {
        return total_size_;
    }

    const fs::path&
    path() const
    {
        return path_;
    }

    void
    reinstate()
    {
        VERIFY(device_fd_ < 0);

        if(!fs::exists(path_))
        {
            LOG_ERROR(path_ << " does not exist");
            throw fungi::IOException("Cannot reinstate ClusterCacheDiskStore - inexistent path",
                                     path_.string().c_str());
        }

        const fs::file_status status = fs::status(path_);

        switch(status.type())
        {
        case fs::regular_file:
            if(fs::file_size(path_) < total_size_)
            {
                LOG_ERROR(path_ << ": file size " << fs::file_size(path_) <<
                          " smaller than total_size " << total_size_);
                throw fungi::IOException("Cannot reinstate ClusterCacheDiskStore - file shrunk?",
                                         path_.string().c_str());
            }
            break;
        case fs::block_file:
            {
                uint64_t block_size = 0;
                ::ioctl(device_fd_, BLKGETSIZE64, &block_size);
                if(block_size < total_size_)
                {
                    LOG_ERROR(path_ << ": size of block device " << block_size <<
                              " smaller than total_size " << total_size_);
                    throw fungi::IOException("Cannot reinstate ClusterCacheDiskStore - block device shrunk?",
                                             path_.string().c_str());
                }
            }
            break;
        default:
            LOG_ERROR(path_ << " does not refer to a regular file or a block device");
            throw fungi::IOException("Cannot reinstate ClusterCacheDiskStore - neither block device nor file",
                                     path_.string().c_str());
            break;
        }

        device_fd_ = open(path_.string().c_str(),
                          O_RDWR);
        if(device_fd_ < 0)
        {
            int err = errno;
            LOG_ERROR(path_ << ": unable to open: " << strerror(err));
            throw fungi::IOException("Cannot reinstate ClusterCacheDiskStore - failed to open",
                                     path_.string().c_str(),
                                     err);
        }
    }

    static size_t
    default_cluster_size()
    {
        return default_cluster_size_;
    }

    size_t
    cluster_size() const
    {
        return cluster_size_;
    }

private:
    DECLARE_LOGGER("ClusterCacheDiskStore");

    static const size_t default_cluster_size_;

    const fs::path path_;
    uint64_t cluster_size_;
    uint64_t total_size_;
    int device_fd_;

    friend class boost::serialization::access;

    BOOST_SERIALIZATION_SPLIT_MEMBER();

    template<class Archive>
    void
    load(Archive& ar, const unsigned int version)
    {
        std::string str;
        ar & str;
        ar & total_size_;
        const_cast<fs::path&>(path_) = str;
        if (version > 0)
        {
            ar & cluster_size_;
        }
        else
        {
            cluster_size_ = default_cluster_size();
        }

        VERIFY(cluster_size_ >= 0);
    }

    template<class Archive>
    void
    save(Archive& ar, const unsigned /* version */) const
    {
        VERIFY(cluster_size_ >= 0);

        std::string str = path_.string();
        ar & str;
        ar & total_size_;
        ar & cluster_size_;
    }
};

}

BOOST_CLASS_VERSION(volumedriver::ClusterCacheDiskStore, 1);

#endif // READ_CACHE_DISK_STORE

// Local Variables: **
// mode: c++ **
// End: **
