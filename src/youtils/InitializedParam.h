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

#ifndef INITIALIZED_PARAM_H
#define INITIALIZED_PARAM_H

#include "Assert.h"
#include "BooleanEnum.h"
#include "Catchers.h"
#include "DimensionedValue.h"
#include "IOException.h"
#include "Logging.h"
#include "UpdateReport.h"

#include <atomic>
#include <exception>
#include <iosfwd>
#include <list>

#include <boost/lexical_cast.hpp>
#include <boost/optional.hpp>
#include <boost/property_tree/ptree.hpp>

VD_BOOLEAN_ENUM(ShowDocumentation)
VD_BOOLEAN_ENUM(ReportDefault)
VD_BOOLEAN_ENUM(CanBeReset);
VD_BOOLEAN_ENUM(HasDefault);

namespace initialized_params
{

MAKE_EXCEPTION(MissingParameterException, fungi::IOException);
MAKE_EXCEPTION(MalformedParameterException, fungi::IOException);

template<typename T>
struct SectionName;

template<typename T>
struct PythonName;

template<typename T>
struct Resettable;

template<typename T>
struct HasDefaultValue;

template<typename T>
struct Value
{
    using type = T;
};

template<typename T>
struct Value<std::atomic<T>>
{
    using type = T;
};

template<typename T>
struct ValueAccessor
{
    static const T&
    get(const T& t)
    {
        return t;
    }
};

template<typename T>
struct ValueAccessor<std::atomic<T>>
{
    static T
    get(const std::atomic<T>& a)
    {
        return a.load();
    }
};

template<typename T>
struct PropertyTreeAccessor
{
    static boost::optional<T>
    get_optional(const boost::property_tree::ptree& pt,
                 const std::string& path)
    {
        return pt.get_optional<T>(path);
    }

    static void
    put(boost::property_tree::ptree& pt,
        const std::string& path,
        const T& t)
    {
        pt.put<T>(path, t);
    }
};

template<typename T>
struct PropertyTreeVectorAccessor;

template<typename T>
struct PropertyTreeAccessor<std::vector<T>>
{
    using Vector = typename std::vector<T>;
    using MaybeVector = boost::optional<Vector>;
    using PTreeVectorAccessor = PropertyTreeVectorAccessor<T>;

    DECLARE_LOGGER("PropertyTreeAccessor");

    static MaybeVector
    get_optional(const boost::property_tree::ptree& pt,
                 const std::string& path)
    {
        const auto maybe_child_pt(pt.get_child_optional(path));
        if (maybe_child_pt == boost::none)
        {
            return boost::none;
        }
        else
        {
            Vector v;

            for (const auto& c : *maybe_child_pt)
            {
                v.push_back(PTreeVectorAccessor::get(c.second));
            }

            if (v.empty())
            {
                const auto s(maybe_child_pt->get_value_optional<std::string>());
                if (s and not s->empty())
                {
                    LOG_ERROR("Expected a vector, found \"" << s << "\"");
                    throw MalformedParameterException("Expected a vector, found a value",
                                                      path.c_str());
                }
            }
            return v;
        }
    }

    static void
    put(boost::property_tree::ptree& pt,
        const std::string& path,
        const Vector& vec)
    {
        boost::property_tree::ptree child_pt;

        for (const auto& v : vec)
        {
            boost::property_tree::ptree p;
            PTreeVectorAccessor::put(p, v);
            child_pt.push_back(std::make_pair(std::string(), p));
        }

        pt.put_child(path,
                     child_pt);
    }
};

template<typename T,
         const char* param_name>
class InitializedParam
{
public:
    using ValueType = typename Value<T>::type;
    using MaybeType = boost::optional<ValueType>;

    using Accessor = ValueAccessor<T>;
    using PTreeAccessor = PropertyTreeAccessor<ValueType>;

    explicit InitializedParam(const boost::property_tree::ptree& pt)
    try
        : value_(get_(pt,
                      was_defaulted_))
    {
        VERIFY(not was_defaulted_ or has_default_value());
    }
    CATCH_STD_ALL_LOG_RETHROW("Failed to extract param " << param_name <<
                              " from " << path());

    explicit InitializedParam(const T& val)
        : value_(Accessor::get(val))
        , was_defaulted_(false)
    {}

    InitializedParam(const InitializedParam& other)
        : value_(Accessor::get(other.value_))
        , was_defaulted_(other.was_defaulted_)
    {}

    ~InitializedParam() = default;

    InitializedParam&
    operator=(const InitializedParam& other) = delete;

    void
    persist(boost::property_tree::ptree& pt,
            const ReportDefault report_default = ReportDefault::F) const
    {
        if (not was_defaulted_ or
            report_default == ReportDefault::T)
        {
            PTreeAccessor::put(pt,
                               path(),
                               value_);
        }
    }

