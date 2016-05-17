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

#ifndef MAIN_EVENT_H_
#define MAIN_EVENT_H_

#include "Logger.h"

namespace youtils
{

class MainEvent
{
public:
    MainEvent(const std::string& what,
              Logger::logger_type& logger);

    ~MainEvent();

private:
    const std::string what_;
    Logger::logger_type& logger_;
};

#define MAIN_EVENT(arg)                                                 \
    ::std::stringstream m_e_s_s__;                                      \
    m_e_s_s__ << arg;                                                   \
    ::youtils::MainEvent __main_event__(m_e_s_s__.str(), getLogger__());

}

#endif // MAIN_EVENT_H_
