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

from volumedriver.toolcut import backend as Backend
from volumedriver.pitreplication import PITReplication

import unittest
import os
import nose

import volumedrivertest.voldrv.lockedexecutable as lockedexecutable
import time

from volumedriver.base import testutils
import volumedrivertest.voldrv.testbackend as testbackend

VOLNAME = "5c17a9f4f16348b8bbc"

THISDIR = os.path.dirname(os.path.abspath(__file__))


class Locked_executable_test(unittest.TestCase):

    def __init__(self, methodName):
        unittest.TestCase.__init__(self, methodName)

    def setUp(self):
        testbackend.setUp()

    def tearDown(self):
        lockedexecutable.stopAll()
        testbackend.tearDown()

    def test_simple1(self):
        ns = "test-simple1"
        testbackend.createNamespace(ns)
        pid = lockedexecutable.start(testbackend.theBackend()(ns))
        lockedexecutable.stop(pid)

    def test_simple2(self):
        #        """ Take a lock in 2 different namespaces and release again
        #        """
        ns1 = "test-simple2-ns1"
        ns2 = "test-simple2-ns2"
        testbackend.createNamespace(ns1)
        testbackend.createNamespace(ns2)
        pid1 = lockedexecutable.start(testbackend.theBackend()(ns1))
        pid2 = lockedexecutable.start(testbackend.theBackend()(ns2))
        lockedexecutable.stop(pid1)
        lockedexecutable.stop(pid2)

    def test_sequential(self):
        #        """Or just remove any documentation about which test is running and what it's supposed to do
        #        so the next guy can spend half a day fixing it.
        #        """
        timestamp = time.time()
        ns = "test-sequential"
        testbackend.createNamespace(ns)
        session = 1
        pid = lockedexecutable.start(
            testbackend.theBackend()(ns), lock_session_timeout=session)
        lockedexecutable.stop(pid)
        pid = lockedexecutable.start(
            testbackend.theBackend()(ns), lock_session_timeout=session)
        lockedexecutable.stop(pid)
        execution_time = time.time() - timestamp
        self.assertTrue(execution_time <= session,
                        'All previous steps together should be finished within the sessiontimeout (%s) but they take %s s'
                        % (session, execution_time))

    def test_mutex(self):
        #        """ why don't we put the name of the of the test on the first line? Take a lock
        #            Try to take the lock again, this should fail (throws exception)
        #            The previous step should take at least 5 sec
        #        """
        ns = "test-mutex"
        testbackend.createNamespace(ns)
        session = 1
        pid = lockedexecutable.start(
            testbackend.theBackend()(ns), lock_session_timeout=session)
        timestamp = time.time()
        self.assertRaises(lockedexecutable.LockNotTakenException,
                          lockedexecutable.start, testbackend.theBackend()(ns))
        execution_time = time.time() - timestamp
        self.assertTrue(execution_time >= 3 * session,
                        'The previous step should take at least 3 x sessiontimeout (%s) but they take %s s'
                        % (session, execution_time))
        lockedexecutable.stop(pid)

    def test_recover(self):
        #        """ Take a lock and kill it
        #            Try to take the lock again, this should succeed
        #            The previous step should take at least 5 sec
        #        """
        ns = "test-recover"
        testbackend.createNamespace(ns)
        session = 1
        pid = lockedexecutable.start(testbackend.theBackend()(ns), session)
        lockedexecutable.kill(pid)
        timestamp = time.time()
        pid = lockedexecutable.start(testbackend.theBackend()(ns))
        execution_time = time.time() - timestamp
        self.assertTrue(execution_time >= 3 * session,
                        'The previous step should take at least 3 x sessiontimeout (%s) but they take %s s'
                        % (session, execution_time))
        lockedexecutable.stop(pid)


class Backup_locking_tests(testutils.TestWithDir):

    def __init__(self, methodName):
        testutils.TestWithDir.__init__(
            self, methodName, "/tmp/Backup_locking_tests")

    def setUp(self):
        testutils.TestWithDir.setUp(self)
        testbackend.setUp()

        self.makeDirs("scocache")
        self.source = Backend.LocalBackend(
            os.path.join(THISDIR, "data"))(VOLNAME)

    def tearDown(self):
        lockedexecutable.stopAll()
        testbackend.tearDown()
        testutils.TestWithDir.tearDown(self)

    @nose.tools.nottest
    def failbackup_test(self):
        #        """ create a namespace
        #            backup to certain snapshot -> should succeed
        #            take a lock on the namespace
        #            try to backup to the end of the volume -> this should fail
        #        """
        PITReplication.testConfiguration()
        ns = "failbackup-test"
        testbackend.createNamespace(ns)
        target = testbackend.theBackend()(ns)
        PITReplication.backup(self.source, target, startSnapshot="A")
        pid = lockedexecutable.start(target)
        self.assertRaises(Exception,
                          PITReplication.backup,
                          self.source,
                          target,
                          endSnapshot="C")
        lockedexecutable.stop(pid)

    @nose.tools.nottest
    def failscrub_test(self):
        #        """ create a namespace
        #            prepare it with at least 2 snapshots for scrubbing (cfr. backup_test -> testScrubbingAndSnapshotMgmtTarget)
        #            scrub 1 snapshot -> should succeed
        #            take a lock on the namespace
        #            scrub the other snapshot -> should fail
        #        """
        PITReplication.testConfiguration()
        ns = "failscrub-test"
        testbackend.createNamespace(ns)
        target = testbackend.theBackend()(ns)

        def check(allss, toscrubss):
            self.assertEqual(PITReplication.getVolumeSnapshots(target), allss)
            self.assertEqual(
                PITReplication.getScrubSnapshots(target), toscrubss)

        for ss in PITReplication.getVolumeSnapshots(self.source):
            PITReplication.backup(self.source, target, endSnapshot=ss)

        check(['A', 'B', 'C', 'D', 'E', 'F'], [])

        PITReplication.deleteSnapshots(target, "B")
        check(['A', 'C', 'D', 'E', 'F'], ["C"])
        pid = lockedexecutable.start(target)
        # self.assertRaises(Exception,
        #                   PITReplication.scrubSnapshot,
        #                   target,
        #                   "C",
        #                   'scrub the other snapshot should fail')
        lockedexecutable.stop(pid)

    @nose.tools.nottest
    def failrestore_test(self):
        #        """ create a namespace
        #            copy a volume to it
        #            rename the volume
        #            take a lock on the namespace
        #            try to rename the volume again -> should fail
        #        """
        PITReplication.testConfiguration()
        ns = "failrestore-test"
        testbackend.createNamespace(ns)
        target = testbackend.theBackend()(ns)
        PITReplication.copy(self.source, target)
        PITReplication.renameVolume(target, "new_name1")
        pid = lockedexecutable.start(target)
        self.assertRaises(Exception,
                          PITReplication.renameVolume,
                          target,
                          "new_name2",
                          'rename the volume again should fail')
        lockedexecutable.stop(pid)

if __name__ == '__main__':
    unittest.main()

# Local Variables: **
# mode : python **
# python-top-dir: "../../../../../../.." **
# End: **
