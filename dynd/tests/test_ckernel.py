from __future__ import print_function, absolute_import
import sys
import ctypes
import unittest
from dynd import nd, ndt, _lowlevel
import numpy as np

# ctypes.c_ssize_t/c_size_t was introduced in python 2.7
if sys.version_info >= (2, 7):
    c_ssize_t = ctypes.c_ssize_t
    c_size_t = ctypes.c_size_t
else:
    if ctypes.sizeof(ctypes.c_void_p) == 4:
        c_ssize_t = ctypes.c_int32
        c_size_t = ctypes.c_uint32
    else:
        c_ssize_t = ctypes.c_int64
        c_size_t = ctypes.c_uint64

class TestCKernelBuilder(unittest.TestCase):
    def test_creation(self):
        with _lowlevel.ckernel.CKernelBuilder() as ckb:
            pass

    def test_allocation(self):
        with _lowlevel.ckernel.CKernelBuilder() as ckb:
            # Initially, the memory within the structure
            # is being used
            self.assertEqual(ckb.ckb.data,
                        ctypes.addressof(ckb.ckb.static_data))
            # The capacity is 16 pointer-sized objects
            initial_capacity = 16 * ctypes.sizeof(ctypes.c_void_p)
            self.assertEqual(ckb.ckb.capacity, initial_capacity)
            # Requesting exactly the space already there should do nothing
            ckb.ensure_capacity(initial_capacity -
                            ctypes.sizeof(_lowlevel.CKernelPrefixStruct))
            self.assertEqual(ckb.ckb.data,
                        ctypes.addressof(ckb.ckb.static_data))
            self.assertEqual(ckb.ckb.capacity, initial_capacity)
            # Requesting more space should reallocate
            ckb.ensure_capacity(initial_capacity)
            self.assertTrue(ckb.ckb.data !=
                        ctypes.addressof(ckb.ckb.static_data))
            self.assertTrue(ckb.ckb.capacity >=
                    initial_capacity +
                        ctypes.sizeof(_lowlevel.CKernelPrefixStruct))

    def test_allocation_leaf(self):
        with _lowlevel.ckernel.CKernelBuilder() as ckb:
            # Initially, the memory within the structure
            # is being used
            self.assertEqual(ckb.ckb.data,
                        ctypes.addressof(ckb.ckb.static_data))
            # The capacity is 16 pointer-sized objects
            initial_capacity = 16 * ctypes.sizeof(ctypes.c_void_p)
            self.assertEqual(ckb.ckb.capacity, initial_capacity)
            # Requesting exactly the space already there should do nothing
            ckb.ensure_capacity_leaf(initial_capacity)
            self.assertEqual(ckb.ckb.data,
                        ctypes.addressof(ckb.ckb.static_data))
            self.assertEqual(ckb.ckb.capacity, initial_capacity)
            # Requesting more space should reallocate
            ckb.ensure_capacity(initial_capacity +
                            ctypes.sizeof(ctypes.c_void_p))
            self.assertTrue(ckb.ckb.data !=
                        ctypes.addressof(ckb.ckb.static_data))
            self.assertTrue(ckb.ckb.capacity >=
                    initial_capacity +
                        ctypes.sizeof(_lowlevel.CKernelPrefixStruct))

    def test_reset(self):
        with _lowlevel.ckernel.CKernelBuilder() as ckb:
            initial_capacity = 16 * ctypes.sizeof(ctypes.c_void_p)
            # put the ckernel builder in a non-initial state
            ckb.ensure_capacity_leaf(initial_capacity + 16)
            self.assertTrue(ckb.ckb.data !=
                        ctypes.addressof(ckb.ckb.static_data))
            # verify that reset puts it back in an initial state
            ckb.reset()
            self.assertEqual(ckb.ckb.data,
                        ctypes.addressof(ckb.ckb.static_data))
            self.assertEqual(ckb.ckb.capacity, initial_capacity)

    def test_assignment_ckernel_single(self):
        with _lowlevel.ckernel.CKernelBuilder() as ckb:
            _lowlevel.make_assignment_ckernel(
                        ndt.float32, None, ndt.int64, None,
                        "single", ckb)
            ck = ckb.ckernel(_lowlevel.UnarySingleOperation)
            # Do an assignment using ctypes
            i64 = ctypes.c_int64(1234)
            f32 = ctypes.c_float(1)
            ck(ctypes.addressof(f32), ctypes.addressof(i64))
            self.assertEqual(f32.value, 1234.0)

    def test_assignment_ckernel_strided(self):
        with _lowlevel.ckernel.CKernelBuilder() as ckb:
            _lowlevel.make_assignment_ckernel(
                        ndt.float32, None, ndt.type('string(15,"A")'), None,
                        'strided', ckb)
            ck = ckb.ckernel(_lowlevel.UnaryStridedOperation)
            # Do an assignment using a numpy array
            src = np.array(['3.25', '-1000', '1e5'], dtype='S15')
            dst = np.arange(3, dtype=np.float32)
            ck(dst.ctypes.data, 4, src.ctypes.data, 15, 3)
            self.assertEqual(dst.tolist(), [3.25, -1000, 1e5])

