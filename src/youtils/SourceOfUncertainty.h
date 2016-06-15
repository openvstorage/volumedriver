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
