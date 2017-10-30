#ifndef YPY_OPTIONAL_CONVERTER_H_
#define YPY_OPTIONAL_CONVERTER_H_

#include "Wrapper.h"

#include <boost/optional.hpp>
#include <boost/python.hpp>
#include <boost/python/extract.hpp>
#include <boost/python/object.hpp>
#include <boost/python/to_python_converter.hpp>

#include <youtils/Logging.h>

namespace youtils
{

namespace python
{

template<typename T>
struct OptionalConverter
{
    typedef boost::optional<T> optional_t;

    DECLARE_LOGGER("OptionalConverter");

    static PyObject*
    convert(const optional_t& opt)
    {
        if (opt)
        {
            boost::python::object o(*opt);
            return boost::python::incref(o.ptr());
        }
        else
        {
            boost::python::object o;
            return boost::python::incref(o.ptr());
        }
    }

    static void*
    convertible(PyObject* o)
    {
        if (o == Py_None or boost::python::extract<T>(o).check())
        {
            return o;
        }
        else
        {
            return nullptr;
        }
    }

    static void
    from_python(PyObject* o,
                boost::python::converter::rvalue_from_python_stage1_data* data)
    {
        typedef boost::python::converter::rvalue_from_python_storage<optional_t> storage_t;
        void* storage = reinterpret_cast<storage_t*>(data)->storage.bytes;

        if (o == Py_None)
        {
            data->convertible = new(storage) optional_t();
        }
        else
        {
            data->convertible = new(storage) optional_t(boost::python::extract<T>(o));
        }
    }

    static void
    registerize()
    {
        boost::python::converter::registry::push_back(&OptionalConverter<T>::convertible,
                                                      &OptionalConverter<T>::from_python,
                                                      boost::python::type_id<optional_t>());

        boost::python::to_python_converter<optional_t, OptionalConverter<T>>();
    }
};

}

}

#define REGISTER_OPTIONAL_CONVERTER(T)                                  \
    youtils::python::register_once<youtils::python::OptionalConverter<T>>()

#endif // !YPY_OPTIONAL_CONVERTER_H_
