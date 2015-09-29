#ifndef VFS_ITERABLE_CONVERTER_H_
#define VFS_ITERABLE_CONVERTER_H_

#include <list>
#include <vector>

#include <boost/python.hpp>
#include <boost/python/stl_iterator.hpp>

// Gotten from
// http://stackoverflow.com/questions/15842126/

namespace volumedriverfs
{

namespace python
{

template <typename V>
struct VectorToListConverter
{
    static PyObject*
    convert(const V& vect)
    {
        boost::python::list lst;
        for (const auto& e : vect)
        {
            lst.append(e);
        }
        return boost::python::incref(lst.ptr());
    }
};


/// @brief Type that allows for registration of conversions from and to
///        python iterable types.
template<typename Container>
struct IterableConverter
{
    /// @note Registers converter from a python interable type to the
    ///       provided type.
    static void
    registerize()
    {
        boost::python::converter::registry::push_back(&IterableConverter<Container>::convertible,
                                                      &IterableConverter<Container>::from_python,
                                                      boost::python::type_id<Container>());

        boost::python::to_python_converter<Container , VectorToListConverter<Container>>();
    }

    /// @brief Check if PyObject is iterable.
    static void*
    convertible(PyObject* object)
    {
        return PyObject_GetIter(object) ? object : nullptr;
    }

    /// @brief Convert iterable PyObject to C++ container type.
    ///
    /// Container Concept requirements:
    ///
    ///   * Container::value_type is CopyConstructable.
    ///   * Container can be constructed and populated with two iterators.
    ///     I.e. Container(begin, end)
    static void
    from_python(PyObject* object,
                boost::python::converter::rvalue_from_python_stage1_data* data)
    {
        namespace python = boost::python;
        // Object is a borrowed reference, so create a handle indicting it is
        // borrowed for proper reference counting.
        python::handle<> handle(python::borrowed(object));

        // Obtain a handle to the memory block that the converter has allocated
        // for the C++ type.
        typedef python::converter::rvalue_from_python_storage<Container>
            storage_type;
        void* storage = reinterpret_cast<storage_type*>(data)->storage.bytes;

        typedef python::stl_input_iterator<typename Container::value_type>
            iterator;

        // Allocate the C++ type into the converter's memory block, and assign
        // its handle to the converter's convertible variable.  The C++
        // container is populated by passing the begin and end iterators of
        // the python object to the container's constructor.
        data->convertible =
            new (storage) Container(iterator(python::object(handle)), // begin
                                    iterator());                      // end
    }
};

}

}

#define REGISTER_ITERABLE_CONVERTER(t)                  \
    volumedriverfs::python::IterableConverter<t>::registerize()

#endif //VFS_ITERABLE_CONVERTER_H_
