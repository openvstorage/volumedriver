// Copyright 2015 iNuron NV
//
// licensed under the Apache license, Version 2.0 (the "license");
// you may not use this file except in compliance with the license.
// You may obtain a copy of the license at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the license is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the license for the specific language governing permissions and
// limitations under the license.

#include "LockedPythonClient.h"
#include "XMLRPCKeys.h"
#include "XMLRPCStructs.h"

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <youtils/Catchers.h>
#include <youtils/ScopeExit.h>

#include <backend/BackendConnectionManager.h>

#include <volumedriver/LockStoreFactory.h>

namespace volumedriverfs
{

namespace be = backend;
namespace bpt = boost::property_tree;
namespace bpy = boost::python;
namespace vd = volumedriver;
namespace yt = youtils;

#define LOCK_CHILD()                                      \
    boost::lock_guard<decltype(child_lock_)> clg__(child_lock_)

LockedPythonClient::LockedPythonClient(const std::string& cluster_id,
                                       const std::vector<ClusterContact>& cluster_contacts,
                                       const std::string& volume_id,
                                       const yt::UpdateInterval& update_interval,
                                       const yt::GracePeriod& grace_period)
    : PythonClient(cluster_id,
                   cluster_contacts)
    , volume_id_(volume_id)
    , grace_period_(grace_period)
    , update_interval_(update_interval)
{}

LockedPythonClient::~LockedPythonClient()
{
    // exit() is responsible for cleanup and should've been called
    try
    {
        VERIFY(not locked_section_);
        VERIFY(not promise_);
        VERIFY(not thread_.joinable());
        VERIFY(not child_);
    }
    CATCH_STD_ALL_LOG_IGNORE(volume_id_ << ": exception in destructor");
}

LockedPythonClient::Ptr
LockedPythonClient::create(const std::string& cluster_id,
                           const std::vector<ClusterContact>& cluster_contacts,
                           const std::string& volume_id,
                           const unsigned update_interval_secs,
                           const unsigned grace_period_secs)
{
    return Ptr(new LockedPythonClient(cluster_id,
                                      cluster_contacts,
                                      volume_id,
                                      yt::UpdateInterval(boost::posix_time::seconds(update_interval_secs)),
                                      yt::GracePeriod(boost::posix_time::seconds(grace_period_secs))));
}

LockedPythonClient::Ptr
LockedPythonClient::enter()
try
{
    LOG_INFO(volume_id_ << ": entered");
    VERIFY(not promise_);

    const XMLRPCVolumeInfo vinfo(info_volume(volume_id_));

    XmlRpc::XmlRpcValue req;
    req[XMLRPCKeys::show_defaults] = "true"; // just in case defaults change.
    req[XMLRPCKeys::vrouter_id] = vinfo.vrouter_id;

    const std::string s(call(PersistConfigurationToString::method_name(),
                             req)[XMLRPCKeys::configuration]);
    std::stringstream ss;
    ss << s;

    bpt::ptree pt;
    bpt::json_parser::read_json(ss,
                                pt);

    be::BackendConnectionManagerPtr
        cm(be::BackendConnectionManager::create(pt,
                                                RegisterComponent::F));
    vd::LockStoreFactory lsf(pt,
                             RegisterComponent::F,
                             cm);

    start_promise_ = Promise();
    start_future_ = start_promise_.get_future();

    promise_ = Promise();

    future_ = promise_->get_future();

    locked_section_ =
        std::make_unique<LockedSection>(boost::ref(*this),
                                        LockedSection::retry_connection_times_default(),
                                        LockedSection::connection_retry_timeout_default(),
                                        lsf.build_one(be::Namespace(vinfo._namespace_)),
                                        update_interval_);

    thread_ = boost::thread([&]
                            {
                                (*locked_section_)();
                            });

    start_future_.wait();
    return shared_from_this();
}
CATCH_STD_ALL_EWHAT({
        LOG_ERROR(volume_id_ << ": caught exception: " << EWHAT);
        locked_section_ = nullptr;
        promise_ = boost::none;
        throw;
    });

void
LockedPythonClient::exit(boost::python::object& /* exc_type */,
                         boost::python::object& /* exc_value */,
                         boost::python::object& /* traceback */)
{
    LOG_INFO(volume_id_ << ": exiting");

    VERIFY(promise_);
    VERIFY(not child_);

    promise_->set_value();
    thread_.join();
    locked_section_ = nullptr;
    promise_ = boost::none;
}

void
LockedPythonClient::operator()()
{
    try
    {
        start_promise_.set_value();
        future_.wait();
    }
    CATCH_STD_ALL_LOG_IGNORE(volume_id_ << ": got an exception");

    LOCK_CHILD();

    if (child_)
    {
        LOG_ERROR(volume_id_ << ": child " << child_->rdbuf()->pid() <<
                  " is still alive, sending SIGKILL");
        child_->rdbuf()->kill(SIGKILL);
        child_ = nullptr;
    }
}

std::string
LockedPythonClient::info()
{
    return volume_id_ + ": LockedPythonClient";
}

bpy::list
LockedPythonClient::get_scrubbing_work()
{
    LOG_INFO(volume_id_ << ": getting scrub work");

    THROW_UNLESS(promise_);
    THROW_UNLESS(locked_section_);

    return PythonClient::get_scrubbing_work(volume_id_);
}

void
LockedPythonClient::apply_scrubbing_result(const std::string& scrub_res)
{
    LOG_INFO(volume_id_ << ": applying scrub result");

    THROW_UNLESS(promise_);
    THROW_UNLESS(locked_section_);

    return PythonClient::apply_scrubbing_result(volume_id_,
                                                scrub_res);
}

std::string
LockedPythonClient::scrub(const std::string& scrub_work,
                          const std::string& scratch_dir,
                          const uint64_t region_size_exponent,
                          const float fill_ratio,
                          const bool verbose,
                          const std::string& scrubber_name)
{
    LOG_INFO(volume_id_ << ": scrubbing " << scrub_work);

    THROW_UNLESS(promise_);
    THROW_UNLESS(locked_section_);

    std::vector<std::string> args;
    args.reserve(11);

    args.emplace_back(scrubber_name);
    args.emplace_back("--scrub-work");
    args.emplace_back(scrub_work);
    args.emplace_back("--scratch-dir");
    args.emplace_back(scratch_dir);
    args.emplace_back("--region-size-exponent");
    args.emplace_back(boost::lexical_cast<std::string>(region_size_exponent));
    args.emplace_back("--fill-ratio");
    args.emplace_back(boost::lexical_cast<std::string>(fill_ratio));
    args.emplace_back("--verbose");
    args.emplace_back(boost::lexical_cast<std::string>(verbose));

    LOG_INFO(volume_id_ << ": launching scrubber, args: ");
    for (const auto& a : args)
    {
        LOG_INFO("\t" << a);
    }

    ChildOutputStreamPtr child;

    {
        LOCK_CHILD();
        child_ = boost::make_shared<redi::ipstream>(args);
        child = child_;
    }

    auto on_exit(yt::make_scope_exit([&]
                                     {
                                         LOCK_CHILD();
                                         child_ = nullptr;
                                     }));

    std::stringstream ss;

    std::string line;
    while (std::getline(*child,
                        line))
    {
        ss << line << std::endl;
    }

    LOG_INFO(volume_id_ << ": scrubber said: " << ss.str());

    child->rdbuf()->close();
    const int status = child->rdbuf()->status();

    if (status)
    {
        LOG_ERROR(volume_id_ << ": scrubber's exit status: " << status);
        throw std::runtime_error("scrubber returned with nonzero exit status " + boost::lexical_cast<std::string>(status));
    }

    return ss.str();
}

}
