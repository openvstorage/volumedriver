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
import random

import volumedrivertest.voldrv.testbackend as testbackend


class Backend_test(unittest.TestCase):

    def setUp(self):
        unittest.TestCase.setUp(self)
        testbackend.setUp()
        ns = "backend-test"
        testbackend.createNamespace(ns)
        self.target = testbackend.theBackend()(ns)

    def _randomBackendName(self):
        return "".join(chr(random.randrange(ord('a'), ord('z'))) for x in xrange(10))

    def checkRoundtrip(self, obj):
        n = self._randomBackendName()
        self.target.putObject(obj, n, False)
        self.assertEqual(obj, self.target.getObject(n))

    def tearDown(self):
        testbackend.tearDown()
        unittest.TestCase.tearDown(self)
