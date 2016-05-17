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
