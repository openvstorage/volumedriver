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

#ifndef prudence_H_
#define prudence_H_
#include <sys/prctl.h>
enum return_values
{
    Success = 0,
    CouldNotSetNotification = 1,
    CouldNotSetSignalHandler = 2,
    CouldNotBlockSignals = 3,
    PollError = 4,
    StartScriptNotOk = 5,
    ProblemRunningTheScript = 6

};

const static int parent_dies_signal = SIGUSR1;
#endif // prudence_H_
