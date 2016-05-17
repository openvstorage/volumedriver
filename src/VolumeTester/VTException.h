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

#ifndef VT_EXCEPTION_H
#define VT_EXCEPTION_H
#include <exception>
#include <string>
#include <sstream>

class VTException : public std::exception
{
  public:
    VTException(const char *msg,
                const char *name = 0,
                int error_code = 0);

    VTException();

	virtual ~VTException() throw ();
	virtual const char* what() const throw ();
	int getErrorCode() const;
  private:
	int error_code_;
	std::string msg_;
};

#endif // VT_EXCEPTION_H
