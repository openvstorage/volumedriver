# Copyright 2015 iNuron NV
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
