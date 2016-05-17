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
