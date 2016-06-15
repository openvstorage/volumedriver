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

#include "VTException.h"
#include "errno.h"
#include <string.h>

VTException::VTException(const char *msg, const char *name, int error_code)
: error_code_(error_code),
  msg_(msg)
{
  std::stringstream ss;
  ss << " ";
  if (name != 0)
  {
    ss << name;
  }
  if (error_code != 0)
  {
    ss << " ";
    char *res = strerror(error_code);
    if (res != 0)
    {
      ss << res;
    }
    ss << " (" << error_code << ")";
  }
  msg_ += ss.str();
}

VTException::VTException()
:error_code_(errno),
 msg_(strerror(error_code_))
{}

VTException::~VTException() throw ()
{}


const char *
VTException::what() const throw ()
{
  return msg_.c_str();
}

int VTException::getErrorCode() const
{
  return error_code_;
}
