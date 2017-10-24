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

#ifndef VD_PAGE_GENERATOR_H_
#define VD_PAGE_GENERATOR_H_

#include "Types.h"

#include <vector>
#include <memory>
#include <unordered_map>

#include <boost/shared_ptr.hpp>

#include <youtils/Generator.h>
#include <youtils/Logging.h>

namespace volumedriver
{

struct Entry;
struct TLogReaderInterface;

using PageData = std::vector<Entry>;
using PageDataPtr = boost::shared_ptr<PageData>;

class PageGenerator
    : public youtils::Generator<PageDataPtr>
{
    // Generator that groups entries in pages, which are however not sorted
    // (opposed to PageSortingGenerator_).
public:
    PageGenerator(uint64_t cached_max,
                  const std::shared_ptr<TLogReaderInterface>& reader)
        : cached_max_(cached_max)
        , reader_(reader)
    {
        VERIFY(cached_max_ > 0);
        update_();
    }

    ~PageGenerator() = default;

    PageGenerator(const PageGenerator&) = delete;

    PageGenerator&
    operator=(const PageGenerator&) = delete;

    void
    next() override final
    {
        VERIFY(current_);
        cached_ -= current_->size();
        current_ = nullptr;
        update_();
    }

    PageDataPtr&
    current() override final
    {
        return current_;
    }

    bool
    finished() override final
    {
        return cached_ == 0;
    }

private:
    DECLARE_LOGGER("PageGenerator");

    std::unordered_map<PageAddress, PageDataPtr> pages_;
    bool tlog_finished_ = false;
    const uint64_t cached_max_;
    uint64_t cached_ = 0;
    std::shared_ptr<TLogReaderInterface> reader_;
    PageDataPtr current_;

    void
    update_()
    {
        get_more_data_();
        update_current_();
    }

    void
    get_more_data_();

    void
    update_current_()
    {
        if (cached_ > 0)
        {
            VERIFY(not pages_.empty());
            auto it = pages_.begin();

            current_ = it->second;
            VERIFY(not current_->empty());

            pages_.erase(it);
        }
        else
        {
            VERIFY(pages_.empty());
            current_ = nullptr;
        }
    }
};

}

#endif // VD_PAGE_GENERATOR_H_

// Local Variables: **
// mode: c++ **
// End: **
