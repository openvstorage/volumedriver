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

/*
 * Protocol.h
 *
 */

#ifndef PROTOCOL_H_
#define PROTOCOL_H_

#include "Runnable.h"

namespace fungi {

class Protocol : public Runnable {
public:
	DECLARE_LOGGER("Protocol");
	Protocol(){}
	virtual ~Protocol(){}
	virtual void start() = 0;
};

}

#endif /* PROTOCOL_H_ */
