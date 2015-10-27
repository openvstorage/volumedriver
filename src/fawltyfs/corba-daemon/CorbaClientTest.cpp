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

#include <youtils/TestBase.h>
#include <fawlty.hh>
#include <youtils/Logging.h>
#include <youtils/Assert.h>
#include "Literals.h"
#include <youtils/FileUtils.h>
#include "../ErrorRules.h"

namespace fs = boost::filesystem;
using youtils::FileUtils;

class CorbaClientTest : public youtilstest::TestBase
{
public:
    CorbaClientTest()
    {};

    void SetUp()
    {
        if(fs::exists(dir_))
        {
            FileUtils::removeAllNoThrow(dir_);
        }

        int argc = 1;

        const char* argv[] = {
            "CorbaClientTest",
            0
        };

        orb_ = CORBA::ORB_init(argc, (char**)argv);
        VERIFY(not CORBA::is_nil(orb_));
        CORBA::Object_var obj = getObjectReference(orb_);
        VERIFY(not CORBA::is_nil(obj));
        fawlty_ref_ = Fawlty::FileSystemFactory::_narrow(obj);
        VERIFY(not CORBA::is_nil(fawlty_ref_));
        FileUtils::checkDirectoryEmptyOrNonExistant(dir_);


    }
    DECLARE_LOGGER("CorbaClientTest");


    void
    TearDown()
    {
        FileUtils::removeAllNoThrow(dir_);
        orb_->destroy();
    }

    CORBA::Object_ptr
    getObjectReference(CORBA::ORB_ptr orb)
    {
        CosNaming::NamingContext_var rootContext;

        try
        {
            // Obtain a reference to the root context of the Name service:
            CORBA::Object_var obj;
            obj = orb->resolve_initial_references("NameService");

            // Narrow the reference returned.
            rootContext = CosNaming::NamingContext::_narrow(obj);
            if( CORBA::is_nil(rootContext) )
            {
                LOG_FATAL("Failed to narrow the root naming context.");

                return CORBA::Object::_nil();
            }
        }
        catch (CORBA::NO_RESOURCES&)
        {
            LOG_FATAL("Caught NO_RESOURCES exception. You must configure omniORB with the location of the naming service.");
            return CORBA::Object::_nil();

        }
        catch(CORBA::ORB::InvalidName& ex)
        {
            // This should not happen!
            LOG_FATAL("Service required is invalid [does not exist].");
            return CORBA::Object::_nil();
        }

        // Create a name object, containing the name test/context:
        CosNaming::Name name;
        name.length(2);

        name[0].id   = (const char*) TEST_NAME;
        name[0].kind = (const char*) TEST_KIND;
        name[1].id   = (const char*) OBJECT_NAME;
        name[1].kind = (const char*) OBJECT_KIND;
        // Note on kind: The kind field is used to indicate the type
        // of the object. This is to avoid conventions such as that used
        // by files (name.type -- e.g. test.ps = postscript etc.)

        try
        {
            // Resolve the name to an object reference.
            return rootContext->resolve(name);
        }
        catch(CosNaming::NamingContext::NotFound& ex)
        {
            // This exception is thrown if any of the components of the
            // path [contexts or the object] aren't found:
            LOG_INFO("Context not found.");
        }
        catch(CORBA::TRANSIENT& ex)
        {
            LOG_FATAL( "Caught system exception TRANSIENT -- unable to contact the naming service.");
            LOG_FATAL("Make sure the naming server is running and that omniORB is configured correctly.");
        }
        catch(CORBA::SystemException& ex)
        {
            LOG_FATAL("Caught a CORBA::" << ex._name()  << " while using the naming service.");
        }
        return CORBA::Object::_nil();
    }

    void
    create_fs(const std::string& fs_dir,
             Fawlty::FileSystem_var& fs)
    {

        fs::path backend_path = dir_ / fs::path(fs_dir + std::string("_backend"));
        fs::create_directories(backend_path);

        CORBA::String_var backend_string = backend_path.string().c_str();

        fs::path frontend_path = dir_ / fs::path(fs_dir + std::string("_frontend"));
        fs::create_directories(frontend_path);

        CORBA::String_var frontend_string = frontend_path.string().c_str();

        CORBA::String_var filesystem_name = "corba_fuse_and_what_have_you";

        VERIFY(not CORBA::is_nil(fawlty_ref_));
        CORBA::Object_var obj = fawlty_ref_->create(backend_string,
                                                    frontend_string,
                                                    filesystem_name);
        VERIFY(not CORBA::is_nil(obj));
        fs = Fawlty::FileSystem::_narrow(obj);
        VERIFY(not CORBA::is_nil(fs));
    }

