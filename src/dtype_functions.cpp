//
// Copyright (C) 2011-13 Mark Wiebe, DyND Developers
// BSD 2-Clause License, see LICENSE.txt
//

#include "dtype_functions.hpp"
#include "array_functions.hpp"
#include "numpy_interop.hpp"
#include "ctypes_interop.hpp"
#include "utility_functions.hpp"

#include <dynd/dtypes/convert_dtype.hpp>
#include <dynd/dtypes/fixedstring_type.hpp>
#include <dynd/dtypes/string_type.hpp>
#include <dynd/dtypes/bytes_dtype.hpp>
#include <dynd/dtypes/pointer_dtype.hpp>
#include <dynd/dtypes/struct_type.hpp>
#include <dynd/dtypes/cstruct_type.hpp>
#include <dynd/dtypes/fixed_dim_type.hpp>
#include <dynd/dtypes/date_type.hpp>
#include <dynd/dtypes/datetime_dtype.hpp>
#include <dynd/dtypes/dtype_dtype.hpp>
#include <dynd/shape_tools.hpp>
#include <dynd/dtypes/builtin_dtype_properties.hpp>

// Python's datetime C API
#include "datetime.h"

using namespace std;
using namespace dynd;
using namespace pydynd;

// Initialize the pydatetime API
namespace {
struct init_pydatetime {
    init_pydatetime() {
#if !(PY_MAJOR_VERSION == 2 && PY_MINOR_VERSION <= 6)
        PyDateTime_IMPORT;
#else
        // The Python 2 API isn't const-correct, was causing build failures on some configurations
        // This is a copy/paste of the macro to here, with an explicit cast added.
        PyDateTimeAPI = (PyDateTime_CAPI*) PyCObject_Import((char *) "datetime",
                                                            (char *) "datetime_CAPI");
#endif
    }
};
init_pydatetime pdt;
} // anonymous namespace

PyTypeObject *pydynd::WType_Type;

void pydynd::init_w_type_typeobject(PyObject *type)
{
    pydynd::WType_Type = (PyTypeObject *)type;
}

std::string pydynd::dtype_repr(const dynd::ndt::type& d)
{
    std::stringstream ss;
    if (d.is_builtin() &&
                    d.get_type_id() != dynd::complex_float32_type_id &&
                    d.get_type_id() != dynd::complex_float64_type_id) {
        ss << "ndt." << d;
    } else {
        switch (d.get_type_id()) {
            case complex_float32_type_id:
                ss << "ndt.cfloat32";
                break;
            case complex_float64_type_id:
                ss << "ndt.cfloat64";
                break;
            case date_type_id:
                ss << "ndt.date";
                break;
            case json_type_id:
                ss << "ndt.json";
                break;
            case bytes_type_id:
                if (static_cast<const bytes_dtype *>(d.extended())->get_target_alignment() == 1) {
                    ss << "ndt.bytes";
                } else {
                    ss << "ndt.type('" << d << "')";
                }
                break;
            case string_type_id:
                if (static_cast<const string_type *>(
                            d.extended())->get_encoding() == string_encoding_utf_8) {
                    ss << "ndt.string";
                } else {
                    ss << "ndt.type('" << d << "')";
                }
                break;
            default:
                ss << "ndt.type('" << d << "')";
                break;
        }
    }
    return ss.str();
}

PyObject *pydynd::dtype_get_kind(const dynd::ndt::type& d)
{
    stringstream ss;
    ss << d.get_kind();
    string s = ss.str();
#if PY_VERSION_HEX >= 0x03000000
    return PyUnicode_FromStringAndSize(s.data(), s.size());
#else
    return PyString_FromStringAndSize(s.data(), s.size());
#endif
}

PyObject *pydynd::dtype_get_type_id(const dynd::ndt::type& d)
{
    stringstream ss;
    ss << d.get_type_id();
    string s = ss.str();
#if PY_VERSION_HEX >= 0x03000000
    return PyUnicode_FromStringAndSize(s.data(), s.size());
#else
    return PyString_FromStringAndSize(s.data(), s.size());
#endif
}

