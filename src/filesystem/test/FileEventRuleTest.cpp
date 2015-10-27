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

#include "FileSystemEventTestSetup.h"
#include "FileSystemTestBase.h"

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include "../FileEventRule.h"
#include "../FrontendPath.h"

namespace volumedriverfstest
{

namespace bpt = boost::property_tree;
namespace ip = initialized_params;
namespace vfs = volumedriverfs;

class FileEventRuleTest
    : public FileSystemTestBase
    , public FileSystemEventTestSetup
{
protected:
    FileEventRuleTest()
        : FileSystemTestBase(FileSystemTestSetupParameters("FileEventRuleTest"))
        , FileSystemEventTestSetup("FileEventRuleTest")
    {}

    void
    add_file_event_rules(const std::vector<vfs::FileEventRule>& rules)
    {
        bpt::ptree pt;
        FileSystemTestBase::make_config_(pt, topdir_, local_node_id());
        FileSystemEventTestSetup::make_config(pt);

        ip::PARAMETER_TYPE(fs_file_event_rules)(rules).persist(pt);

        stop_fs();
        start_fs(pt);

        std::stringstream ss;
        bpt::write_json(ss, pt);
        LOG_TRACE(ss.str());
    }

    template<typename T>
    void
    check_file_event(const vfs::FrontendPath& fname,
                     const T& t)
    {
        const auto ext(get_event(t));
        EXPECT_EQ(fname.str(),
                  ext.path());
    }

    DECLARE_LOGGER("FileEventRuleTest");
 };

 TEST_F(FileEventRuleTest, mixed_bag)
 {
     if (use_amqp())
     {
         std::set<vfs::FileSystemCall> calls;
         calls.insert(vfs::FileSystemCall::Mknod);
         calls.insert(vfs::FileSystemCall::Write);
         calls.insert(vfs::FileSystemCall::Unlink);

         const vfs::FrontendPath fname("/some-file");
         std::vector<vfs::FileEventRule> rules { vfs::FileEventRule(calls, fname.str()) };

         add_file_event_rules(rules);

         create_file(fname);
         const std::string pattern("foo");
         write_to_file(fname, pattern, pattern.size(), 0);
         unlink(fname);

         check_file_event(fname,
                          events::file_create);

         check_file_event(fname,
                          events::file_write);

         check_file_event(fname,
                          events::file_delete);
    }
    else
    {
        LOG_WARN("Not running test as AMQP is not configured");
    }
}

TEST_F(FileEventRuleTest, rename)
{
    if (use_amqp())
    {
        std::set<vfs::FileSystemCall> calls;
        calls.insert(vfs::FileSystemCall::Rename);

        const vfs::FrontendPath from("/some-file");
        std::vector<vfs::FileEventRule> rules { vfs::FileEventRule(calls, from.str()) };

        add_file_event_rules(rules);

        create_file(from);

        const vfs::FrontendPath to("/some-renamed-file");

        EXPECT_EQ(0, rename(from, to));
        EXPECT_EQ(0, rename(to, from));

        {
            const events::FileRenameMessage msg(get_event(events::file_rename));

            EXPECT_EQ(from.str(),
                      msg.old_path());

            EXPECT_EQ(to.str(),
                      msg.new_path());
        }
        {
            const events::FileRenameMessage msg(get_event(events::file_rename));

            EXPECT_EQ(to.str(),
                      msg.old_path());

            EXPECT_EQ(from.str(),
                      msg.new_path());
        }
    }
    else
    {
        LOG_WARN("Not running test as AMQP is not configured");
    }
}

}
