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

#ifndef BUILD_INFO_H_
#define BUILD_INFO_H_

// The C file that holds the definitions of these variables is generated
// by the build process.

#ifdef __cplusplus
extern "C"
{
#endif

struct BuildInfo
{

    static const char* version;

    static const char* version_revision;

    static const char* revision;

    static const char* branch;

    static const char* buildTime;

    static const char* repository_url;
};

#ifdef __cplusplus
}
#endif

#endif // !BUILD_INFO_H_

/*
// Local Variables: **
// mode: c++ **
// End: **
*/