    void
    release_fs(Fawlty::FileSystem_var& fs)
    {
        fawlty_ref_->release(fs);
    }

    void
    add_delay_rule(Fawlty::FileSystem_ptr fs,
                   int rule_id,
                   const std::string& path_regex,
                   const std::set<fawltyfs::FileSystemCall>& calls,
                   int ramp_up,
                   int duration,
                   int delay_usecs)

    {
        CORBA::String_var path_regex_corba = path_regex.c_str();
        fs->addDelayRule(rule_id,
                         path_regex_corba,
                         toCorba(calls),
                         ramp_up,
                         duration,
                         delay_usecs);
    }

    fawltyfs::delay_rules_list_t
    get_delay_rules(Fawlty::FileSystem_ptr fs)
    {
        fawltyfs::delay_rules_list_t d_r;

        Fawlty::DelayRules_var rules = fs->listDelayRules();
        for(size_t i = 0; i < rules->length(); ++i)
        {
            Fawlty::DelayRule& rule = rules[i];
            d_r.push_back(std::make_shared<fawltyfs::DelayRule>(rule.id,
                                                                std::string(rule.path_regex),
                                                                fromCorba(rule.calls),
                                                                rule.ramp_up,
                                                                rule.duration,
                                                                rule.delay_usecs));
        }

        return d_r;

    }

    fawltyfs::failure_rules_list_t
    get_failure_rules(Fawlty::FileSystem_ptr fs)
    {
        fawltyfs::failure_rules_list_t f_r;

        Fawlty::FailureRules_var rules = fs->listFailureRules();
        for(size_t i = 0; i < rules->length(); ++i)
        {
            Fawlty::FailureRule& rule = rules[i];
            f_r.push_back(std::make_shared<fawltyfs::FailureRule>(rule.id,
                                                                  std::string(rule.path_regex),
                                                                  fromCorba(rule.calls),
                                                                  rule.ramp_up,
                                                                  rule.duration,
                                                                  rule.errcode));
        }

        return f_r;
    }

protected:
    CORBA::ORB_var orb_;

    Fawlty::FileSystemFactory_var fawlty_ref_;
    fs::path dir_ = getTempPath("CorbaClientTest");

};

TEST_F(CorbaClientTest, hello)
{
    CORBA::String_var src =  "Hello!";

    CORBA::String_var dest = fawlty_ref_->ping(src);

    EXPECT_EQ(std::string(src), std::string(dest));
}


TEST_F(CorbaClientTest, CreateFileSystem)
{
    Fawlty::FileSystem_var fs;
    create_fs("CreateFileSystem",
              fs);

    VERIFY(not CORBA::is_nil(fs));
    fs->mount();

    fs->umount();
    release_fs(fs);
}

TEST_F(CorbaClientTest, AddDelayRules)
{
    Fawlty::FileSystem_var fs;
    create_fs("CreateFileSystem",
              fs);
    fawltyfs::delay_rules_list_t delay_rules = get_delay_rules(fs);
    EXPECT_EQ(delay_rules.size(),0);

    std::set<fawltyfs::FileSystemCall> calls  {fawltyfs::FileSystemCall::Read, fawltyfs::FileSystemCall::Write};

    std::string path_regex(".*");

    add_delay_rule(fs,
                   100,
                   path_regex,
                   calls,
                   101,
                   102,
                   103);

    delay_rules = get_delay_rules(fs);
    EXPECT_EQ(delay_rules.size(), 1);
    fawltyfs::DelayRulePtr& rule = delay_rules.front();

    EXPECT_EQ(rule->id, 100);
    EXPECT_EQ(rule->path_regex.str(), path_regex);
    EXPECT_TRUE(rule->calls_ == calls);
    EXPECT_EQ(rule->ramp_up, 101);

    EXPECT_EQ(rule->duration, 102);
    EXPECT_EQ(rule->delay_usecs, 103);

}

TEST_F(CorbaClientTest, shutdown)
{
    fawlty_ref_->shutdown(true);
    sleep(1);
    //    VERIFY(CORBA::is_nil(fawlty_ref_));
}
