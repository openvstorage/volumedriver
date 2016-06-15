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
import shutil
import os
import tempfile
from volumedriver.base import env
import logging


def logToConsole():
    ch = logging.StreamHandler()
    ch.setLevel(logging.DEBUG)
    formatter = logging.Formatter(
        "%(asctime)s - %(name)s - %(levelname)s - %(message)s")
    ch.setFormatter(formatter)
    env.logger.addHandler(ch)


class TestWithDir(unittest.TestCase):

    def __init__(self, methodName, theDir):
        unittest.TestCase.__init__(self, methodName)
        self._dir = theDir.rstrip('/')
        self._toKeep = 2

    def _rotatedDir(self, i):
        if i == 0:
            return self._dir
        else:
            return self._dir + "." + str(i)

    def setUp(self):
        unittest.TestCase.setUp(self)

        if os.path.exists(self._rotatedDir(self._toKeep)):
            shutil.rmtree(self._rotatedDir(self._toKeep))

        for i in xrange(self._toKeep, 0, -1):
            if os.path.exists(self._rotatedDir(i - 1)):
                shutil.move(self._rotatedDir(i - 1), self._rotatedDir(i))

        os.mkdir(self._dir)

    def tearDown(self):
        unittest.TestCase.tearDown(self)

    def getDir(self):
        return self._dir

    def pathTo(self, *args):
        return os.path.join(self._dir, *args)

    def makeDirs(self, *args):
        pth = self.pathTo(*args)
        if not os.path.exists(pth):
            os.makedirs(pth)
        return pth


class TestWithTempDir(unittest.TestCase):

    def __init__(self, methodName, prefix=None):
        unittest.TestCase.__init__(self, methodName)
        self._prefix = prefix if prefix else "TestWithTempDir"

    def setUp(self):
        unittest.TestCase.setUp(self)
        self._dir = tempfile.mkdtemp(prefix=self._prefix)

    def tearDown(self):
        shutil.rmtree(self._dir)
        unittest.TestCase.tearDown(self)

    def getDir(self):
        return self._dir

    def pathTo(self, pth):
        return os.path.join(self._dir, pth)
