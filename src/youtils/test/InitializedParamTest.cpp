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

#include "../InitializedParam.h"
#include "../StreamUtils.h"
#include "../TestBase.h"

#include <vector>

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

namespace bpt = boost::property_tree;
namespace ip = initialized_params;
namespace yt = youtils;

using namespace std::literals::string_literals;

namespace
{

struct Foo
{
    Foo(int k,
        int v)
        : key(k)
        , val(v)
    {};

    bool
    operator==(const Foo& other) const
    {
        return key == other.key and
            val == other.val;
    }

    int key;
    int val;
};

using FooVector = std::vector<Foo>;

std::ostream&
operator<<(std::ostream& os,
           const Foo& foo)
{
    os << "(" << foo.key << ", " << foo.val << ")";
    return os;
}

std::ostream&
operator<<(std::ostream& os,
           const FooVector& foos)
{
    return yt::StreamUtils::stream_out_sequence(os,
                                                foos);
}

const FooVector default_foo_vector({ Foo(1, 2), Foo(2, 3) });
const int default_for_atomic_int(42);
const yt::DimensionedValue default_dimensioned_value("999PiB");
const std::string default_string("default");

}

namespace initialized_params
{

const char component_name[] = "initialized_param_test";

DECLARE_INITIALIZED_PARAM(non_defaulted_non_resettable_string,
                          std::string);

DEFINE_INITIALIZED_PARAM(non_defaulted_non_resettable_string,
                         component_name,
                         "non_defaulted_non_resettable_string",
                         "string, not defaulted, not resettable",
                         ShowDocumentation::T);

DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(defaulted_non_resettable_string,
                                       std::string);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(defaulted_non_resettable_string,
                                      component_name,
                                      "defaulted_non_resettable_string",
                                      "string, defaulted, not resettable",
                                      ShowDocumentation::T,
                                      default_string);

DECLARE_RESETTABLE_INITIALIZED_PARAM(non_defaulted_resettable_string,
                                     std::string);

DEFINE_INITIALIZED_PARAM(non_defaulted_resettable_string,
                         component_name,
                         "non_defaulted_resettable_string",
                         "string, not defaulted, resettable",
                         ShowDocumentation::T);

DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(defaulted_resettable_atomic_int,
                                                  std::atomic<int>);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(defaulted_resettable_atomic_int,
                                      component_name,
                                      "defaulted_resettable_atomic_int",
                                      "atomic int, defaulted, resettable",
                                      ShowDocumentation::T,
                                      default_for_atomic_int);

DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(defaulted_resettable_dimensioned_value,
                                                  yt::DimensionedValue);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(defaulted_resettable_dimensioned_value,
                                      component_name,
                                      "defaulted_resettable_int",
                                      "dimensioned value, defaulted, resettable",
                                      ShowDocumentation::T,
                                      default_dimensioned_value);

template<>
struct PropertyTreeVectorAccessor<Foo>
{
    static Foo
    get(const bpt::ptree& pt)
    {
        int k = pt.get<int>("key");
        int v = pt.get<int>("val");

        return Foo(k, v);
    }

    static void
    put(bpt::ptree& pt,
        const Foo& foo)
    {
        pt.put<int>("key", foo.key);
        pt.put<int>("val", foo.val);
    }
};

DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(defaulted_resettable_foo_vector,
                                                  FooVector);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(defaulted_resettable_foo_vector,
                                      component_name,
                                      "defaulted_resettable_foo_vector",
                                      "FooVector, defaulted, resettable",
                                      ShowDocumentation::T,
                                      default_foo_vector);
}

