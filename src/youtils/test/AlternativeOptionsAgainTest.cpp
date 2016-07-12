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

#include <gtest/gtest.h>
#include "../AlternativeOptionAgain.h"

#include <boost/program_options.hpp>
#include <loki/TypelistMacros.h>
#include "../Assert.h"

namespace youtilstest
{
class AlternativeOptionsAgainTest
    : public testing::Test
{};

enum class OptsTest
{
    Foo,
    Bar
};
}

namespace youtils
{

template<> struct
AlternativeOptionTraits<youtilstest::OptsTest>
{
    static std::map<youtilstest::OptsTest,
                    const char*> str_;

};

using opt_traits_t = struct
    AlternativeOptionTraits<youtilstest::OptsTest> ;


std::map<youtilstest::OptsTest, const char*>
opt_traits_t::str_ = {
    {
        youtilstest::OptsTest::Foo, "foo"
    },
    {
        youtilstest::OptsTest::Bar, "bar"
    }
};

}


namespace youtilstest
{
namespace yt = youtils;
namespace po = boost::program_options;



struct FooOpts
    : public yt::AlternativeOptionAgain<OptsTest,
                                        OptsTest::Foo>
{
    explicit FooOpts()
    {
         desc_.add_options()
             ("some-option",
              po::value<std::string>(&some_option),
              "foo this");
    }

    void
    actions() override
    {}

    const boost::program_options::options_description&
     options_description() const override
     {
         return desc_;
     }

     std::string some_option;
     boost::program_options::options_description desc_;

};

struct BarOpts
    : public yt::AlternativeOptionAgain<OptsTest,
                                        OptsTest::Bar>
{
    explicit BarOpts()
    {
        desc_.add_options()
            ("some-other-option",
             po::value<std::string>(&some_option),
             "bar that");
    }

    void
    actions() override
    {}

    const boost::program_options::options_description&
    options_description() const override
    {
        return desc_;

    }

    boost::program_options::options_description desc_;
    std::string some_option;
};

TEST_F(AlternativeOptionsAgainTest, test)
{
    std::vector<std::string> args;
    args.emplace_back("--some-option");
    args.emplace_back("some-value");

    po::variables_map vm;
    po::command_line_parser parser = po::command_line_parser(args);
    parser.style(po::command_line_style::default_style ^
                 po::command_line_style::allow_guessing);
    typedef LOKI_TYPELIST_2(BarOpts,
                            FooOpts)
        options_t;



    auto opt = ::youtils::get_option<OptsTest,
                                     options_t>("foo");


    // yt::AlternativeOptionsAgain::mapped_type opt(ao.at("foo"));
    parser.options(opt->options_description());
    po::parsed_options parsed = parser.run();
    po::store(parsed, vm);
    po::notify(vm);

    EXPECT_EQ(1UL, vm.count("some-option"));
    EXPECT_EQ("some-value", vm["some-option"].as<std::string>());
    EXPECT_EQ(0UL, vm.count("some-other-option"));

    EXPECT_THROW((opt = ::youtils::get_option<OptsTest,
                                              options_t>("foobar")),
                 std::out_of_range);
    // EXPECT_TRUE(ao.find("baz") == nullptr);
};

}

// Local Variables: **
// mode: c++ **
// End: **
