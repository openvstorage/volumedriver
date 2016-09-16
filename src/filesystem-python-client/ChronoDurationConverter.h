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

#ifndef VFS_CHRONO_CONVERTER_H_
#define VFS_CHRONO_CONVERTER_H_

#include <boost/python/object.hpp>
#include <boost/python/to_python_converter.hpp>

// pythonX.Y/patchlevel.h
#include <patchlevel.h>

#include <youtils/Logging.h>

// TODO:
// - Move to another namespace / location. youtils? youtils::pyconverters?
// - Try to unify with StrongArithmeticTypedefConverter
namespace volumedriverfs
{

template<typename T>
struct ChronoDurationConverter
{
    DECLARE_LOGGER("ChronoDurationConverter");

    static PyObject*
    convert(const T& t)
    {
        boost::python::object o(t.count());
        return boost::python::incref(o.ptr());
    }

    static void*
    convertible(PyObject* o)
    {
        if (
#if PY_MAJOR_VERSION < 3
            PyInt_Check(o)
#else
            PyLong_Check(o)
#endif
            )
        {
            long l =
#if PY_MAJOR_VERSION < 3
                       PyInt_AsLong(o)
#else
                       PyLong_AsLong(o)
#endif
                       ;

            if (l == -1)
            {
                if (PyErr_Occurred())
                {
                    PyErr_Print();
                }

                return nullptr;
            }

            if (l >= static_cast<long>(T::min().count()) and
                l <= static_cast<long>(T::max().count()))
            {
                return o;
            }
        }

        return nullptr;
    }

    static void
    from_python(PyObject* o,
                boost::python::converter::rvalue_from_python_stage1_data* data)
    {
        const long l =
#if PY_MAJOR_VERSION < 3
                       PyInt_AsLong(o)
#else
                       PyLong_AsLong(o)
#endif
            ;
        assert(not (l == -1 and PyErr_Occurred()));
        typename T::rep t = l;

        typedef boost::python::converter::rvalue_from_python_storage<T> storage_type;

        void* storage = reinterpret_cast<storage_type*>(data)->storage.bytes;
        data->convertible = new(storage) T(t);
    }

    static void
    registerize()
    {
        boost::python::converter::registry::push_back(&ChronoDurationConverter<T>::convertible,
                                                      &ChronoDurationConverter<T>::from_python,
                                                      boost::python::type_id<T>());

        boost::python::to_python_converter<T, ChronoDurationConverter<T>>();
    }

};

}

#define REGISTER_CHRONO_DURATION_CONVERTER(t) \
    volumedriverfs::ChronoDurationConverter<t>::registerize()

#endif // !VFS_CHRONO_DURATION_CONVERTER_H_
