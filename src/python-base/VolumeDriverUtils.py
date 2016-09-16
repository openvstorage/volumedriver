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

import volumedriver.base.auxiliary as auxiliary
import ConfigParser


VOL_PARAMS_NAME = "parameters.ini"
SESSION_ID_NAME = "session_id.ini"
SESSION_ID_VOID = "void"


VD_WRONG_VOLUMENAME = "Wrong volume name"
VD_PATHS_DO_NOT_EXIST = "Local paths do not exist"
VD_L_LOCK_ERROR = "Local restart of volume locked by another session"
VD_NL_LOCK_ERROR = "Non-local restart of a locked volume"
VD_FOC_NOT_AVAILABLE = "Failovercache was not configured: cannot be use on restart"
VD_FOC_INCOMPAT_PARAMS = "Cannot use failover cache on local restart"


FOC_OK_SYNC = "OK_SYNC"
FOC_OK_STANDALONE = "OK_STANDALONE"
FOC_CATCH_UP = "CATCHUP"
FOC_DEGRADED = "DEGRADED"

FOC_STATES = [FOC_OK_SYNC, FOC_OK_STANDALONE, FOC_CATCH_UP, FOC_DEGRADED]

FOC_NAME = "failovercache.ini"

FOC_TYPE_STANDALONE = "Standalone"
FOC_TYPE_REMOTE = "Remote"


class VolumeDriverException(Exception):

    def __init__(self, string):
        Exception.__init__(self)
        self._str = string

    def type(self):
        return self._str

    def __repr__(self):
        return "VolumeDriverException(\"%s\")" % self._str

    def __str__(self):
        return "VolumeDriverException(\"%s\")" % self._str


class Obsolete(Exception):

    def __init__(self, call):
        Exception.__init__(self)
        self._call = call

    def __str__(self):
        return "For support for the " + self._call + " function please go back to 2011 and don't come back"


def sessionIDToDict(ident):
    return {"session_id": ident}


def sessionIDFromDict(dct):
    return dct["session_id"]


def genFOCDict(focType, ip="", port=""):
    return auxiliary.mapDict(str,
                             {"failoverType": focType,
                              "failoverIP": ip,
                              "failoverPORT": port})


def getConfigParser():
    config = ConfigParser.RawConfigParser()
    config.optionxform = str
    return config


def prettySize(val):
    f1 = 1024 * 1024 * 1024
    f2 = 1024 * 1024
    if val < 10 * f1:
        return str(val / f2) + "MiB"
    else:
        return str(val / f1) + "GiB"


def getLocalPortRange():
    '''get the system's port range used for client sockets (linux specific)'''
    with open("/proc/sys/net/ipv4/ip_local_port_range", "r") as f:
        l = f.readline().split()
        return (int(l[0]), int(l[1]))


def getPortRange():
    '''get a (inclusive) range of valid ports'''
    return (1, 2 ** 16 - 1)


def verifyPortRange(begin, end):
    '''verify that the ports in the (inclusive) range from begin to end can safely
    be used for sockets'''
    assert(begin <= end)
    prange = getPortRange()
    if prange[0] <= begin and prange[1] >= end:
        return True
    else:
        return False


def verifyServerPortRange(begin, end):
    '''verify that the ports in the (inclusive) range from begin to end can safely
    be used for server sockets, i.e. the system will not interfere by allocating
    ports from that range for client sockets'''
    if verifyPortRange(begin, end):
        prange = getLocalPortRange()
        if end < prange[0] or begin > prange[1]:
            return True
        else:
            return False
    else:
        return False


def helpWithPostfixes():
    print("PostFixes supported are B, KiB, MiB,GiB,TiB (byte and 1024 powers)")
    print("and KB, MB, GB,TB (1000 powers)")
    print("They have to be attached to the number they modify i.e no space in between")

# Local Variables: **
# mode: python **
# End: **
