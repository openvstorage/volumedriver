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

#ifndef PAGE_SORTING_GENERATOR_H_
#define PAGE_SORTING_GENERATOR_H_

#include "CachedMetaDataPage.h"
#include "Entry.h"
#include "TLogReaderInterface.h"

#include <boost/bimap.hpp>
#include <boost/bimap/multiset_of.hpp>

#include <youtils/Generator.h>

namespace volumedriver
{

using PageData = std::vector<Entry>;

// XXX: get rid of this.
using namespace boost::bimaps;
using namespace boost;

class PageSortingGenerator_
    : public youtils::Generator<PageData>
{
    // Generator that gives out entries sorted per page for fast processing.
    // The entries read from the tlog are sorted into the cachedPages_ vector.
    // We can choose which page to give out through the current() call, and we take the heuristic of always
    // returning the page with the most entries at that point, thereby trying to minimize the total number of pages
    // given out and number of cache misses on the metadatastore.
    // While cachedPages_ contains enough information to find the most populated page, scanning this requires linear time.
    // Therefore we keep a bimap, sizesMap_, which keeps track of the number of entries in each page in an ordered fashion,
    // and use this sizesMap_ to select current.

    // class invariants between public calls (violated on update)
    //        (cached_ == 0) <=> finished() <=> not current_.get()
    //        (cached_ == 0) => tlogfinished_

public:
    PageSortingGenerator_(uint64_t cachedMax,
                          std::shared_ptr<TLogReaderInterface> reader)
        : max_(cachedMax)
        , reader_(reader)
    {
        VERIFY(max_ > 0); // convert to throw
        update_();
    }

    void
    next()
    {
        //will throw if next() is called when already finished
        cached_ -= current_size_;
        current_size_ = 0;
        update_();
    }

    PageData&
    current()
    {
        return current_;
    }

    bool
    finished()
    {
        return cached_ == 0;
    }

private:
    DECLARE_LOGGER("PageSortingGenerator_");

    //factoring out common code between construction and next()
    void
    update_()
    {
        getMoreData_();
        updateCurrent_();
    }

    void
    getMoreData_()
    {
        if (not tlogfinished_)
        { //get some more data in
            VERIFY(cached_ < max_);

            const Entry* e;
            while((e = reader_->nextLocation()))
            {
                PageAddress pageAddress = CachePage::pageAddress(e->clusterAddress());
                makePagesUpTo_(pageAddress);
                cachedPages_[pageAddress].push_back(*e);

                auto it = sizesMap_.left.find(pageAddress);
                VERIFY(it != sizesMap_.left.end());

                size_t sz = cachedPages_[pageAddress].size();
                sizesMap_.left.replace_data(it, sz);

                if (++cached_ >= max_)
                {
                    break;
                }
            }

            if(not e)
            {
                tlogfinished_ = true;
            }
        }
    }

    void
    updateCurrent_()
    {
        if(cached_ > 0)
        {
            VERIFY(not sizesMap_.empty());

            auto iter = sizesMap_.right.end();
            iter--; //point to highest element
            size_t size = iter->first;

            VERIFY(size > 0);

            uint64_t maxindex = iter->second;

            sizesMap_.right.replace_key(iter, 0);

            current_ = PageData();
            std::swap(current_,
                      cachedPages_[maxindex]);
            current_size_ = current_.size();
        }
    }

    void makePagesUpTo_(uint64_t p_addr)
    {
        for (uint64_t addr = cachedPages_.size(); addr <= p_addr; addr++)
        {
            cachedPages_.emplace_back();
            sizesMap_.right.insert(std::pair<size_t, uint32_t>( 0, addr));
        }
    }

    bool tlogfinished_ = false;
    const uint64_t max_;
    uint64_t cached_ = 0;
    std::shared_ptr<TLogReaderInterface> reader_;
    PageData current_;
    size_t current_size_ = 0;
    std::vector<PageData> cachedPages_;
    bimap< uint32_t, multiset_of<size_t> > sizesMap_;
};
}

#endif // PAGE_SORTING_GENERATOR_H_
// Local Variables: **
// mode: c++ **
// End: **
