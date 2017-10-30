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

#ifndef YPY_DICT_CONVERTER_H_
#define YPY_DICT_CONVERTER_H_

#include "PairConverter.h"
#include "Wrapper.h"

#include <type_traits>

#include <boost/python.hpp>
#include <boost/python/dict.hpp>
#include <boost/python/object.hpp>
#include <boost/python/stl_iterator.hpp>
#include <boost/python/to_python_converter.hpp>

namespace youtils
{

namespace python
{

template<typename Map>
struct DictConverter
{
    using K = typename Map::key_type;
    using V = typename Map::mapped_type;

    static PyObject*
    convert(const Map& map)
    {
        namespace bpy = boost::python;
        bpy::dict dict;

        for (const auto& p : map)
        {
            dict[p.first] = p.second;
        }
        return bpy::incref(dict.ptr());
    }

    static void*
    convertible(PyObject* o)
    {
        return PyDict_Check(o) ? o : nullptr;
    }

    static void
    from_python(PyObject* obj,
                boost::python::converter::rvalue_from_python_stage1_data* data)
    {
        namespace bpy = boost::python;

        bpy::handle<> handle(bpy::borrowed(obj));

        using Storage = bpy::converter::rvalue_from_python_storage<Map>;
        void* storage = reinterpret_cast<Storage*>(data)->storage.bytes;

        const bpy::dict dict = bpy::extract<bpy::dict>(handle.get());

        using Iterator = bpy::stl_input_iterator<typename Map::value_type>;
        data->convertible = new(storage) Map(Iterator(dict.iteritems()),
                                             Iterator());
    }

    static void
    registerize()
    {
        namespace bpy = boost::python;

        bpy::converter::registry::push_back(&DictConverter<Map>::convertible,
                                            &DictConverter<Map>::from_python,
                                            bpy::type_id<Map>());

        bpy::to_python_converter<Map , DictConverter<Map>>();
    }
};

}

}

#define REGISTER_DICT_CONVERTER_(Map)                                   \
    youtils::python::register_once<youtils::python::DictConverter<Map>>()

#define REGISTER_DICT_CONVERTER(Map)                                    \
    REGISTER_PAIR_CONVERTER(std::add_const<Map::key_type>::type, Map::mapped_type); \
    REGISTER_DICT_CONVERTER_(Map)

#endif // !YPY_DICT_CONVERTER_H_
