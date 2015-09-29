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
