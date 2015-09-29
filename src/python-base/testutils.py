# Copyright 2015 Open vStorage NV
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

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
