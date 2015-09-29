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

#include <youtils/TestMainHelper.h>
#include "BackendConfig.h"

namespace backend
{

namespace yt = youtils;
namespace be = backend;
namespace fs = boost::filesystem;
namespace po = boost::program_options;


struct BackendConfigurationData
{
    virtual ~BackendConfigurationData()
    {};
};


struct TestWithBackendMainHelper : public youtils::TestMainHelper
{
    TestWithBackendMainHelper(int argc,
                              char** argv);

    void
    setupTestBackend();


    void
    tearDownTestBackend();

    virtual void
    log_backend_setup_help(std::ostream& strm)
    {
        strm << backend_setup_options_;
    }

    virtual void
    parse_command_line_arguments() override
    {
        init_google_test();
        parse_unparsed_options(backend_setup_options_,
                               yt::AllowUnregisteredOptions::T,
                               vm_);

    }

    static const std::unique_ptr<BackendConfig>&
    backendConfig()
    {
        return backend_config_;
    }



    virtual int
    run() override
    {
        int res = 0;
        setupTestBackend();
        res = TestMainHelper::run();
        tearDownTestBackend();
        return res;
    }

protected:
    virtual void
    check_and_setup_local();

    virtual void
    check_and_setup_S3();

    virtual void
    check_and_setup_MULTI();

    virtual void
    check_and_setup_ALBA();

    virtual void
    teardown_local();

    virtual void
    teardown_S3();

    virtual void
    teardown_MULTI();

    virtual void
    teardown_ALBA();

    static std::unique_ptr<BackendConfig> backend_config_;

    std::unique_ptr<BackendConfigurationData> backend_config_data_;
    boost::program_options::options_description backend_setup_options_;
    bool skip_backend_setup_;


};
}

// Local Variables: **
// mode: c++ **
// End: **
