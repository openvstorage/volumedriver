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

#include <cerrno>
#include <iostream>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mount.h>

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "DeviceReader.h"
#include "VTException.h"
#include <youtils/Logging.h>

DeviceReader::DeviceReader(const std::string& device,
                           const std::string& reference,
                           double read_chance)
 : device_name_(device)
 , reference_name_(reference)
 , size_(0)
 , read_chance_(read_chance)
 , device_fd_(-1)
 , reference_fd_(-1)
{
  device_fd_ = open(device.c_str(),
                    O_RDONLY|O_FSYNC);
  if(device_fd_<0)
  {
    LOG_ERROR("Could not open " << device_name_);
    throw VTException();
  }

  if(not reference.empty())
  {

      reference_fd_= open(reference.c_str(),
                          O_RDONLY|O_EXCL|O_FSYNC);

      if(reference_fd_<0)
      {
          LOG_ERROR("Could not open file " << reference_name_);
          throw VTException();
      }
  }

  if(ioctl(device_fd_,BLKGETSIZE64, & size_) == -1)
  {
      LOG_ERROR("Could not get device size");
      throw VTException();
  }


  LOG_INFO("The device size is " << size_);

}

bool
DeviceReader::shouldRead()
{
    if(read_chance_ <= 0.0)
    {
        return false;
    }
    else if(read_chance_ >= 1.0)
    {
        return true;
    }
    else
    {
        return drand48() < read_chance_;
    }
}



bool
DeviceReader::operator()()
{

  char device[blocksize_];
  char reference[blocksize_];

  if(lseek(device_fd_,0,SEEK_SET)==-1)
  {
    LOG_ERROR("Could not seek to beginning on " << device_name_);
    throw VTException();
  }

  if(reference_fd_ >= 0)
  {
      if(lseek(reference_fd_,0,SEEK_SET)==-1)
      {
          LOG_ERROR("Could not seek to beginning on " << reference_name_);
          throw VTException();
      }
  }


  for(unsigned long long i = 0; i < size_; i+=blocksize_)
  {
      if(shouldRead())
      {
          ssize_t device_read_size = read(device_fd_,&device, blocksize_);
          if (device_read_size < 0)
          {
              LOG_ERROR("Read error on device");
              throw VTException("Read error on device");
          }

          if(reference_fd_ >= 0)
          {

              ssize_t reference_read_size= read(reference_fd_,&reference,blocksize_);
              if (reference_read_size < 0)
              {
                  LOG_ERROR("Read error on reference");
                  throw VTException("Read error on reference");
              }

              if(device_read_size != reference_read_size)
              {
                  LOG_ERROR("Read different amounts of data from the device and reference");
                  return false;
              }

              if(memcmp(device, reference,device_read_size) != 0)
              {
                  LOG_ERROR("Difference in blocks at offset " << i );
                  return false;
              }
          }
      }
  }


  return true;
}

// Local Variables: **
// compile-command: "make" **
// End: **
