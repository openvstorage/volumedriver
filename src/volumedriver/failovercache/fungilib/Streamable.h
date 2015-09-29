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

#ifndef STREAMABLE_H_
#define STREAMABLE_H_

#include "defines.h"
#include <youtils/Logging.h>

namespace fungi {
class Streamable {
public:
    DECLARE_LOGGER("Streamable");
    /** @exception IOException */
    virtual	int32_t read(byte *buf, int32_t n) = 0;
    /** @exception IOException */
    virtual int32_t write(const byte *buf, int32_t n) = 0;
    /** @exception IOException */
    virtual void setCork() = 0;
    /** @exception IOException */
    virtual void clearCork() = 0;
    /** @exception IOException */
    virtual void getCork() = 0;
    virtual void setRequestTimeout(double timeout) = 0;
    virtual void close() = 0;
    virtual void closeNoThrow() = 0;
    virtual bool isClosed() const = 0;
    virtual ~Streamable() {}
};
}

#endif /* STREAMABLE_H_ */

// Local Variables: **
// mode: c++ **
// End: **
