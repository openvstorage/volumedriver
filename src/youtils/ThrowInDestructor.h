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

#ifndef THROW_IN_DESTRUCTOR_H_
#define THROW_IN_DESTRUCTOR_H_
#include "BooleanEnum.h"

VD_BOOLEAN_ENUM(EnableThrowInDestructor)

// See the test for an example of how to use this
  // https://akrzemi1.wordpress.com/2011/09/21/destructors-that-thro/w
namespace youtils
{
class ThrowingDestructor
{
    ThrowInDestructor()
        : throw_in_destructor(EnableThrowInDestructor::F);

    ~ThrowInDestructor() noexcept(false)
    {}


protected:
    void
    throw_in_destructor(EnableThrowInDestructor in)
    {
        throw_in_destructor = in;
    }

    bool
    throw_in_destructor()
    {
        return T(throw_in_destructor);
    }



    EnableThrowInDestructor throw_in_destructor_;

};

}

#endif // THROW_IN_DESTRUCTOR_H_
