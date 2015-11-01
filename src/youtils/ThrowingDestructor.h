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

#ifndef THROW_IN_DESTRUCTOR_H_
#define THROW_IN_DESTRUCTOR_H_
#include "BooleanEnum.h"

BOOLEAN_ENUM(EnableThrowInDestructor)

// See the ThrowingDestructorTest for an example of how to use this
// https://akrzemi1.wordpress.com/2011/09/21/destructors-that-thro/w
namespace youtils
{
class ThrowingDestructor
{
protected:
    ThrowingDestructor()
        : throw_in_destructor_(EnableThrowInDestructor::F)
    {}

    ~ThrowingDestructor() noexcept(false)
    {}


public:
    void
    throw_in_destructor(EnableThrowInDestructor in)
    {
        throw_in_destructor_ = in;
    }
protected:
    bool
    throw_in_destructor()
    {
        return T(throw_in_destructor_);
    }

private:
    EnableThrowInDestructor throw_in_destructor_;

};

}

#endif // THROW_IN_DESTRUCTOR_H_
