// This file is dual licensed GPLv2 and Apache 2.0.
// Active license depends on how it is used.
//
// Copyright 2016 iNuron NV
//
// // GPL //
// This file is part of OpenvStorage.
//
// OpenvStorage is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with OpenvStorage. If not, see <http://www.gnu.org/licenses/>.
//
// // Apache 2.0 //
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "OrbParameters.h"
using std::make_tuple;

#define T make_tuple

std::vector<std::tuple<std::string, std::string, std::string> > orb_args =
{
    T("traceLevel",
      "1",
      "level 0	critical errors only\n"
      "level 1	informational messages only\n"
      "level 2	configuration information and warnings\n"
      "level 5	notifications when server threads are created and communication endpoints are shutdown"
      "level 10	execution and exception traces"
      "level 25	trace each send or receive of a GIOP message"
      "level 30	dump up to 128 bytes of each GIOP message"
      "level 40	dump complete contents of each GIOP message"),
    T("traceExceptions",
                    "0",
                    "If the traceExceptions parameter is set true, all system exceptions are logged as they are thrown, along with details about where the exception is thrown from. This parameter is enabled by default if the traceLevel is set to 10 or more."),

    T("traceInvocations",
                    "0",
                    "If the traceInvocations parameter is set true, all local and remote invocations are logged, in addition to any logging that may have been selected with traceLevel."),
    T("traceInvocationReturns",
                    "0",
                    "If the traceInvocationReturns parameter is set true, a log message is output as an operation invocation returns. In conjunction with traceInvocations and traceTime (described below), this provides a simple way of timing CORBA calls within your application."),
    T("traceThreadId",
                    "1",
                    "If traceThreadId is set true, all trace messages are prefixed with the id of the thread outputting the message. This can be handy for making sense of multi-threaded code, but it adds overhead to the logging so it can be disabled."),
    T("traceTime",
                    "1",
                    "If traceTime is set true, all trace messages are prefixed with the time. This is useful, but on some platforms it adds a very large overhead, so it can be turned off."),
    T("traceFile",
                    "",
       "omniORB’s tracing is normally sent to stderr. If traceFile it set, the specified file name is used for trace messages."),
    T("dumpConfiguration",
      "0",
      "If set true, the ORB dumps the values of all configuration parameters at start-up."),
    T("scanGranularity",
      "5",
      "explained in chapter 6, omniORB regularly scans incoming and outgoing connections, so it can close unused ones. This value is the granularity in seconds at which the ORB performs its scans. A value of zero turns off the scanning altogether."),
    T("nativeCharCodeSet",
      "ISO-8859-1",
      "The native code set the application is using for char and string. See chapter 8."),
    T("nativeWCharCodeSet",
      "UTF-16",
      "The native code set the application is using for wchar and wstring. See chapter 8."),
    T("copyValuesInLocalCalls",
      "1",
      "Determines whether valuetype parameters in local calls are copied or not. See chapter 11."),
    T("abortOnInternalError",
      "0",
      "If this is set true, internal fatal errors will abort immediately, rather than throwing the omniORB::fatalException exception. This can be helpful for tracking down bugs, since it leaves the call stack intact."),
    T("abortOnNativeException",
      "0",
      "On Windows, ‘native’ exceptions such as segmentation faults and divide by zero appear as C++ exceptions that can be caught with catch (...). Setting this parameter to true causes such exceptions to abort the process instead."),
    T("maxSocketSend",
      "",
      "On some platforms, calls to send() and recv() have a limit on the buffer size that can be used. These parameters set the limits in bytes that omniORB uses when sending / receiving bulk data.\n"
      "The default values are platform specific. It is unlikely that you will need to change the values from the defaults.\n"
      "The minimum valid limit is 1KB, 1024 bytes."),
    T("maxSocketRecv",
      "",
      "On some platforms, calls to send() and recv() have a limit on the buffer size that can be used. These parameters set the limits in bytes that omniORB uses when sending / receiving bulk data.\n"
      "The default values are platform specific. It is unlikely that you will need to change the values from the defaults.\n"
      "The minimum valid limit is 1KB, 1024 bytes."),
    T("socketSendBuffer",
      "-1",
      "On Windows, there is a kernel buffer used during send operations. A bug in Windows means that if a send uses the entire kernel buffer, a select() on the socket blocks until all the data has been acknowledged by the receiver, resulting in dreadful performance. This parameter modifies the socket send buffer from its default (8192 bytes on Windows) to the value specified. If this parameter is set to -1, the socket send buffer is left at the system default.\n"
      "On Windows, the default value of this parameter is 16384 bytes; on all other platforms the default is -1."),
    T("validateUTF8",
      "0",
      "When transmitting a string that is supposed to be UTF-8, omniORB usually passes it directly, assuming that it is valid. With this parameter set true, omniORB checks that all UTF-8 strings are valid, and throws DATA_CONVERSION if not.")

};
#undef T
