//
// Copyright (C) 2011-13, DyND Developers
// BSD 2-Clause License, see LICENSE.txt
//

#ifndef _DYND__PY_LOWLEVEL_API_HPP_
#define _DYND__PY_LOWLEVEL_API_HPP_

#include <dynd/lowlevel_api.hpp>

#include "ndobject_functions.hpp"
#include "dtype_functions.hpp"

namespace pydynd {

/**
 * This struct contains a bunch of function which provide
 * low level C-level access to the innards of dynd's python
 * exposure.
 *
 * These functions are static and should not be modified
 * after initialization.
 */
struct py_lowlevel_api_t {
    uintptr_t version;
    // Extracts the dynd object pointers from their Python wrappers.
    // These functions do not check the type of the arguments.
    dynd::ndobject_preamble *(*get_ndobject_ptr)(WNDObject *obj);
    const dynd::base_dtype *(*get_base_dtype_ptr)(WDType *obj);
    PyObject *(*ndobject_from_ptr)(PyObject *dt, PyObject *ptr, PyObject *owner, PyObject *access);
    PyObject *(*make_single_assignment_kernel)(PyObject *dst_dt_obj, PyObject *src_dt_obj, PyObject *kerntype, void *out_dki_ptr);
};

} // namespace pydynd

/**
 * Returns a pointer to the static low level API structure.
 */
extern "C" const void *dynd_get_py_lowlevel_api();

#endif // _DYND__PY_LOWLEVEL_API_HPP_