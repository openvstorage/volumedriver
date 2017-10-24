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

#include "CachedMetaDataPage.h"
#include "Entry.h"
#include "PageGenerator.h"
#include "TLogReaderInterface.h"

namespace volumedriver
{

void
PageGenerator::get_more_data_()
{
    if (not tlog_finished_)
    {
        VERIFY(cached_ < cached_max_);
        const Entry* e;
        while ((e = reader_->nextLocation()))
        {
            const PageAddress pa = CachePage::pageAddress(e->clusterAddress());
            auto it = pages_.find(pa);
            if (it == pages_.end())
            {
                std::tie(it, std::ignore) = pages_.emplace(pa,
                                                           boost::make_shared<PageData>());
            }

            it->second->push_back(*e);

            if (++cached_ >= cached_max_)
            {
                break;
            }
        }

        if (e == nullptr)
        {
            tlog_finished_ = true;
        }
    }
}

}
