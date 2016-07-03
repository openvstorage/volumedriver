#ifndef VFS_INTEGER_CONVERTER_H_
#define VFS_INTEGER_CONVERTER_H_

#include <type_traits>
#include <typeinfo>

#include <boost/filesystem/path.hpp>
#include <boost/python/object.hpp>
#include <boost/python/to_python_converter.hpp>

#include <youtils/Assert.h>

namespace volumedriverfs
{

namespace python
{

TODO("AR: protect against overflows");
template<typename T>
struct IntegerConverter
{
    static PyObject*
    convert(const T t)
    {
        return PyLong_FromLong(static_cast<long>(t));
    }

    static void*
    convertible(PyObject* o)
    {
        return PyLong_Check(o) ? o : nullptr;
    }

    static void
    from_python(PyObject* o,
                boost::python::converter::rvalue_from_python_stage1_data* data)
    {
        using namespace boost::python;

        handle<> handle(borrowed(o));
        const T t(PyLong_AsLong(handle.get()));

        using Storage = converter::rvalue_from_python_storage<T>;
        void* storage = reinterpret_cast<Storage*>(data)->storage.bytes;
        data->convertible = new(storage) T(t);
    }

    static void
    registerize()
    {
        using namespace boost::python;

        converter::registry::push_back(&IntegerConverter<T>::convertible,
                                       &IntegerConverter<T>::from_python,
                                       type_id<T>());

        to_python_converter<T, IntegerConverter<T>>();
    }
};

}

}

#define REGISTER_INTEGER_CONVERTER(t)                   \
    volumedriverfs::python::IntegerConverter<t>::registerize()

#endif //!VFS_INTEGER_CONVERTER_H_
