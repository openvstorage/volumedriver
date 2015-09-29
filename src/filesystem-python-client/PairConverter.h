#ifndef VFSPY_PAIR_CONVERTER_H_
#define VFSPY_PAIR_CONVERTER_H_

#include <utility>
#include <type_traits>

#include <boost/python.hpp>
#include <boost/python/extract.hpp>
#include <boost/python/object.hpp>
#include <boost/python/to_python_converter.hpp>
#include <boost/python/tuple.hpp>

#include <youtils/Assert.h>
#include <youtils/Logging.h>

namespace volumedriverfs
{

namespace python
{

template<typename F, typename S>
struct PairConverter
{
    typedef std::pair<F, S> pair_type;

    static void*
    convertible(PyObject* o)
    {
        return (PyTuple_Check(o) and (PyTuple_Size(o) == 2)) ? o : nullptr;
    }

    static PyObject*
    convert(const pair_type& p)
    {
        boost::python::tuple t = boost::python::make_tuple(p.first, p.second);
        return boost::python::incref(t.ptr());
    }

    static void
    from_python(PyObject* o,
                boost::python::converter::rvalue_from_python_stage1_data* data)
    {
        boost::python::handle<> handle(boost::python::borrowed(o));

        PyObject* fst = PyTuple_GetItem(handle.get(), 0);
        VERIFY(fst);

        PyObject* snd = PyTuple_GetItem(handle.get(), 1);
        VERIFY(snd);

        typedef boost::python::converter::rvalue_from_python_storage<pair_type> storage_type;
        void* storage = reinterpret_cast<storage_type*>(data)->storage.bytes;

        data->convertible = new(storage) pair_type(boost::python::extract<F>(fst),
                                                   boost::python::extract<S>(snd));
    }

    static void
    registerize()
    {
        boost::python::converter::registry::push_back(&PairConverter<F, S>::convertible,
                                                      &PairConverter<F, S>::from_python,
                                                      boost::python::type_id<pair_type>());

        boost::python::to_python_converter<pair_type, PairConverter<F, S>>();
    }

    DECLARE_LOGGER("PairConverter");
};

}

}

#define REGISTER_PAIR_CONVERTER(f, s)                           \
    volumedriverfs::python::PairConverter<f, s>::registerize()

#endif // !VFSPY_PAIR_CONVERTER_H_
