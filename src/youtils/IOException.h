// This file is dual licensed GPLv2 and Apache 2.0.
// Active license depends on how it is used.
//
// Copyright 2016 iNuron NV
//
// // GPL //
// This file is part of OpenvStorage.
//
// OpenvStorage is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with OpenvStorage. If not, see <http://www.gnu.org/licenses/>.
//
// // Apache 2.0 //
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef _IOEXCEPTION_H
#define	_IOEXCEPTION_H

#include <exception>
#include <string>

namespace fungi {

class IOException: public std::exception
{
public:
    IOException(const char *msg,
                const char *name = 0,
                int error_code = 0);

    IOException(std::string&& str);

    IOException(const std::string&);

    virtual ~IOException() noexcept;

    virtual const char* what() const noexcept;

    int getErrorCode() const;

private:
    int error_code_;
    std::string msg_;
};
}

// this macro can be used to easily make child exceptions
#define MAKE_EXCEPTION(myex, parentex)                                  \
    class myex: public parentex                                         \
    {                                                                   \
        public:                                                         \
        myex(const char *msg,                                           \
             const char *name = 0,                                      \
             int error_code = 0)                                        \
            : parentex(msg, name, error_code)                           \
        {}                                                              \
                                                                        \
        myex(std::string&& str)                                         \
            : parentex(std::move(str))                                  \
        {}                                                              \
                                                                        \
        myex(const std::string& str)                                    \
            : parentex(str)                                             \
        {}                                                              \
    };                                                                  \


namespace fungi {

MAKE_EXCEPTION(ResolvingError, IOException);
MAKE_EXCEPTION(ConnectionRefusedError, IOException);

}

#endif	/* _IOEXCEPTION_H */
