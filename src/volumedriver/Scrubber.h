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

#ifndef SCRUBBER_H
#define SCRUBBER_H

#include "SCO.h"
#include "ScrubbingTypes.h"
#include "SnapshotPersistor.h"
#include "Types.h"
#include "VolumeConfig.h"

#include <memory>

#include "youtils/Serialization.h"
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/assume_abstract.hpp>

#include "backend/BackendConfig.h"
#include "backend/BackendConnectionManager.h"
#include "youtils/WithGlobalLock.h"

#include <youtils/Logging.h>

namespace backend
{

class BackendInterface;

}

namespace scrubbing
{

class ScrubberException
    : public std::exception
{
public:
    enum ExitCode
    {
        // 1 is already used in the scrubber to show exit with problem
        NoCleanup = 3,
        Cleanup   = 4
    };

    ~ScrubberException() throw()
    {}

    ScrubberException(const std::string& what,
                      const ExitCode exit_code,
                      const std::string& result = "");


    virtual const char* what() const throw();
    ExitCode
    getExitCode() const
    {
        return exit_code_;
    }

private:
    ExitCode exit_code_;
    std::string what_;
    std::string result_;
};

struct ScrubberArgs
{
    ScrubberArgs()
    {}

    ScrubberArgs(const ScrubberArgs& other)
    {
        clone(other);
    }

    ScrubberArgs&
    operator=(const ScrubberArgs& other)
    {
        return clone(other);
    }

    std::unique_ptr<const backend::BackendConfig> backend_config;

    const backend::Namespace
    getNS() const
    {
        return backend::Namespace(name_space);
    }

    /* Namespace to scrub */
    std::string name_space;
    /* Directory to use for temporary files */
    std::string scratch_dir;
    /* Snapshot to scrub */
    std::string snapshot_name;
    /* size of a region in clusters as a power of two*/
    RegionExponent region_size_exponent;
    /* size of a SCO in number of clusters */
    uint16_t sco_size;

    /* size of a cluster in bytes as a power of two */
    volumedriver::ClusterExponent cluster_size_exponent;

    /* fill ratio as a number 0-1, sco's with larger fillratios will not be scrubbed */
    float fill_ratio;

    /* if true applies the scrubbing work immediately */
    bool apply_immediately;

private:
    ScrubberArgs&
    clone(const ScrubberArgs& other)
    {
        backend_config = other.backend_config->clone();
        name_space = other.name_space;
        scratch_dir = other.scratch_dir;
        snapshot_name = other.snapshot_name;
        region_size_exponent = other.region_size_exponent;
        sco_size = other.sco_size;
        fill_ratio = other.fill_ratio;
        return *this;
    }
};

struct ScrubberResult
{
    std::string snapshot_name;
    volumedriver::OrderedTLogNames tlog_names_in;
    std::vector<volumedriver::TLog> tlogs_out;
    std::vector<std::string> relocs;
    uint64_t relocNum;
    std::vector<volumedriver::SCO> sconames_to_be_deleted;
    std::vector<std::pair<volumedriver::SCO, float> > prefetch;
    std::vector<volumedriver::SCO> new_sconames;
    uint32_t version;
};

class Scrubber
{
public:
    explicit Scrubber(const ScrubberArgs& args,
                      bool verbose = true);

    void
    operator()();

    static bool
    isScrubbingResultString(const std::string& name);

    const std::string&
    getScrubbingResultName()
    {
        return scrubbing_result_name;
    }

private:
    DECLARE_LOGGER("Scrubber");

    static const char scrub_result_string[];

    const ScrubberArgs& args_;
    volumedriver::BackendInterfacePtr backend_interface_;

    std::unique_ptr<volumedriver::SnapshotPersistor> snapshot_persistor_;
    volumedriver::SnapshotNum snapshot_num_;

    ScrubberResult result_;
    bool verbose_;
    uint16_t minimum_number_of_used_entries;
    std::string scrubbing_result_name;

    void
    SetupBackend();
};

}

namespace boost
{
namespace serialization
{

template<class Archive>
void serialize(Archive& ar,
               scrubbing::ScrubberResult& res,
               const unsigned int version)
{
    if(version < 5)
    {
        throw youtils::SerializationVersionException("ScrubberResult", version, 5, 5);
    }
    else
    {
        ar & res.snapshot_name;
        ar & res.tlog_names_in;
        ar & res.tlogs_out;
        ar & res.relocs;
        ar & res.relocNum;
        ar & res.sconames_to_be_deleted;
        ar & res.prefetch;
        ar & res.new_sconames;
        res.version = version;
    }
}

}

}

BOOST_CLASS_VERSION(scrubbing::ScrubberResult, 5)

#endif // SCRUBBER_H

// Local Variables: **
// mode: c++ **
// End: **
