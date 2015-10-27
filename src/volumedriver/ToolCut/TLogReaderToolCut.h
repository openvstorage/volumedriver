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
