#ifndef VFS_STRINGY_CONVERTER_H_
#define VFS_STRINGY_CONVERTER_H_

#include <typeinfo>

#include <boost/filesystem/path.hpp>
#include <boost/python/object.hpp>
#include <boost/python/to_python_converter.hpp>

// pythonX.Y/patchlevel.h
#include <patchlevel.h>

#include <youtils/Assert.h>
#include <youtils/Logging.h>

// TODO: move to another namespace / location. youtils? youtils::pyconverters?
namespace volumedriverfs
{

namespace python
{

// We could avoid copying by offering a 'const char* c_str()' trait (as that's what
// we actually want - see callsite below), but that could lead to problems with
// stringstreams or other types whose str() create temporaries.
template<typename S>
struct StringyConverterTraits
{
    static std::string
    str(const S& s)
    {
        return s.str();
    }
};

template<>
struct StringyConverterTraits<boost::filesystem::path>
{
    static std::string
    str(const boost::filesystem::path& p)
    {
        return p.string();
    }
};

// Gee, this is complicated and under-documented. Taken from:
// http://stackoverflow.com/questions/15842126/feeding-a-python-list-into-a-function-taking-in-a-vector-with-boost-python
template<typename S, typename Traits = StringyConverterTraits<S> >
struct StringyConverter
{
    DECLARE_LOGGER("StringyConverter");

    static PyObject*
    convert(const S& s)
    {
        LOG_TRACE("Converting " << typeid(S).name() << " \"" << s << "\" to python");

        boost::python::object o(Traits::str(s).c_str());
        return boost::python::incref(o.ptr());
    }

    static void*
    convertible(PyObject* o)
    {
#if PY_MAJOR_VERSION < 3
        LOG_TRACE(PyString_AsString(o)  << "convertible to " << typeid(S).name() << "?");
        return PyString_Check(o) ? o : nullptr;
#else
        LOG_TRACE(PyUnicode_AsASCIIString(o) << "convertible to " << typeid(S).name() << "?");
        return PyUnicode_Check(o) ? o : nullptr;
#endif
    }

    static void
    from_python(PyObject* o,
                boost::python::converter::rvalue_from_python_stage1_data* data)
    {
        boost::python::handle<> handle(boost::python::borrowed(o));
#if PY_MAJOR_VERSION < 3
        const char* cstr = PyString_AsString(handle.get());
#else
        const PyObject* tmp = PyUnicode_AsASCIIString(handle.get());
        const char* cstr = PyBytes_AS_STRING(tmp);
#endif
        VERIFY(cstr);

        LOG_TRACE("Converting " << typeid(S).name() << " \"" << cstr << "\" from python");

        typedef boost::python::converter::rvalue_from_python_storage<S> storage_type;
        void* storage = reinterpret_cast<storage_type*>(data)->storage.bytes;
        data->convertible = new(storage) S(cstr);
    }

    static void
    registerize()
    {
        boost::python::converter::registry::push_back(&StringyConverter<S,
                                                                        Traits>::convertible,
                                                      &StringyConverter<S,
                                                                        Traits>::from_python,
                                                      boost::python::type_id<S>());

        boost::python::to_python_converter<S, StringyConverter<S, Traits>>();
    }
};

}

}

#define REGISTER_STRINGY_CONVERTER(t)                   \
    volumedriverfs::python::StringyConverter<t>::registerize()

#endif // !VFS_STRINGY_CONVERTER_H_
