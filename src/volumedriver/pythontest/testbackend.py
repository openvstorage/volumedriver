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