    void
    update(const ValueType& val)
    {
        static_assert(Resettable<InitializedParam>::value,
                      "param is not resettable");

        value_ = val;
        was_defaulted_ = false;
    }

    void
    update(const ValueType& val,
           youtils::UpdateReport& report)
    {
        if (resettable())
        {
            report.update(param_name,
                          Accessor::get(value_),
                          val);
            was_defaulted_ = false;
            value_ = val;
        }
        else
        {
            report.no_update(param_name,
                             Accessor::get(value_),
                             val);
        }
    }

    void
    update(const boost::property_tree::ptree pt,
           youtils::UpdateReport& report)
    {
        try
        {
            const MaybeType new_value(PTreeAccessor::get_optional(pt, path()));
            if (new_value == boost::none)
            {
                // We end up here if the key is missing *and* if it's there but could not be
                // parsed:
                if (pt.get_child_optional(path()) != boost::none)
                {
                    LOG_ERROR("Malformed entry " << path() <<
                              ": found key but failed to parse value");
                    report.no_update(param_name);
                    return;
                }
            }

            if (resettable())
            {
                if (was_defaulted_)
                {
                    VERIFY(has_default_value());

                    if (new_value == boost::none)
                    {
                        // default -> default
                        report.no_update(param_name);
                    }
                    else
                    {
                        // default -> !default
                        report.update(param_name,
                                      Accessor::get(value_),
                                      *new_value);

                        value_ = *new_value;
                        was_defaulted_ = false;
                    }
                }
                else
                {
                    if (new_value == boost::none)
                    {
                        // !default -> default
                        if (has_default_value())
                        {
                            report.update(param_name,
                                          Accessor::get(value_),
                                          *default_value_);
                            value_ = *default_value_;
                            was_defaulted_ = true;
                        }
                        else
                        {
                            report.no_update(param_name);
                        }
                    }
                    else if (*new_value == value_)
                    {
                        report.no_update(param_name,
                                         Accessor::get(value_),
                                         *new_value);
                    }
                    else
                    {
                        // !default -> !default
                        report.update(param_name,
                                      Accessor::get(value_),
                                      *new_value);

                        value_ = *new_value;
                        was_defaulted_ = false;
                    }
                }
            }
            else
            {
                report.no_update(param_name);
            }
        }
        CATCH_STD_ALL_EWHAT({
                LOG_ERROR("Failed to update " << name() << ": " << EWHAT);
                report.no_update(param_name);
            });
    }

    const T&
    value() const
    {
        return value_;
    }

    static const char*
    section_name()
    {
        return SectionName<InitializedParam>::value;
    }

    static const char*
    name()
    {
        return param_name;
    }

    static const char*
    python_name()
    {
        return PythonName<InitializedParam>::value;
    }

    static std::string
    path()
    {
        return std::string(section_name()) + "." + std::string(name());
    }

    static bool
    resettable()
    {
        return Resettable<InitializedParam>::value;
    }

    static bool
    has_default_value()
    {
        return HasDefaultValue<InitializedParam>::value;
    }

    bool
    was_defaulted() const
    {
        return was_defaulted_;
    }

    static const MaybeType default_value_;

private:
    DECLARE_LOGGER("InitializedParam");

    T value_;
    bool was_defaulted_;

