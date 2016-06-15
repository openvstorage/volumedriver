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

#include "LockedArakoon.h"

namespace ara = arakoon;

namespace youtils
{

void
LockedArakoon::run_sequence(const char* desc,
                            LockedArakoon::PrepareSequenceFun prepare_fun,
                            RetryOnArakoonAssert retry_on_assert)
{
    // CAUTION! When using this function you need to make sure to
    // check the preconditions on arakoon that would make the sequence
    // of actions succeed without concurrent updates.
    // Otherwize we're in for an pseudo-infinite loop.


    uint32_t tries = 0;

    while (true)
    {
        ara::sequence seq;
        prepare_fun(seq);

        try
        {
            boost::lock_guard<lock_type> g(lock_);

            arakoon_->synced_sequence(seq);
            LOG_INFO(desc << " succeeded after " << (tries + 1) << " attempt(s)");
            return;
        }
        catch (ara::error_assertion_failed&)
        {
            static const uint32_t max_retries = 10000;
            if (++tries < max_retries and T(retry_on_assert))
            {
                LOG_WARN(desc << " failed due to concurrent arakoon update, attempt " <<
                         tries << ". Retrying.");
            }
            else
            {
                LOG_ERROR(desc << " failed after " << tries
                          << " attempt(s), retry requested: "
                          << (T(retry_on_assert) ? "Yes" : "No"));
                throw;
            }
        }
    }
}

}