ndt::type pydynd::deduce_dtype_from_pyobject(PyObject* obj)
{
#if DYND_NUMPY_INTEROP
    if (PyArray_Check(obj)) {
        // Numpy array
        PyArray_Descr *d = PyArray_DESCR((PyArrayObject *)obj);
        return dtype_from_numpy_dtype(d);
    } else if (PyArray_IsScalar(obj, Generic)) {
        // Numpy scalar
        return dtype_of_numpy_scalar(obj);
    }
#endif // DYND_NUMPY_INTEROP
    
    if (PyBool_Check(obj)) {
        // Python bool
        return ndt::make_dtype<dynd_bool>();
#if PY_VERSION_HEX < 0x03000000
    } else if (PyInt_Check(obj)) {
        // Python integer
# if SIZEOF_LONG > SIZEOF_INT
        long value = PyInt_AS_LONG(obj);
        // Use a 32-bit int if it fits. This conversion strategy
        // is independent of sizeof(long), and is the same on 32-bit
        // and 64-bit platforms.
        if (value >= INT_MIN && value <= INT_MAX) {
            return ndt::make_dtype<int>();
        } else {
            return ndt::make_dtype<long>();
        }
# else
        return ndt::make_dtype<int>();
# endif
#endif // PY_VERSION_HEX < 0x03000000
    } else if (PyLong_Check(obj)) {
        // Python integer
        PY_LONG_LONG value = PyLong_AsLongLong(obj);
        if (value == -1 && PyErr_Occurred()) {
            throw runtime_error("error converting int value");
        }
        // Use a 32-bit int if it fits. This conversion strategy
        // is independent of sizeof(long), and is the same on 32-bit
        // and 64-bit platforms.
        if (value >= INT_MIN && value <= INT_MAX) {
            return ndt::make_dtype<int>();
        } else {
            return ndt::make_dtype<PY_LONG_LONG>();
        }
    } else if (PyFloat_Check(obj)) {
        // Python float
        return ndt::make_dtype<double>();
    } else if (PyComplex_Check(obj)) {
        // Python complex
        return ndt::make_dtype<complex<double> >();
#if PY_VERSION_HEX < 0x03000000
    } else if (PyString_Check(obj)) {
        // Python string
        return make_string_type();
#else
    } else if (PyBytes_Check(obj)) {
        // Python bytes string
        return make_bytes_dtype(1);
#endif
    } else if (PyUnicode_Check(obj)) {
        // Python string
        return make_string_type();
    } else if (PyDateTime_Check(obj)) {
        if (((PyDateTime_DateTime *)obj)->hastzinfo &&
                        ((PyDateTime_DateTime *)obj)->tzinfo != NULL) {
            throw runtime_error("Converting datetimes with a timezone to dynd arrays is not yet supported");
        }
        return make_datetime_dtype(datetime_unit_usecond, tz_abstract);
    } else if (PyDate_Check(obj)) {
        return make_date_type();
    } else if (WType_Check(obj)) {
        return make_dtype_dtype();
    } else if (PyType_Check(obj)) {
        return make_dtype_dtype();
#if DYND_NUMPY_INTEROP
    } else if (PyArray_DescrCheck(obj)) {
        return make_dtype_dtype();
#endif // DYND_NUMPY_INTEROP
    }

    throw std::runtime_error("could not deduce pydynd type from the python object");
}

/**
 * Creates a dynd::dtype out of typical Python typeobjects.
 */
static dynd::ndt::type make_dtype_from_pytypeobject(PyTypeObject* obj)
{
    if (obj == &PyBool_Type) {
        return ndt::make_dtype<dynd_bool>();
#if PY_VERSION_HEX < 0x03000000
    } else if (obj == &PyInt_Type) {
        return ndt::make_dtype<int32_t>();
#endif
    } else if (obj == &PyLong_Type) {
        return ndt::make_dtype<int32_t>();
    } else if (obj == &PyFloat_Type) {
        return ndt::make_dtype<double>();
    } else if (obj == &PyComplex_Type) {
        return ndt::make_dtype<complex<double> >();
    } else if (PyObject_IsSubclass((PyObject *)obj, ctypes.PyCData_Type)) {
        // CTypes type object
        return dtype_from_ctypes_cdatatype((PyObject *)obj);
    } else if (obj == PyDateTimeAPI->DateType) {
        return make_date_type();
    }

    throw std::runtime_error("could not convert the given Python TypeObject into a dynd::dtype");
}