    static ValueType
    get_(const boost::property_tree::ptree& pt,
         bool& was_defaulted)
    {
        was_defaulted = false;
        const auto maybe_t(PTreeAccessor::get_optional(pt,
                                                       path()));
        if (maybe_t == boost::none)
        {
            // We end up here if the key is missing *and* if it's there but could not be
            // parsed:
            if (pt.get_child_optional(path()) != boost::none)
            {
                throw MalformedParameterException("found key but failed to parse value");
            }

            if (has_default_value())
            {
                VERIFY(default_value_ != boost::none);
                was_defaulted = true;
                return *default_value_;
            }
            else
            {
                throw MissingParameterException("parameter not present in ptree");
            }
        }
        else
        {
            return *maybe_t;
        }
    }
};

#define STRING_NAME(name) name##_string
#define PARAMETER_TYPE(name) name##_pt
#define PARAMETER_INFO_NAME(name) name##_info

#define DECLARE_INITIALIZED_PARAM_TRAITS(name, resettable, has_default) \
    template<>                                                          \
    struct SectionName<PARAMETER_TYPE(name)>                            \
    {                                                                   \
        static const char* value;                                       \
    };                                                                  \
                                                                        \
    template<>                                                          \
    struct PythonName<PARAMETER_TYPE(name)>                             \
    {                                                                   \
        static const char* value;                                       \
    };                                                                  \
                                                                        \
    template<>                                                          \
    struct Resettable<PARAMETER_TYPE(name)>                             \
    {                                                                   \
        static constexpr bool value = resettable == CanBeReset::T;      \
    };                                                                  \
                                                                        \
    template<>                                                          \
    struct HasDefaultValue<PARAMETER_TYPE(name)>                        \
    {                                                                   \
        static constexpr bool value = has_default == HasDefault::T;     \
    }

#define DECLARE_INITIALIZED_PARAM_(name, type, resettable, has_default) \
    extern const char STRING_NAME(name) [];                             \
    using PARAMETER_TYPE(name) = InitializedParam<type, STRING_NAME(name)>; \
    DECLARE_INITIALIZED_PARAM_TRAITS(name, resettable, has_default)

#define DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(name, type)   \
    DECLARE_INITIALIZED_PARAM_(name, type, CanBeReset::T, HasDefault::T)

#define DECLARE_RESETTABLE_INITIALIZED_PARAM(name, type)        \
    DECLARE_INITIALIZED_PARAM_(name, type, CanBeReset::T, HasDefault::F)

#define DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(name, type)   \
    DECLARE_INITIALIZED_PARAM_(name, type, CanBeReset::F, HasDefault::T)

#define DECLARE_INITIALIZED_PARAM(name, type)   \
    DECLARE_INITIALIZED_PARAM_(name, type, CanBeReset::F, HasDefault::F)

struct ParameterInfo
{
    template <typename T>
    ParameterInfo(const std::string doc,
                  const ShowDocumentation document,
                  const T* /* dummy */)
        : name(T::name())
        , has_default(T::has_default_value())
        , can_be_reset(T::resettable())
        , section_name(T::section_name())
        , python_name(T::python_name())
        , doc_string(doc)
        , def_(has_default ?
               boost::lexical_cast<std::string>(*T::default_value_) :
               boost::lexical_cast<std::string>(boost::none))
        , document_(document)
        , dimensioned_value(std::is_base_of<youtils::DimensionedValue,
                                            T>::value)
    {
        get_parameter_info_list().push_back(this);
    }

    static std::list<ParameterInfo*>&
    get_parameter_info_list();

    static void
    verify_property_tree(const boost::property_tree::ptree& pt);

    MAKE_EXCEPTION(UnknownParameterException, fungi::IOException);

    const std::string name;
    const bool has_default;
    const bool can_be_reset;
    const std::string section_name;
    const std::string python_name;
    const std::string doc_string;
    const std::string def_;
    const ShowDocumentation document_;
    const bool dimensioned_value;

private:
    DECLARE_LOGGER("ParameterInfo");
};

#define DEFINE_INITIALIZED_PARAM_DEFAULT(name, value)                   \
    template<>                                                          \
    const PARAMETER_TYPE(name)::MaybeType                               \
    PARAMETER_TYPE(name)::default_value_ = value

#define DEFINE_INITIALIZED_PARAM_TRAITS(name, s_name, p_name)           \
    const char* SectionName<PARAMETER_TYPE(name)>::value = s_name;      \
    const char* PythonName<PARAMETER_TYPE(name)>::value = p_name

#define DEFINE_INITIALIZED_PARAM_(name, s_name, p_name, doc, show_doc, def) \
    const char STRING_NAME(name)[] = #name;                             \
                                                                        \
    DEFINE_INITIALIZED_PARAM_DEFAULT(name, def);                        \
                                                                        \
    DEFINE_INITIALIZED_PARAM_TRAITS(name, s_name, p_name);              \
                                                                        \
    namespace                                                           \
    {                                                                   \
        ParameterInfo PARAMETER_INFO_NAME(name)(doc,                    \
                                                show_doc,               \
                                                static_cast<const PARAMETER_TYPE(name)*>(nullptr)); \
    }

#define DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(name, s_name, p_name, doc, show_doc, def) \
    static_assert(HasDefaultValue<PARAMETER_TYPE(name)>::value,         \
                  #name " does not support a default value");           \
    DEFINE_INITIALIZED_PARAM_(name, s_name, p_name, doc, show_doc, def)

#define DEFINE_INITIALIZED_PARAM(name, s_name, p_name, doc, show_doc)   \
    static_assert(not HasDefaultValue<PARAMETER_TYPE(name)>::value,     \
                  #name " expects a default value");                    \
    DEFINE_INITIALIZED_PARAM_(name, s_name, p_name, doc, show_doc, boost::none)
}

#define DECLARE_PARAMETER(name)                         \
    initialized_params::PARAMETER_TYPE(name) name

#define PARAMETER_VALUE_FROM_PROPERTY_TREE(name, property_tree) \
    initialized_params::PARAMETER_TYPE(name)(pt).value()

#endif

// Local Variables: **
// mode: c++ **
// End: **
