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

#ifndef SCOPE_EXIT_H
#define SCOPE_EXIT_H

#include <utility>

namespace youtils
{

template<typename T>
struct scope_exit
{
    scope_exit(T&& f) :
        f_(std::move(f))
    {}

    ~scope_exit()
    {
        f_();
    }

    T f_;
};

template<typename T>
scope_exit<T>
make_scope_exit(T&& f)
{
    return scope_exit<T>(std::move(f));
}

}

#endif // SCOPE_EXIT_H
