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

#include "DeviceWriter.h"
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <iostream>
#include "VTException.h"
#include <errno.h>
#include <youtils/Logging.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <boost/filesystem.hpp>
DeviceWriter::DeviceWriter(const std::string& device,
                           const std::string& reference,
                           const uint64_t skipblocks,
                           const uint64_t firstblock,
                           double rand,
                           const Strategy& strategy)
    : device_name_(device)
    , reference_name_(reference)
    , strategy_(strategy)
    , device_fd_(-1)
    , reference_fd_(-1)
    , skipblocks_(skipblocks)
    , firstblock_(firstblock)
    , random_(rand)
{
    assert(skipblocks_ >= 1);
	device_fd_ = open(device_name_.c_str(), O_RDWR);
    if(device_fd_<0)
    {
    	LOG_ERROR("Could not open " << device_name_);
        throw VTException();
    }

    ioctl(device_fd_, BLKGETSIZE64, &size_);

    LOG_INFO("Device size is " << size_ << " bytes");

    if(boost::filesystem::exists(reference_name_))
    {
        reference_fd_ = open(reference_name_.c_str(), O_RDWR);
        if(reference_fd_<0)
        {
            LOG_ERROR("Could not open file " << reference_name_);
            throw VTException();
        }

        if(lseek(reference_fd_, 0, SEEK_END) != static_cast<ssize_t>(size_))
        {
            LOG_ERROR("Device and preexisting reference file have different sizes ");
            throw VTException();
        }

    }
    else
    {

        reference_fd_ = open(reference_name_.c_str(),
                             O_RDWR|O_CREAT|O_TRUNC,
                             S_IWUSR|S_IRUSR);
        if(reference_fd_<0)
        {
            LOG_ERROR("Could not open file " << reference_name_);
            throw VTException();
        }

        if((errno=posix_fallocate(reference_fd_, 0,size_))!=0)
        {
            LOG_ERROR("Could not allocate file" << reference);
            throw VTException();
        }
    }


    assert(reference_fd_ >= 0);

    if(random_ >= 0.0 and random_ < 1.0)
    {
        srand48(time(0));
    }

    if(lseek(device_fd_,0,SEEK_SET)==-1)
    {
        LOG_ERROR("Could not seek to beginning on " << device_name_);
        throw VTException();
    }
    if(lseek(reference_fd_,0,SEEK_SET)==-1)
    {
        LOG_ERROR("Could not seek to beginning on " << reference_name_);
        throw VTException();
    }

}

DeviceWriter::~DeviceWriter()
{
    if(device_fd_>0)
    {
        if(close(device_fd_)<0)
        {
            LOG_ERROR("Could not close device filedescriptor " << (const char*)strerror(errno));
        }
    }
    if(reference_fd_>0)
    {
        if(close(reference_fd_)<0)
        {
            LOG_ERROR("Could not close reference filedescriptor " << (const char*)strerror(errno));
        }
    }
}


unsigned long long
DeviceWriter::size() const
{
    return size_;
}

void
DeviceWriter::operator()()
{
    // does one sweep over the device and calls the Strategy for each block
    // assumption is we always get the amount of data we want to read and
    // we always can write the amount of data we want to write.
    if(fcntl(device_fd_, F_GETFL) bitand O_FSYNC)
    {
        LOG_INFO("Device was opened with O_FSYNC flag");
    }
    else
    {
        LOG_INFO("Device was not opened with O_FSYNC flag");
    }

    if(fcntl(reference_fd_, F_GETFL) bitand O_FSYNC)
    {
        LOG_INFO("Reference was opened with O_FSYNC flag");
    }
    else
    {
        LOG_INFO("Reference was not opened with O_FSYNC flag");
    }


    if(lseek(device_fd_,0,SEEK_SET)==-1)
    {
        LOG_ERROR("Could not seek to beginning on " << device_name_);
        throw VTException();
    }
    if(lseek(reference_fd_,0,SEEK_SET)==-1)
    {
        LOG_ERROR("Could not seek to beginning on " << reference_name_);
        throw VTException();
    }

    // unsigned char device[blocksize_];
    // unsigned char reference[blocksize_];

    unsigned char new_data[blocksize_];
    unsigned int counter = 0;
    //    std::cerr << "Size: " << size_ << std::endl;

    for(unsigned long long i = 0; i < size_; i +=blocksize_)
    {

        // ssize_t device_read_size = read(device_fd_,&device, blocksize_);
        // ssize_t reference_read_size= read(reference_fd_,&reference,blocksize_);
        // if(device_read_size != reference_read_size)
        // {
        //     LOG_ERROR("Read different amounts of data from the device and reference");
        //     throw VTException();
        // }
        const uint64_t current_block = i / blocksize_;

        if(DontSkip(current_block) and
           RandomWrite())
        {
            strategy_(new_data,blocksize_);
            ssize_t device_write_size = write(device_fd_,&new_data,blocksize_);
            ssize_t reference_write_size = write(reference_fd_,&new_data,device_write_size);

            if(reference_write_size < blocksize_)
            {
                LOG_ERROR("reference_write_size < blocksize_");
                throw VTException();
            }

            if(device_write_size < blocksize_)
            {
                LOG_ERROR("device_write_size < blocksize_");
                throw VTException();
            }
        }
        else
        {
            off_t dev = lseek(device_fd_, blocksize_, SEEK_CUR);

            if( dev == -1)
            {
                LOG_ERROR("could not advance position on device " << device_name_);
                throw VTException();

            }

            off_t ref = lseek(reference_fd_, blocksize_, SEEK_CUR);
            if( ref == -1)
            {
                LOG_ERROR("could not advance file position on " << reference_name_);
                throw VTException();
            }

            if(dev != ref)
            {
                throw VTException("dev ! ref");
            }

        }
        counter += blocksize_;

        if(counter >= (1 << 27))
        {
            counter -= (1<<27);
            LOG_INFO("The machine that goes ping has written another 128 meg");
        }
    }

    if(fsync(reference_fd_) != 0)
    {
        LOG_ERROR("Could not sync the reference file");
        throw VTException();
    }
    if(fsync(device_fd_) != 0)
    {
        LOG_ERROR("Could not sync the device file");
        throw VTException();
    }


    LOG_INFO("Success: Finished writing files");
}
// Local Variables: **
// compile-command: "scons -D --kernel_version=system --ignore-buildinfo -j 5" **
// mode: c++ **
// End: **
