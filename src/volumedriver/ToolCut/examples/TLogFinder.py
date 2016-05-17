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

import ToolCut


class TLogFinder():

    class FoundException():
        pass

    def __init__(self, filename, tlogname):
        self.file_name = filename
        self.tlog_name = tlogname
        self.count = 0

    def __call__(self):
        try:
            snappers = ToolCut.SnapshotPersistor(self.file_name)
            snappers.forEach(self.onSnapshot)
            return False
        except self.FoundException:
            return True

    def onSnapshot(self, snap_):
        self.snap = snap_
        self.count = 0
        self.snap.forEach(self.tlog_finder)

    def tlog_finder(self, tlog):
        if(tlog.name() == self.tlog_name):
            print "Found " + self.tlog_name + " in snapshot " + self.snap.name() + " at offset " + str(self.count)
            self.snap = self.snap.name()
            raise self.FoundException()
        else:
            self.count += 1

val = TLogFinder('testfile', 'tlog_808df477-8b09-494b-b356-990ba4fbfb1d')
