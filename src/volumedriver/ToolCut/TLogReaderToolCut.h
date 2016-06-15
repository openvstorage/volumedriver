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

#ifndef TLOG_INFO_H
#define TLOG_INFO_H

#include <boost/filesystem.hpp>
#include <boost/python/list.hpp>
#include <backend-python/ConnectionInterface.h>
#include "../TLogReaderInterface.h"



namespace toolcut
{
namespace vd = volumedriver;
namespace fs = boost::filesystem;
namespace bpy = boost::python;

class TLogReaderToolCut
{
public:
    // More constructor -- e.g list of tlogs...

    explicit TLogReaderToolCut(const fs::path& path);

    TLogReaderToolCut(boost::python::object& backend_config,
                      const std::string& nspace,
                      const std::string& name,
                      const fs::path& dst_path);


    TLogReaderToolCut(const TLogReaderToolCut&) = delete;
    TLogReaderToolCut& operator=(const TLogReaderToolCut&) = delete;

    bpy::list
    SCONames() const;

    void
    rewind();

    static bool
    isTLog(const std::string& tlog);

    void
    forEach(const boost::python::object&,
            const boost::python::object&,
            const boost::python::object&,
            const boost::python::object&);

    void
    forEachEntry(const boost::python::object&);

    static boost::python::object object_;

    std::string
    str() const;

    std::string
    repr() const;


private:
    std::unique_ptr<vd::TLogReaderInterface> tlog_reader_;
    const fs::path path_;


};
}

#endif // TLOG_INFO_H

// Local Variables: **
// mode: c++ **
// End: **
