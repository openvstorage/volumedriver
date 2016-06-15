# Copyright (C) 2016 iNuron NV
#
# This file is part of Open vStorage Open Source Edition (OSE),
# as available from
#
#      http://www.openvstorage.org and
#      http://www.openvstorage.com.
#
# This file is free software; you can redistribute it and/or modify it
# under the terms of the GNU Affero General Public License v3 (GNU AGPLv3)
# as published by the Free Software Foundation, in version 3 as it comes in
# the LICENSE.txt file of the Open vStorage OSE distribution.
# Open vStorage is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY of any kind.

import unittest
from nose.tools import ok_

import volumedriver.base.testutils as testutils
from volumedriver.base.auxiliary import divCeiling, ErrorReportingThread, multipleCeiling, multipleFloor, recursiveOverlayDict
import volumedriver.base.auxiliary as auxiliary
import os
import subprocess


class FooException(Exception):
    pass


class Test_Aux(testutils.TestWithTempDir):

    def setUp(self):
        testutils.TestWithTempDir.setUp(self)

    def testDivCeiling(self):
        self.assertEqual(divCeiling(97, 10), 10)
        self.assertEqual(divCeiling(100, 10), 10)
        self.assertEqual(divCeiling(101, 10), 11)
        self.assertEqual(divCeiling(0, 10), 0)
        self.assertEqual(divCeiling(-1, 10), 0)
        self.assertEqual(divCeiling(-91, 10), -9)

    def testMultipleCeiling(self):
        self.assertEqual(multipleCeiling(79, 10), 80)
        self.assertEqual(multipleCeiling(80, 10), 80)
        self.assertEqual(multipleCeiling(81, 10), 90)
        self.assertEqual(multipleCeiling(-91, 10), -90)
        self.assertEqual(multipleCeiling(-90, 10), -90)
        self.assertEqual(multipleCeiling(-89, 10), -80)

    def testMultipleFloor(self):
        self.assertEqual(multipleFloor(79, 10), 70)
        self.assertEqual(multipleFloor(80, 10), 80)
        self.assertEqual(multipleFloor(-81, 10), -90)

    def testRecursiveUpdate(self):
        target = {"a": {"b": {"c": 3},
                        "d": 4,
                        "e": 5},
                  "f": 6}

        source1 = {"a": {"e": 10,
                         "g": 11}}

        self.assertEqual(recursiveOverlayDict(target, source1),
                         {"a": {"b": {"c": 3},
                                "d": 4,
                                "e": 10,
                                "g": 11},
                          "f": 6})

        # value assignment to a dict not allowed
        self.assertRaises(ValueError, recursiveOverlayDict, target, {"a": 4})
        self.assertRaises(
            ValueError, recursiveOverlayDict, target, {"f": {"r": 2}})

        self.assertEqual(
            recursiveOverlayDict(target, {"a": {"b": {"c": 7}}}),
            {"a": {"b": {"c": 7},
                   "d": 4,
                   "e": 5},
             "f": 6})

        recursiveOverlayDict(target, {"f": 7})
        self.assertRaises(
            ValueError, recursiveOverlayDict, target, {"f": 7}, False)

    def testWithTempFile1(self):
        d = None
        with auxiliary.cleanedUpTempDir(self.getDir(), "D1") as tmp:
            self.assertTrue(os.path.exists(tmp))
            d = tmp

        self.assertFalse(os.path.exists(d))

    def testWithTempFile2(self):
        try:
            d = None
            with auxiliary.cleanedUpTempDir(self.getDir(), "D1") as tmp:
                self.assertTrue(os.path.exists(tmp))
                d = tmp
                raise FooException

            self.assertFalse(os.path.exists(d))

        except FooException:
            pass

    def testWithTempFile3(self):
        try:
            d = None
            with auxiliary.cleanedUpTempDir(self.getDir(), "D1", False) as tmp:
                d = tmp
                raise FooException

            self.assertTrue(os.path.exists(d))

        except FooException:
            pass

    def testWithTempFile4(self):
        with auxiliary.cleanedUpTempDir(self.getDir(), "D1") as d1:
            with auxiliary.cleanedUpTempDir(d1, "D2") as d2:
                with auxiliary.cleanedUpTempDir(d2, "D3") as d3:
                    with auxiliary.cleanedUpTempDir(d3, "D4") as d4:
                        self.assertTrue(os.path.exists(d4))
                    self.assertTrue(os.path.exists(d3))
                self.assertTrue(os.path.exists(d2))
            self.assertTrue(os.path.exists(d1))

    def testpidAlive(self):
        self.assertTrue(auxiliary.isPidAlive(os.getpid()))
        proc = subprocess.Popen(["ls", "-al"])
        proc.communicate()
        proc.wait()
        self.assertFalse(auxiliary.isPidAlive(proc.pid))

    def testWithUpdatedDict(self):
        d = {"a": 1, "b": 2}
        with auxiliary.updatedDict(d, {"b": 30, "c": 40}):
            self.assertEqual(d, {"a": 1, "b": 30, "c": 40})

        self.assertEqual(d, {"a": 1, "b": 2})

    def testErrorReportingThread(self):
        def errorFromThreadWithException(exctype):
            def runner():
                if exctype:
                    raise exctype()

            t = ErrorReportingThread(target=runner)
            t.start()
            t.join()
            return t.error

        self.assertEqual(None, errorFromThreadWithException(None))
        self.assertTrue(
            isinstance(errorFromThreadWithException(TypeError), TypeError))
        self.assertTrue(
            isinstance(errorFromThreadWithException(Exception), Exception))
