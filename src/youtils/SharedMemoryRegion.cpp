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

#include "Assert.h"
#include "Catchers.h"
#include "SharedMemoryRegion.h"

#include <boost/lexical_cast.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/random_device.hpp>
#include <boost/random/uniform_int_distribution.hpp>

namespace youtils
{

namespace bi = boost::interprocess;
namespace br = boost::random;

namespace
{

const std::string prefix("/ovs-mds-");

const std::string
shmem_object_name(SharedMemoryRegionId id)
{
    return prefix + boost::lexical_cast<std::string>(id);
}

SharedMemoryRegionId
shmem_object_name_to_id(const std::string& s)
{
    return boost::lexical_cast<SharedMemoryRegionId>(s.substr(prefix.size()));
}

bi::shared_memory_object
create_shmem_object(size_t size)
{
    // we're not using youtils::SourceOfUncertainty here as that one is seeded with
    // std::time() which lead to collisions in tests:
    // (1) C: create region with ID X
    // (2) S: open region with X
    // (3) C: unlink region X (S still has a reference!)
    // (4) C: create new region with ID X
    // (5) S: unlink and close region X (due to the old reference obtained in (2)),
    //        attempt to open region X
    const br::random_device::result_type seed = br::random_device()();
    br::mt19937 gen(seed);
    br::uniform_int_distribution<uint64_t> dist(1,
                                                std::numeric_limits<uint64_t>::max());

    while (true)
    {
        const SharedMemoryRegionId id(dist(gen));

        try
        {
            bi::shared_memory_object shmem(bi::create_only,
                                           shmem_object_name(id).c_str(),
                                           bi::read_write);
            shmem.truncate(size);
            return shmem;
        }
        catch (bi::interprocess_exception& e)
        {
            if (e.get_native_error() == EEXIST)
            {
                continue;
            }

            throw;
        }
    }
}

}

SharedMemoryRegion::SharedMemoryRegion(size_t size)
try
    : object_(create_shmem_object(size))
    , region_(object_,
              bi::read_write)
    , id_(shmem_object_name_to_id(object_.get_name()))
{
    LOG_TRACE("Created shmem region " << id_ << ", size " << this->size())
}
CATCH_STD_ALL_LOG_RETHROW("Failed to create shmem region");

SharedMemoryRegion::SharedMemoryRegion(SharedMemoryRegionId id)
try
    : object_(bi::open_only,
              shmem_object_name(id).c_str(),
              bi::read_write)
    , region_(object_,
              bi::read_write)
    , id_(id)
{
    LOG_TRACE("Opened shmem region " << id_ << ", size " << this->size())
}
CATCH_STD_ALL_LOG_RETHROW("Failed to open shmem region " << id);

SharedMemoryRegion::~SharedMemoryRegion()
{
    if (owner_)
    {
        LOG_TRACE("Unlinking shmem region " << id_);
        // we deliberately ignore errors here as all users of the shmem will invoke this
        // call.
        object_.remove(shmem_object_name(id_).c_str());
    }
}

SharedMemoryRegion::SharedMemoryRegion(SharedMemoryRegion&& other)
    : object_(std::move(other.object_))
    , region_(std::move(other.region_))
    , id_(other.id_)
{
    other.owner_ = false;
}

}
