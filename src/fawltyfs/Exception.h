// Copyright 2015 iNuron NV
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

#ifndef FAWLTY_FS_EXCEPTION_H_
#define FAWLTY_FS_EXCEPTION_H_

#include <exception>
#include <string>

namespace fawltyfs
{

class Exception
    : public std::exception
{
public:
    explicit Exception(const std::string& msg)
        : msg_(msg)
    {}

    virtual ~Exception() throw ()
    {}

    virtual const char*
    what() const throw ()
    {
        return msg_.c_str();
    }

private:
    const std::string msg_;
};

#define DECLARE_EXCEPTION(ex, parent)           \
    class ex                                    \
        : public parent                         \
    {                                           \
        public:                                 \
        explicit ex(const std::string& msg)     \
            : parent(msg)                       \
        {}                                      \
    };
}

#endif // !FAWLTY_FS_EXCEPTION_H_

// Local Variables: **
// compile-command: "scons -D -j 4" **
// mode: c++ **
// End: **
