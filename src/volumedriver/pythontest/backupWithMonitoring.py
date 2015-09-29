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

import time
import threading
from volumedriver.pitreplication import PITReplication


class ThreadWithExn(threading.Thread):

    def __init__(self,
                 group=None,
                 target=None,
                 name=None,
                 args=(),
                 kwargs=None,
                 verbose=None):
        threading.Thread.__init__(self,
                                  group=group,
                                  target=target,
                                  name=name,
                                  args=args,
                                  kwargs=kwargs,
                                  verbose=verbose)
        self._exn = None

    def run(self):
        try:
            threading.Thread.run(self)
        except Exception as e:  # pylint: disable=W0703
            self._exn = e

    def join_with_exception(self):
        self.join()
        if self._exn != None:
            raise self._exn  # pylint: disable=E0702


def backupWithMonitoring(self, source, target, *args, **kwargs):
    infoList = []

    PITReplication.removeProgressInfo(target)
    backupThread = ThreadWithExn(target=PITReplication.backup,
                                 args=((source, target) + args),
                                 kwargs=kwargs)
    backupThread.start()

    progressInfo = None

    def check_progress(self, old, new, infoList):
        # check monotonicity
        self.assertTrue(old == None or new != None)

        if old != None:
            self.assertEqual(new["total_size"], old["total_size"])
            self.assertTrue(new["seen"] >= old["seen"])
            self.assertTrue(new["sent_to_backend"] >= old["sent_to_backend"])

        if new != None:
            self.assertTrue(new["status"] in ["running", "finished"])
            if old == None or new["seen"] > old["seen"]:
                infoList.append(new)

    previoustime = time.time()
    delta = 1.0

    while backupThread.isAlive():
        time.sleep(0.01)
        newtime = time.time()
        if newtime > previoustime + delta:
            previoustime = newtime
            newProgressInfo = PITReplication.getProgressInfo(target)
            check_progress(self, progressInfo, newProgressInfo, infoList)
            progressInfo = newProgressInfo

    backupThread.join_with_exception()
    newProgressInfo = PITReplication.getProgressInfo(target)
    check_progress(self, progressInfo, newProgressInfo, infoList)
    self.assertNotEqual(
        newProgressInfo, None, "no progressinfo found after backup has successfully finished")
    self.assertEqual(newProgressInfo["status"], "finished")

    self.assertTrue(len(infoList) >= 1,
                    "internal test error in backupWithMonitoring")

    return infoList
