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

#ifndef YT_LOGGER_PRIVATE_H_
#define YT_LOGGER_PRIVATE_H_

#include <string>

namespace youtils
{

// Used by both Logger.cpp and LoggerTest.cpp

#define LOGGER_ATTRIBUTE_ID "ID"

typedef std::string LOGGER_ATTRIBUTE_ID_TYPE;

#define LOGGER_ATTRIBUTE_SUBSYSTEM "SUBSYSTEM"

typedef std::string LOGGER_ATTRIBUTE_SUBSYSTEM_TYPE;

}

#endif // YT_LOGGER_PRIVATE_H_