class TestCKernelDeferred(unittest.TestCase):
    def test_creation(self):
        with _lowlevel.ckernel.CKernelDeferred() as ckd:
            pass

    def test_assignment_ckernel(self):
        with _lowlevel.ckernel.CKernelDeferred() as ckd:
            _lowlevel.make_ckernel_deferred_from_assignment(
                        ndt.float32, ndt.int64,
                        "unary", "none", ckd)
            # Instantiate as a single kernel
            with _lowlevel.ckernel.CKernelBuilder() as ckb:
                meta = (ctypes.c_void_p * 2)()
                ckd.instantiate(ckb, 0, meta, "single")
                ck = ckb.ckernel(_lowlevel.UnarySingleOperation)
                # Do an assignment using ctypes
                i64 = ctypes.c_int64(1234)
                f32 = ctypes.c_float(1)
                ck(ctypes.addressof(f32), ctypes.addressof(i64))
                self.assertEqual(f32.value, 1234.0)
            # Instantiate as a strided kernel
            with _lowlevel.ckernel.CKernelBuilder() as ckb:
                meta = (ctypes.c_void_p * 2)()
                ckd.instantiate(ckb, 0, meta, "strided")
                ck = ckb.ckernel(_lowlevel.UnaryStridedOperation)
                # Do an assignment using ctypes
                i64 = (ctypes.c_int64 * 3)()
                for i, v in enumerate([3,7,21]):
                    i64[i] = v
                f32 = (ctypes.c_float * 3)()
                ck(ctypes.addressof(f32), 4,
                            ctypes.addressof(i64), 8,
                            3)
                self.assertEqual([f32[i] for i in range(3)], [3,7,21])

    def check_from_numpy_int32_add(self, requiregil):
        # Get int32 add as a ckernel_deferred
        with _lowlevel.ckernel.CKernelDeferred() as ckd:
            _lowlevel.ckernel_deferred_from_ufunc(np.add,
                            (np.int32, np.int32, np.int32),
                            ckd, requiregil)
            # Instantiate as a single kernel
            with _lowlevel.ckernel.CKernelBuilder() as ckb:
                meta = (ctypes.c_void_p * 3)()
                ckd.instantiate(ckb, 0, meta, "single")
                ck = ckb.ckernel(_lowlevel.ExprSingleOperation)
                a = ctypes.c_int32(10)
                b = ctypes.c_int32(21)
                c = ctypes.c_int32(0)
                src = (ctypes.c_void_p * 2)()
                src[0] = ctypes.addressof(a)
                src[1] = ctypes.addressof(b)
                ck(ctypes.addressof(c), src)
                self.assertEqual(c.value, 31)
            # Instantiate as a strided kernel
            with _lowlevel.ckernel.CKernelBuilder() as ckb:
                meta = (ctypes.c_void_p * 3)()
                ckd.instantiate(ckb, 0, meta, "strided")
                ck = ckb.ckernel(_lowlevel.ExprStridedOperation)
                a = (ctypes.c_int32 * 3)()
                b = (ctypes.c_int32 * 3)()
                c = (ctypes.c_int32 * 3)()
                for i, v in enumerate([1,4,6]):
                    a[i] = v
                for i, v in enumerate([3, -1, 12]):
                    b[i] = v
                src = (ctypes.c_void_p * 2)()
                src[0] = ctypes.addressof(a)
                src[1] = ctypes.addressof(b)
                strides = (c_ssize_t * 2)()
                strides[0] = strides[1] = 4
                ck(ctypes.addressof(c), 4, src, strides, 3)
                self.assertEqual(c[0], 4)
                self.assertEqual(c[1], 3)
                self.assertEqual(c[2], 18)

    def test_from_numpy_int32_add_nogil(self):
        self.check_from_numpy_int32_add(False)

    def test_from_numpy_int32_add_withgil(self):
        self.check_from_numpy_int32_add(True)

if __name__ == '__main__':
    unittest.main()