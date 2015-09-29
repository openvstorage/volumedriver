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

#ifndef _BYTEARRAY_H
#define	_BYTEARRAY_H

#include "defines.h"
#include <memory>
#include <string>

namespace fungi {

    class ByteArray {
    public:
        ByteArray(){}
        virtual ~ByteArray()
        {}

        virtual operator byte *() = 0;
        virtual operator const byte *() const = 0;

        virtual byte operator [](int index) const = 0;

        virtual byte& operator [](int index) = 0;
        virtual void clear() = 0;
        virtual int32_t size() const = 0;

        virtual void copyFrom(const ByteArray &from) = 0;

        // for when we're interfacing  with non-ByteArray code:
        virtual void copyFrom(const byte *from) = 0;

        std::string toString() const;

        bool compare(const ByteArray &) const;
    };

    typedef std::auto_ptr<ByteArray> ByteArray_aptr;
}

#endif	/* _BYTEARRAY_H */

// Local Variables: **
// End: **
