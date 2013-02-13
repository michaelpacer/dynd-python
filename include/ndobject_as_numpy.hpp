//
// Copyright (C) 2011-13, DyND Developers
// BSD 2-Clause License, see LICENSE.txt
//

#ifndef _DYND__NDARRAY_AS_NUMPY_HPP_
#define _DYND__NDARRAY_AS_NUMPY_HPP_

#include <Python.h>

#include <dynd/ndobject.hpp>

namespace pydynd {

/**
 * Converts an ndobject into a NumPy object
 * using the default settings.
 *
 * \param n_obj  A WNDObject containing the ndobject to wrap.
 * \param allow_copy  If true, a copy can be made to make things fit,
 *                    otherwise produce an error instead of a copy.
 */
PyObject *ndobject_as_numpy(PyObject *n_obj, bool allow_copy);

} // namespace pydynd

#endif // _DYND__NDARRAY_AS_NUMPY_HPP_