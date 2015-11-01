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

#ifndef _WRAPBYTEARRAY_H
#define	_WRAPBYTEARRAY_H


#include "ByteArray.h"

namespace fungi {
    class WrapByteArray: public ByteArray {
	public:
        explicit WrapByteArray(byte *bytes, int32_t size);
        virtual ~WrapByteArray();

        virtual operator byte *();
        virtual operator const byte *() const;


        virtual byte operator [](int index) const;
        virtual byte& operator [](int index);

        virtual void clear();
        virtual int32_t size() const;

        virtual void copyFrom(const ByteArray &from);

        // for when we're interfacing  with non-ByteArray code:
        virtual void copyFrom(const byte *from);

    private:
        byte *data_;
        int32_t size_;

        // avoid implicit conversions
        WrapByteArray();
        WrapByteArray(const WrapByteArray &);
        WrapByteArray & operator=(const WrapByteArray &);
    };
}


#endif	/* _WRAPBYTEARRAY_H */
