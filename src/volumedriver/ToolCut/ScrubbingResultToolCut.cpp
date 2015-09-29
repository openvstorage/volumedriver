// Copyright 2015 Open vStorage NV
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

#include <boost/python.hpp>
#include "ScrubbingResultToolCut.h"
#include <fstream>
#include <boost/python/tuple.hpp>

#include <youtils/Assert.h>
#include <youtils/FileUtils.h>

#include <backend-python/ConnectionInterface.h>

#include <volumedriver/Types.h>

namespace toolcut
{

namespace fs = boost::filesystem;
namespace vd = volumedriver;

ScrubbingResultToolCut::ScrubbingResultToolCut(const std::string& filename)
{
    std::ifstream ifs(filename);
    boost::archive::text_iarchive ia(ifs);
    ia >> scrub_result;
}

TODO("AR: drop filename argument");

ScrubbingResultToolCut::ScrubbingResultToolCut(boost::python::object& backend,
                                               const std::string& nspace,
                                               const std::string& /* filename */)
{
    fs::path p(vd::FileUtils::create_temp_file_in_temp_dir("scrubber_result"));
    ALWAYS_CLEANUP_FILE(p);
    backend.attr("read")(nspace,
                         p.string(),
                         vd::snapshotFilename());

    std::ifstream ifs(p.string());
    boost::archive::text_iarchive ia(ifs);
    ia >> scrub_result;
}


std::string
ScrubbingResultToolCut::getSnapshotName() const
{
    return scrub_result.snapshot_name;
}

boost::python::list
ScrubbingResultToolCut::getTLogsIn() const
{
    boost::python::list result;
    for (const auto& tlog_name : scrub_result.tlog_names_in)
    {
        result.append(tlog_name);
    }
    return result;
}

boost::python::list
ScrubbingResultToolCut::getTLogsOut() const
{
    boost::python::list result;
    for (const auto& tlog : scrub_result.tlogs_out)
    {
        result.append(tlog.getName());
    }
    return result;
}

boost::python::list
ScrubbingResultToolCut::getRelocations() const
{
    boost::python::list result;
    for (const auto& tlog_name : scrub_result.relocs)
    {
        result.append(tlog_name);
    }
    return result;
}

uint64_t
ScrubbingResultToolCut::getNumberOfRelocations() const
{
    return scrub_result.relocNum;
}

boost::python::list
ScrubbingResultToolCut::getSCONamesToBeDeleted() const
{
    boost::python::list result;
    for (const auto& sco : scrub_result.sconames_to_be_deleted)
    {
        result.append(sco.str());
    }
    return result;
}

boost::python::list
ScrubbingResultToolCut::getPrefetch() const
{
    boost::python::list result;
    for (const auto& val : scrub_result.prefetch)
    {
        result.append(boost::python::make_tuple(val.first.str(), val.second));
    }
    return result;
}

boost::python::list
ScrubbingResultToolCut::getNewSCONames() const
{
    if(scrub_result.version == 3 or
       scrub_result.version == 4)
    {

        boost::python::list result;
        for (const auto& sco : scrub_result.new_sconames)
        {
            result.append(sco.str());
        }
        return result;
    }
    else
    {
        throw fungi::IOException("Not available in this version");
    }
}

bool
ScrubbingResultToolCut::isScrubbingResultString(const std::string name)
{
    return scrubbing::Scrubber::isScrubbingResultString(name);
}

uint64_t
ScrubbingResultToolCut::getReferenced() const
{
    uint64_t s = 0;
    for (const auto& t : scrub_result.tlogs_out)
    {
        s += t.backend_size();
    }

    return s;
}

uint64_t
ScrubbingResultToolCut::getStored() const
{

    throw fungi::IOException("Not available in this version");
}

uint32_t
ScrubbingResultToolCut::getVersion() const
{
    return scrub_result.version;
}

std::string
ScrubbingResultToolCut::str() const
{
    std::stringstream ss;
    ss << "snapshotName " << getSnapshotName() << std::endl;
    return ss.str();
}

std::string
ScrubbingResultToolCut::repr() const
{
    return "< ToolCut.ScrubbingResult " + str() + ">";
}

}

// Local Variables: **
// mode: c++ **
// End: **
