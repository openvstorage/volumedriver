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

#ifndef VT_EXCEPTION_H
#define VT_EXCEPTION_H
#include <exception>
#include <string>
#include <sstream>

class VTException : public std::exception
{
  public:
    VTException(const char *msg,
                const char *name = 0,
                int error_code = 0);

    VTException();

	virtual ~VTException() throw ();
	virtual const char* what() const throw ();
	int getErrorCode() const;
  private:
	int error_code_;
	std::string msg_;
};

#endif // VT_EXCEPTION_H