namespace youtilstest
{

class InitializedParamTest
    : public TestBase
{
protected:
    template<typename T>
    void
    test_default()
    {
        bpt::ptree pt;

        if (T::has_default_value())
        {
            T t(pt);

            ASSERT_TRUE(t.was_defaulted());
            ASSERT_NE(boost::none,
                      t.default_value_);
            ASSERT_EQ(*t.default_value_,
                      t.value());

            t.persist(pt,
                      ReportDefault::F);

            ASSERT_TRUE(pt.empty());

            t.persist(pt,
                      ReportDefault::T);

            ASSERT_FALSE(pt.empty());

            T t2(pt);

            ASSERT_FALSE(t2.was_defaulted());
            ASSERT_EQ(t.value(),
                      t2.value());
        }
        else
        {
            ASSERT_THROW(T t(pt),
                         std::exception);
        }
    }

    template<typename T>
    void
    test_no_update(const typename T::ValueType& val)
    {
        bpt::ptree pt;
        T::PTreeAccessor::put(pt,
                              T::path(),
                              val);

        T t(pt);

        ASSERT_FALSE(t.was_defaulted());

        yt::UpdateReport urep;

        t.update(pt,
                 urep);

        ASSERT_FALSE(t.was_defaulted());

        ASSERT_EQ(0U,
                  urep.update_size());
        ASSERT_EQ(1U,
                  urep.no_update_size());
    }

    template<typename T>
    void
    test_update(const typename T::ValueType& oldval,
                const typename T::ValueType& newval)
    {
        ASSERT_NE(oldval,
                  newval) << "Fix yer test!";

        bpt::ptree pt;
        T::PTreeAccessor::put(pt,
                              T::path(),
                              oldval);

        T t(pt);

        ASSERT_FALSE(t.was_defaulted());
        ASSERT_EQ(oldval,
                  t.value());

        T::PTreeAccessor::put(pt,
                              T::path(),
                              newval);

        yt::UpdateReport urep;

        t.update(pt,
                 urep);

        ASSERT_FALSE(t.was_defaulted());

        if (T::resettable())
        {
            ASSERT_EQ(1U,
                      urep.update_size());
            ASSERT_EQ(0U,
                      urep.no_update_size());
            ASSERT_EQ(newval,
                      t.value());
        }
        else
        {
            ASSERT_EQ(0U,
                      urep.update_size());
            ASSERT_EQ(1U,
                      urep.no_update_size());
            ASSERT_EQ(oldval,
                      t.value());
        }
    }

    template<typename T>
    void
    test_reset_to_default(const typename T::ValueType& val)
    {
        if (T::has_default_value())
        {
            ASSERT_NE(boost::none,
                      T::default_value_);
            ASSERT_NE(*T::default_value_,
                      val);
        }
        else
        {
            ASSERT_EQ(boost::none,
                      T::default_value_);
        }

        bpt::ptree pt;
        T::PTreeAccessor::put(pt,
                              T::path(),
                              val);

        T t(pt);

        ASSERT_FALSE(t.was_defaulted());
        ASSERT_EQ(val,
                  t.value());

        bpt::ptree pt2;

        yt::UpdateReport urep;

        t.update(pt2,
                 urep);

        if (T::resettable() and T::has_default_value())
        {
            ASSERT_TRUE(t.was_defaulted());

            ASSERT_EQ(1U,
                      urep.update_size());
            ASSERT_EQ(0U,
                      urep.no_update_size());

            ASSERT_EQ(*T::default_value_,
                      t.value());
        }
        else
        {
            ASSERT_FALSE(t.was_defaulted());

            ASSERT_EQ(0U,
                      urep.update_size());
            ASSERT_EQ(1U,
                      urep.no_update_size());
            ASSERT_EQ(val,
                      t.value());
        }
    }

    template<typename T>
    void
    test_persist(const typename T::ValueType& val)
    {
        if (T::has_default_value())
        {
            ASSERT_NE(boost::none,
                      T::default_value_);
            ASSERT_NE(*T::default_value_,
                      val);
        }
        else
        {
            ASSERT_EQ(boost::none,
                      T::default_value_);
        }

        bpt::ptree pt;
        T::PTreeAccessor::put(pt,
                              T::path(),
                              val);

        T t(pt);

        ASSERT_FALSE(t.was_defaulted());
        ASSERT_EQ(val,
                  t.value());

        bpt::ptree pt2;

        t.persist(pt2);

        T t2(pt2);

        ASSERT_FALSE(t2.was_defaulted());
        ASSERT_EQ(val,
                  t2.value());
    }

    template<typename T,
             typename U>
    void
    test_invalid_input(const U& some_other_value)
    {
        static_assert(not std::is_convertible<U, typename T::ValueType>::value,
                      "fix your test");

        bpt::ptree pt;

        ip::PropertyTreeAccessor<U>::put(pt,
                                         T::path(),
                                         some_other_value);

        ASSERT_THROW(T t(pt),
                     std::exception) << T::name();
    }

    template<typename T,
             typename U>
    void
    test_invalid_update(const typename T::ValueType& val,
                        const U& some_other_value)
    {
        static_assert(not std::is_convertible<U, typename T::ValueType>::value,
                      "fix your test");

        bpt::ptree pt;
        T::PTreeAccessor::put(pt,
                              T::path(),
                              val);

        T t(pt);

        ip::PropertyTreeAccessor<U>::put(pt,
                                         T::path(),
                                         some_other_value);

        yt::UpdateReport urep;

        t.update(pt,
                 urep);

        ASSERT_EQ(0U,
                  urep.update_size()) << T::name();
        ASSERT_EQ(1U,
                  urep.no_update_size()) << T::name();
        ASSERT_EQ(val,
                  t.value()) << T::name();
    }

    template<typename T>
    void
    test_copy_construction(const typename T::ValueType& val)
    {
        const T t(val);
        T u(t);

        ASSERT_EQ(t.was_defaulted(),
                  u.was_defaulted());

        ASSERT_EQ(t.value(),
                  u.value());
    }
};

TEST_F(InitializedParamTest, default_values)
{
    test_default<ip::PARAMETER_TYPE(non_defaulted_non_resettable_string)>();
    test_default<ip::PARAMETER_TYPE(defaulted_non_resettable_string)>();
    test_default<ip::PARAMETER_TYPE(non_defaulted_resettable_string)>();
    test_default<ip::PARAMETER_TYPE(defaulted_resettable_atomic_int)>();
    test_default<ip::PARAMETER_TYPE(defaulted_resettable_dimensioned_value)>();
    test_default<ip::PARAMETER_TYPE(defaulted_resettable_foo_vector)>();
}

TEST_F(InitializedParamTest, copy_construction)
{
    test_copy_construction<ip::PARAMETER_TYPE(non_defaulted_non_resettable_string)>("blah"s);
    test_copy_construction<ip::PARAMETER_TYPE(defaulted_non_resettable_string)>("blah"s);
    test_copy_construction<ip::PARAMETER_TYPE(non_defaulted_resettable_string)>("blah"s);
    test_copy_construction<ip::PARAMETER_TYPE(defaulted_resettable_atomic_int)>(123);
    test_copy_construction<ip::PARAMETER_TYPE(defaulted_resettable_dimensioned_value)>(yt::DimensionedValue("1B"));
    test_copy_construction<ip::PARAMETER_TYPE(defaulted_resettable_foo_vector)>({ Foo(0, 1) });
}

TEST_F(InitializedParamTest, no_updates)
{
    test_no_update<ip::PARAMETER_TYPE(non_defaulted_non_resettable_string)>("blah"s);
    test_no_update<ip::PARAMETER_TYPE(defaulted_non_resettable_string)>("blah"s);
    test_no_update<ip::PARAMETER_TYPE(non_defaulted_resettable_string)>("blah"s);
    test_no_update<ip::PARAMETER_TYPE(defaulted_resettable_atomic_int)>(123);
    test_no_update<ip::PARAMETER_TYPE(defaulted_resettable_dimensioned_value)>(yt::DimensionedValue("1B"));
    test_no_update<ip::PARAMETER_TYPE(defaulted_resettable_foo_vector)>({ Foo(0, 1) });
}

TEST_F(InitializedParamTest, updates)
{
    test_update<ip::PARAMETER_TYPE(non_defaulted_non_resettable_string)>("old"s,
                                                                         "new"s);
    test_update<ip::PARAMETER_TYPE(defaulted_non_resettable_string)>("old"s,
                                                                     "new"s);
    test_update<ip::PARAMETER_TYPE(non_defaulted_resettable_string)>("old"s,
                                                                     "new"s);
    test_update<ip::PARAMETER_TYPE(defaulted_resettable_atomic_int)>(1,
                                                                     2);
    test_update<ip::PARAMETER_TYPE(defaulted_resettable_dimensioned_value)>(yt::DimensionedValue("1B"),
                                                                            yt::DimensionedValue("999PiB"));
    test_update<ip::PARAMETER_TYPE(defaulted_resettable_foo_vector)>({ Foo(0, 1) },
        { Foo(1, 2), Foo(2, 3)} );
}

TEST_F(InitializedParamTest, reset_to_default)
{
    test_reset_to_default<ip::PARAMETER_TYPE(non_defaulted_non_resettable_string)>("blah"s);
    test_reset_to_default<ip::PARAMETER_TYPE(defaulted_non_resettable_string)>("blah"s);
    test_reset_to_default<ip::PARAMETER_TYPE(non_defaulted_resettable_string)>("blah"s);
    test_reset_to_default<ip::PARAMETER_TYPE(defaulted_resettable_atomic_int)>(123);
    test_reset_to_default<ip::PARAMETER_TYPE(defaulted_resettable_dimensioned_value)>(yt::DimensionedValue("1B"));
    test_reset_to_default<ip::PARAMETER_TYPE(defaulted_resettable_foo_vector)>({ Foo(0, 1) });
}

TEST_F(InitializedParamTest, persist)
{
    test_persist<ip::PARAMETER_TYPE(non_defaulted_non_resettable_string)>("blah"s);
    test_persist<ip::PARAMETER_TYPE(defaulted_non_resettable_string)>("blah"s);
    test_persist<ip::PARAMETER_TYPE(non_defaulted_resettable_string)>("blah"s);
    test_persist<ip::PARAMETER_TYPE(defaulted_resettable_atomic_int)>(123);
    test_persist<ip::PARAMETER_TYPE(defaulted_resettable_dimensioned_value)>(yt::DimensionedValue("1B"));
    test_persist<ip::PARAMETER_TYPE(defaulted_resettable_foo_vector)>({ Foo(0, 1) });
}

TEST_F(InitializedParamTest, invalid_input)
{
    test_invalid_input<ip::PARAMETER_TYPE(defaulted_resettable_atomic_int)>("Does this look like an int to you?");
    test_invalid_input<ip::PARAMETER_TYPE(defaulted_resettable_dimensioned_value)>("Nope, this ain't a DimensionedValue");
    test_invalid_input<ip::PARAMETER_TYPE(defaulted_resettable_foo_vector)>("I'm afraid this isn't a FooVector");
}

TEST_F(InitializedParamTest, invalid_update)
{
    test_invalid_update<ip::PARAMETER_TYPE(defaulted_resettable_atomic_int)>(123,
                                                                             "Still no int, eh?");
    test_invalid_update<ip::PARAMETER_TYPE(defaulted_resettable_dimensioned_value)>(yt::DimensionedValue("1B"),
                                                                                    "DimensionedValues look differently");
    test_invalid_update<ip::PARAMETER_TYPE(defaulted_resettable_foo_vector)>({ Foo(0, 1) },
                                                                             "This ain't a FooVector either");
}

TEST_F(InitializedParamTest, empty_vector)
{
    using Param = ip::PARAMETER_TYPE(defaulted_resettable_foo_vector);

    FooVector val;
    ip::PropertyTreeAccessor<FooVector> accessor;
    bpt::ptree pt;

    accessor.put(pt,
                 Param::path(),
                 val);

    Param p(pt);
    ASSERT_TRUE(p.value().empty());
    ASSERT_FALSE(p.was_defaulted());
}

}
