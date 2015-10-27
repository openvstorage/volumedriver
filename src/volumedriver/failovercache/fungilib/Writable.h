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

#ifndef WRITABLE_H_
#define WRITABLE_H_

#include "defines.h"

namespace fungi {

class Writable {
public:
	virtual ~Writable() {}
	virtual void open() = 0;
	virtual void close() = 0;
	/** @exception IOException */
	virtual int32_t write(const byte *ptr, int32_t count) = 0;
	virtual int64_t tell() const  = 0;
	virtual int64_t getBufferSize() const {
		return -1;
	}
};

}

#endif /* WRITABLE_H_ */
