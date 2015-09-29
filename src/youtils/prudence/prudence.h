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
