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

#include "../BackendTestSetup.h"
#include "../BackendException.h"
#include "../BackendConnectionInterface.h"
#include "BackendTestBase.h"

#include <boost/log/common.hpp>
#include <boost/log/sinks.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/shared_ptr.hpp>
#include <iostream>

#include <alba/alba_logger.h>

using namespace std;

namespace backend
{

class BackendLoggingTest
    : public BackendTestBase
{
public:
    BackendLoggingTest()
        : BackendTestBase("BackendLoggingTest")
    {}

    void
    SetUp() override final
    {
        BackendTestBase::SetUp();
        cm_->getConnection(); // Make sure the Alba_Connection is being set up
    }

    void
    TearDown() override final
    {
        BackendTestBase::TearDown();
    }

    void logMessage(alba::logger::AlbaLogLevel level, string message)
    {
        auto *logger = alba::logger::getLogger(level);
        if (nullptr != logger){
            message += "-test";
            (*logger)(level, message);
        }
    }

protected:
    DECLARE_LOGGER("BackendLoggingTest");
};


TEST_F(BackendLoggingTest, Logging)
{
    if (yt::Logger::loggingEnabled() and
        BackendTestSetup::backend_config().backend_type.value() == BackendType::ALBA)
    {
        LOG_INFO("Test started");

        string token = youtils::UUID().str();
        string fullToken = token + "-test";

        // Setup boost logging to use a specific ostringstream as sink
        typedef boost::log::sinks::synchronous_sink<boost::log::sinks::text_ostream_backend> text_sink;
        boost::shared_ptr<text_sink> sink = boost::make_shared<text_sink>();
        auto stream(boost::make_shared<std::ostringstream>());
        sink->locked_backend()->add_stream(stream);
        boost::log::core::get()->add_sink(sink);

        auto found = stream->str().find(token);
        ASSERT_TRUE(found == string::npos);
        logMessage(alba::logger::AlbaLogLevel::WARNING, token);
        sink->flush();
        found = stream->str().find(token);
        ASSERT_FALSE(found == string::npos);
        found = stream->str().find(fullToken);
        ASSERT_FALSE(found == string::npos);

        // Remove the sink again, cleaning everything up
        boost::log::core::get()->remove_sink(sink);
        sink->flush();
        sink.reset();

        LOG_INFO("Test finished");
    }
}

}

// Local Variables: **
// mode: c++ **
// End: **
