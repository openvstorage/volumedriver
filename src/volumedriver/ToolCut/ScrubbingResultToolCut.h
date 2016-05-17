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
