#ifndef VFS_STRONG_ARITHMETIC_TYPEDEF_CONVERTER_H_
#define VFS_STRONG_ARITHMETIC_TYPEDEF_CONVERTER_H_

#include <typeinfo>
#include <type_traits>

#include <boost/filesystem/path.hpp>
#include <boost/python/object.hpp>
#include <boost/python/to_python_converter.hpp>

#include <youtils/Logging.h>

// TODO:
// - Move to another namespace / location. youtils? youtils::pyconverters?
//   Or move into OurStrongTypedef?

namespace volumedriverfs
{

template<typename T>
struct StrongArithmeticTypedefConverter
{
    DECLARE_LOGGER("StrongArithmeticTypedefConverter");

    typedef decltype(T::t) arithmetic_type;

    static_assert(std::is_arithmetic<arithmetic_type>::value,
                  "only works with arithmetic types");
    static_assert(std::is_convertible<T, arithmetic_type>::value,
                  "what do you think this is?");

    // Sorry, only Python Ints for now. Other arithmetic types will be
    // added later on (potentially only once the need arises).
    static_assert(std::is_integral<arithmetic_type>::value,
                  "TODO: support for non-integral types");
    static_assert(sizeof(long) >= sizeof(arithmetic_type),
                  "TODO: support types other than Python Ints");

    static PyObject*
    convert(const T& t)
    {
        boost::python::object o(static_cast<arithmetic_type>(t));
        return boost::python::incref(o.ptr());
    }

    static void*
    convertible(PyObject* o)
    {
        if (PyLong_Check(o))
        {
            long l = PyLong_AsLong(o);
            if (l == -1)
            {
                if (PyErr_Occurred())
                {
                    PyErr_Print();
                }

                return nullptr;
            }

            if (l >= std::numeric_limits<arithmetic_type>::min() and
                l <= std::numeric_limits<arithmetic_type>::max())
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
        arithmetic_type t = PyLong_AsLong(o);
        assert(not (t == -1 and PyErr_Occurred()));

        typedef boost::python::converter::rvalue_from_python_storage<T> storage_type;

        void* storage = reinterpret_cast<storage_type*>(data)->storage.bytes;
        data->convertible = new(storage) T(t);
    }

    static void
    registerize()
    {
        boost::python::converter::registry::push_back(&StrongArithmeticTypedefConverter<T>::convertible,
                                                      &StrongArithmeticTypedefConverter<T>::from_python,
                                                      boost::python::type_id<T>());

        boost::python::to_python_converter<T, StrongArithmeticTypedefConverter<T>>();
    }

};

}

#define REGISTER_STRONG_ARITHMETIC_TYPEDEF_CONVERTER(t) \
    volumedriverfs::StrongArithmeticTypedefConverter<t>::registerize()

#endif // !VFS_STRONG_ARITHMETIC_TYPEDEF_CONVERTER_H_
