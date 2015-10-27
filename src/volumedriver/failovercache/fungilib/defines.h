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

#ifndef _DEFINES_H
#define	_DEFINES_H

// some basic data types and constants used everywhere
// in the code

// for size_t
#include <stdio.h>
// for int32_t and friends; note that most VC++ don't ship with a stdint.h
// therefor there is one included in src/lib/win32
#include <stdint.h>
#ifndef _WIN32
#include <unistd.h>
#endif

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN // exclude unnecesary headers from windows.h
  #define NOMINMAX // don't emit min and max macro's please: they break std::min and std::max
  #include <winsock2.h> // basic windows socket API
  #include <mswsock.h> // MS extension on winsock
  #include <windows.h> // generic windows API
  #include <ws2tcpip.h> // advanced winsock API

  #define sleep(time) Sleep(time*1000) // windows doesn't define sleep(), but it has Sleep()

  int gettimeofday(struct timeval* tp, void* tzp); // implemented in gettimeofday.c

  // some types windows doesn't have:
  typedef int in_addr_t;
  typedef unsigned short in_port_t;
  typedef int socklen_t;
#endif

typedef unsigned char byte;

#define _SEED_BYTES 5
#define _CBID_SIZE 20
#define SB_FACTOR 4096

#define MAX_FILE_NAME_SIZE 255

#define ONE_MEGA_BYTE (1L << 20)
#define ONE_GIGA_BYTE (1LL << 30)
#define TWO_GIGA_BYTE (2LL << 30)


// based on kernel NIPQUAD; use as argument of printf style
// io, and use %d.%d.%d.%d in the format string
#define NIPQUAD(addr) \
	((unsigned char *)&addr)[0], \
	((unsigned char *)&addr)[1], \
	((unsigned char *)&addr)[2], \
	((unsigned char *)&addr)[3]

#if defined(__cplusplus)
// std::for_each(p->begin(), p->end(), deleteIt<T *> ());
template<class T> struct deleteIt {
	void operator()(T x) {
		delete x;
	}
};
#endif

#endif	/* _DEFINES_H */