dynd::ndt::type pydynd::make_dtype_from_pyobject(PyObject* obj)
{
    if (WType_Check(obj)) {
        return ((WType *)obj)->v;
#if PY_VERSION_HEX < 0x03000000
    } else if (PyString_Check(obj)) {
        return ndt::type(pystring_as_string(obj));
#endif
    } else if (PyUnicode_Check(obj)) {
        return ndt::type(pystring_as_string(obj));
    } else if (WArray_Check(obj)) {
        return ((WArray *)obj)->v.as<ndt::type>();
    } else if (PyType_Check(obj)) {
#if DYND_NUMPY_INTEROP
        ndt::type result;
        if (dtype_from_numpy_scalar_typeobject((PyTypeObject *)obj, result) == 0) {
            return result;
        }
#endif // DYND_NUMPY_INTEROP
        return make_dtype_from_pytypeobject((PyTypeObject *)obj);
    }


#if DYND_NUMPY_INTEROP
    if (PyArray_DescrCheck(obj)) {
        return dtype_from_numpy_dtype((PyArray_Descr *)obj);
    }
#endif // DYND_NUMPY_INTEROP

    stringstream ss;
    ss << "could not convert the object ";
    pyobject_ownref repr(PyObject_Repr(obj));
    ss << pystring_as_string(repr.get());
    ss << " into a dynd::dtype";
    throw std::runtime_error(ss.str());
}

static string_encoding_t encoding_from_pyobject(PyObject *encoding_obj)
{
    // Default is utf-8
    if (encoding_obj == Py_None) {
        return string_encoding_utf_8;
    }

    string_encoding_t encoding = string_encoding_invalid;
    string encoding_str = pystring_as_string(encoding_obj);
    switch (encoding_str.size()) {
    case 5:
        switch (encoding_str[1]) {
        case 'c':
            if (encoding_str == "ucs_2" || encoding_str == "ucs-2") {
                encoding = string_encoding_ucs_2;
            }
            break;
        case 's':
            if (encoding_str == "ascii") {
                encoding = string_encoding_ascii;
            }
            break;
        case 't':
            if (encoding_str == "utf_8" || encoding_str == "utf-8") {
                encoding = string_encoding_utf_8;
            }
            break;
        }
        break;
    case 6:
        switch (encoding_str[4]) {
        case '1':
            if (encoding_str == "utf_16" || encoding_str == "utf-16") {
                encoding = string_encoding_utf_16;
            }
            break;
        case '3':
            if (encoding_str == "utf_32" || encoding_str == "utf-32") {
                encoding = string_encoding_utf_32;
            }
            break;
        }
    }

    if (encoding != string_encoding_invalid) {
        return encoding;
    } else {
        stringstream ss;
        ss << "invalid input \"" << encoding_str << "\" for string encoding";
        throw std::runtime_error(ss.str());
    }
}

dynd::ndt::type pydynd::dynd_make_convert_dtype(const dynd::ndt::type& to_dtype, const dynd::ndt::type& from_dtype, PyObject *errmode)
{
    return make_convert_dtype(to_dtype, from_dtype, pyarg_error_mode(errmode));
}

dynd::ndt::type pydynd::dynd_make_view_dtype(const dynd::ndt::type& value_type, const dynd::ndt::type& operand_type)
{
    return make_view_dtype(value_type, operand_type);
}

dynd::ndt::type pydynd::dynd_make_fixedstring_type(intptr_t size,
                PyObject *encoding_obj)
{
    string_encoding_t encoding = encoding_from_pyobject(encoding_obj);

    return make_fixedstring_type(size, encoding);
}

