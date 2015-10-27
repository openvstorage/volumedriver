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

#include "LockedArakoon.h"

namespace ara = arakoon;

namespace volumedriverfs
{

void
LockedArakoon::run_sequence(const char* desc,
                            LockedArakoon::PrepareSequenceFun&& prepare_fun,
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
