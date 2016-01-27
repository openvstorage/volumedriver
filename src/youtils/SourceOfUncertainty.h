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

#ifndef SOURCE_OF_UNCERTAINTY_H
#define SOURCE_OF_UNCERTAINTY_H

#include <ctime>
#include <limits>

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>

namespace youtils
{
class SourceOfUncertainty
{
public:
    SourceOfUncertainty()
        :gen(std::time(0))
    {}

    template<typename T>
    T
    operator()(const T& t1, const T& t2)
    {
        boost::random::uniform_int_distribution<T> dist(t1, t2);
        return dist(gen);
    }

    template<typename T>
    T
    operator()()
    {
        boost::random::uniform_int_distribution<T> dist(std::numeric_limits<T>::min(),
                                                        std::numeric_limits<T>::max());
        return dist(gen);
    }

    template<typename T>
    T
    operator()(const T& t)
    {
        boost::random::uniform_int_distribution<T> dist(static_cast<T>(0), t);
        return dist(gen);
    }

    template<typename Distribution,
             typename T>
    T
    operator()(Distribution& dist)
    {
        return dist(gen);
    }

private:
    boost::random::mt19937 gen;
};


}
#endif // SOURCE_OF_UNCERTAINTY_H
