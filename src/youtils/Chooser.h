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

#ifndef CHOOSER_H_
#define CHOOSER_H_
#include "Assert.h"
#include <boost/random/discrete_distribution.hpp>
#include <boost/random/mersenne_twister.hpp>

namespace youtils
{
template<typename T,
         typename G = boost::random::mt19937>
class Chooser
{
public:
    Chooser(const std::vector<std::pair<T, double> >& choices)
        : dist_(get_odds(choices))
        , choices_(get_choices(choices))
    {
    }

    Chooser(const std::vector<std::pair<T, unsigned> >& choices)
        : dist_(get_weights(choices))
        , choices_(get_choices(choices))
    {
    }

    const T&
    operator()()
    {
        return choices_[dist_(gen_)];
    }

    G gen_;
    DECLARE_LOGGER("Chooser");

private:

    // A range adaptor might be a cool exercise here.
    std::vector<double>
    get_odds(const std::vector<std::pair<T, double> >& choices)
    {
        std::vector<double> res;
        res.reserve(choices.size());
        double acc = 0;

        for(size_t i = 0; i < choices.size(); ++i)
        {
            double val = choices[i].second;
            res.push_back(val);
            acc += val;

        }
        VERIFY(acc == 1.0);
        return res;
    }

    std::vector<unsigned>
    get_weights(const std::vector<std::pair<T, unsigned> >& choices)
    {
        std::vector<unsigned> res;
        res.reserve(choices.size());

        for(size_t i = 0; i < choices.size(); ++i)
        {
            res.push_back(choices[i].second);
        }
        return res;
    }

    template <typename d>
    std::vector<T>
    get_choices(const std::vector<std::pair<T, d> >& choices)
    {
        std::vector<T> res;
        res.reserve(choices.size());
        for(size_t i = 0; i < choices.size(); ++i)
        {
            res.push_back(choices[i].first);
        }
        return res;
    }
    boost::random::discrete_distribution<> dist_;
    std::vector<T> choices_;
};

}

#endif // CHOOSER_H_

// Local Variables: **
// compile-command: "scons -D --kernel_version=system --ignore-buildinfo -j 5" **
// mode: c++ **
// End: **
