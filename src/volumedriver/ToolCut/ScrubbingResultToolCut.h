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

#ifndef SCRUBBING_RESULT_TOOLCUT_H_
#define SCRUBBING_RESULT_TOOLCUT_H_
#include <boost/python/list.hpp>
#define SCRUBRESULT_NO_THROW_ON_DIFFERENT_VERSION
#include "../Scrubber.h"
namespace toolcut
{
class ScrubbingResultToolCut
{
public:
    ScrubbingResultToolCut(const std::string& name);

    ScrubbingResultToolCut(boost::python::object& backend,
                           const std::string& nspace,
                           const std::string& filename);

    ScrubbingResultToolCut(const ScrubbingResultToolCut&) = delete;

    ScrubbingResultToolCut&
    operator=(const ScrubbingResultToolCut&) = delete;

    std::string
    getSnapshotName() const;

    static bool
    isScrubbingResultString(const std::string name);

    boost::python::list
    getTLogsIn() const;

    boost::python::list
    getTLogsOut() const;

    boost::python::list
    getRelocations() const;

    uint64_t
    getNumberOfRelocations() const;

    boost::python::list
    getSCONamesToBeDeleted() const;

    boost::python::list
    getPrefetch() const;

    boost::python::list
    getNewSCONames() const;

    uint64_t
    getReferenced() const;

    uint64_t
    getStored() const;

    uint32_t
    getVersion() const;

    std::string
    str() const;

    std::string
    repr() const;

private:
    scrubbing::ScrubberResult scrub_result;

};


}


#endif // SCRUBBING_RESULT_TOOLCUT_H_

// Local Variables: **
// compile-command: " scons -u -D -j 4" **
// mode: c++ **
// End: **