dynd::ndt::type pydynd::dynd_make_string_type(PyObject *encoding_obj)
{
    string_encoding_t encoding = encoding_from_pyobject(encoding_obj);

    return make_string_type(encoding);
}

dynd::ndt::type pydynd::dynd_make_pointer_dtype(const ndt::type& target_dtype)
{
    return make_pointer_dtype(target_dtype);
}

dynd::ndt::type pydynd::dynd_make_struct_type(PyObject *field_types, PyObject *field_names)
{
    vector<ndt::type> field_types_vec;
    vector<string> field_names_vec;
    pyobject_as_vector_dtype(field_types, field_types_vec);
    pyobject_as_vector_string(field_names, field_names_vec);
    return make_struct_type(field_types_vec, field_names_vec);
}

dynd::ndt::type pydynd::dynd_make_cstruct_type(PyObject *field_types, PyObject *field_names)
{
    vector<ndt::type> field_types_vec;
    vector<string> field_names_vec;
    pyobject_as_vector_dtype(field_types, field_types_vec);
    pyobject_as_vector_string(field_names, field_names_vec);
    if (field_types_vec.size() != field_names_vec.size()) {
        throw runtime_error("The input field types and field names lists must have the same size");
    }
    return make_cstruct_type(field_types_vec.size(), &field_types_vec[0], &field_names_vec[0]);
}

dynd::ndt::type pydynd::dynd_make_fixed_dim_type(PyObject *shape, const ndt::type& element_dtype, PyObject *axis_perm)
{
    vector<intptr_t> shape_vec;
    if (PySequence_Check(shape)) {
        pyobject_as_vector_intp(shape, shape_vec, false);
    } else {
        shape_vec.push_back(pyobject_as_index(shape));
    }

    if (axis_perm != Py_None) {
        vector<int> axis_perm_vec;
        pyobject_as_vector_int(axis_perm, axis_perm_vec);
        if (!is_valid_perm((int)axis_perm_vec.size(), axis_perm_vec.empty() ? NULL : &axis_perm_vec[0])) {
            throw runtime_error("Provided axis_perm is not a valid permutation");
        }
        if (axis_perm_vec.size() != shape_vec.size()) {
            throw runtime_error("Provided axis_perm is a different size than the provided shape");
        }
        return make_fixed_dim_type(shape_vec.size(), &shape_vec[0], element_dtype, &axis_perm_vec[0]);
    } else {
        return make_fixed_dim_type(shape_vec.size(), &shape_vec[0], element_dtype, NULL);
    }
}

dynd::ndt::type pydynd::dtype_getitem(const dynd::ndt::type& d, PyObject *subscript)
{
    // Convert the pyobject into an array of iranges
    intptr_t size;
    shortvector<irange> indices;
    if (!PyTuple_Check(subscript)) {
        // A single subscript
        size = 1;
        indices.init(1);
        indices[0] = pyobject_as_irange(subscript);
    } else {
        size = PyTuple_GET_SIZE(subscript);
        // Tuple of subscripts
        indices.init(size);
        for (Py_ssize_t i = 0; i < size; ++i) {
            indices[i] = pyobject_as_irange(PyTuple_GET_ITEM(subscript, i));
        }
    }

    // Do an indexing operation
    return d.at_array((int)size, indices.get());
}

PyObject *pydynd::dtype_array_property_names(const ndt::type& d)
{
    const std::pair<std::string, gfunc::callable> *properties;
    size_t count;
    if (!d.is_builtin()) {
        d.extended()->get_dynamic_array_properties(&properties, &count);
    } else {
        get_builtin_dtype_dynamic_array_properties(d.get_type_id(), &properties, &count);
    }
    pyobject_ownref result(PyList_New(count));
    for (size_t i = 0; i != count; ++i) {
        const string& s = properties[i].first;
#if PY_VERSION_HEX >= 0x03000000
        pyobject_ownref str_obj(PyUnicode_FromStringAndSize(s.data(), s.size()));
#else
        pyobject_ownref str_obj(PyString_FromStringAndSize(s.data(), s.size()));
#endif
        PyList_SET_ITEM(result.get(), i, str_obj.release());
    }
    return result.release();
}
