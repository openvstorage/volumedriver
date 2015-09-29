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

import os
import shutil
from volumedriver.base import env
from volumedriver.base import auxiliary
from volumedriver.toolcut import backend as Backend
from testconfig import config

_theBackend = None
namespaces_ = []
policy_id_ = None


def theBackend():
    if not _theBackend:
        raise Exception("backend not configured --- forgot to setUp?")
    return _theBackend


def setUp():
    global _theBackend  # pylint: disable=W0603
    global namespaces_  # pylint: disable=W0603
    # global policy_id_ # pylint: disable=W0603

    _theBackend = _readBackend()
    _setUpbackend(_theBackend)
    namespaces_ = []
    # policy_id_ = _theBackend.getConnection().createTestPolicy()


def tearDown():
    if _theBackend:
        c = _theBackend.getConnection()
        for ns in namespaces_:
            c.deleteNamespace(ns)

        _tearDownbackend(_theBackend)


def createNamespace(ns):
    _theBackend.getConnection().createNamespace(ns)
    namespaces_.append(ns)


def _readBackend():
    s = "backend"
    typ = None

    if s not in config.keys():
        typ = "LOCAL"
#        raise Exception("Key: %s not found in test config" % s)
    elif s not in config[s]:
        raise Exception("Key: %s not found in test config[%s]" % (s, s))
    else:
        typ = config[s][s]

    if typ == "REST":
        return Backend.RestBackend(config[s]["host"], config[s]["port"])
    elif typ == "LOCAL":
        p = os.path.join(env.tmpDir, "localbackend")
        return Backend.LocalBackend(p)
    else:
        raise Exception("unsupported backendtype: %s" % typ)


def _setUpbackend(b):
    if isinstance(b, Backend.RestBackend):
        pass
    elif isinstance(b, Backend.LocalBackend):
        auxiliary.createDirSafe(b.params()["local_connection_path"])
    else:
        raise Exception("unsupported backendtype: %s" % b)


def _tearDownbackend(b):
    if isinstance(b, Backend.RestBackend):
        pass
    elif isinstance(b, Backend.LocalBackend):
        shutil.rmtree(b.params()["local_connection_path"])
    else:
        raise Exception("unsupported backendtype: %s" % b)
