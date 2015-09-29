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

import volumedriver.pitreplication.PITReplication as B # a bit verbose...
from volumedriver.toolcut import backend as Backend
from volumedriver.toolcut import ToolCut

import unittest
import os

import volumedrivertest.voldrv.testbackend as testbackend
import nose
from volumedriver.base import testutils
from volumedriver.base import env
from volumedrivertest.voldrv.backupWithMonitoring import backupWithMonitoring

VOLNAME = "5c17a9f4f16348b8bbc"

THISDIR = os.path.dirname(os.path.abspath(__file__))

logger = env.getSublogger(__name__)


class Volumeconfig_upgrade_test(testutils.TestWithDir):

    def __init__(self, methodName):
        testutils.TestWithDir.__init__(self,
                                       methodName,
                                       os.path.join(env.tmpDir,
                                                    "Volumeconfig_upgrade_test"))

    @nose.tools.nottest
    def test_upgradeFrom3(self):
        source = Backend.LocalBackend(
            os.path.join(THISDIR, "data"))(VOLNAME + "-version3")
        self.makeDirs("target")
        target = Backend.LocalBackend(self.getDir())("target")

        self.assertEqual(B.getVolumeInfo(source)["volume_role"],
                         ToolCut.WanBackupVolumeRole.Normal)
        B.backup(source, target)

    @nose.tools.nottest
    def test_failToUpgradeFrom4(self):
        source = Backend.LocalBackend(
            os.path.join(THISDIR, "data"))(VOLNAME + "-version4")
        self.makeDirs("target")
        target = Backend.LocalBackend(self.getDir())("target")
        self.assertRaises(Exception, B.getVolumeInfo, source)
        self.assertRaises(Exception, B.backup, source, target)


@nose.tools.nottest
def testTool(target):
    sp = B.getVolumeSnapshotsTool(target)
    count = 0
    for s in sp.getSnapshots():
        logger.info(s.name())
        logger.info(s.stored())
        logger.info(s.date())
        nose.tools.eq_(s.stored(),
                       sp.snapshotStored(s.name()))
        count += s.stored()
    count += sp.currentStored()
    nose.tools.eq_(sp.stored(), count)


class Backup_test(testutils.TestWithDir):

    def __init__(self, methodName):
        testutils.TestWithDir.__init__(self,
                                       methodName,
                                       os.path.join(env.tmpDir,
                                                    "backup_test"))

    def setUp(self):
        testutils.TestWithDir.setUp(self)
        testbackend.setUp()

        self.makeDirs("scocache")
        self.source = Backend.LocalBackend(
            os.path.join(THISDIR, "data"))(VOLNAME)

    def tearDown(self):
        testbackend.tearDown()
        testutils.TestWithDir.tearDown(self)

    def test_Config(self):
        p2 = Backend.LocalBackend(self.pathTo("backup1"))("somenamespace")
        _ = B.prepareEnvironment("jobID",
                                 os.path.join(env.tmpDir, "whatever"),
                                 self.source.getJSONDict(),
                                 p2.getJSONDict(),
                                 endSnapshot="C",
                                 startSnapshot="A")

    @nose.tools.nottest
    def test_Basic(self):
        B.testConfiguration()
        testbackend.createNamespace("basebackup")
        target = testbackend.theBackend()("basebackup")

        backupWithMonitoring(self, self.source, target, endSnapshot="A")
        backupWithMonitoring(self, self.source, target, endSnapshot="C")
        backupWithMonitoring(self, self.source, target, endSnapshot="E")
        backupWithMonitoring(self, self.source, target)

    @nose.tools.nottest
    def test_OutOfOrder(self):
        for ns in ["incremental", "final", "usb"]:
            testbackend.createNamespace(ns)

        backupWithMonitoring(self,
                             self.source,
                             testbackend.theBackend()("usb"),
                             endSnapshot="C")
        backupWithMonitoring(self,
                             self.source,
                             testbackend.theBackend()("incremental"),
                             startSnapshot="C")
        B.copy(testbackend.theBackend()("usb"),
               testbackend.theBackend()("final"))
        backupWithMonitoring(self,
                             testbackend.theBackend()("incremental"),
                             testbackend.theBackend()("final"))

    @nose.tools.nottest
    def test_cheapInitialIncremental(self):
        testbackend.createNamespace("foo")
        backupWithMonitoring(self,
                             self.source,
                             testbackend.theBackend()("foo"),
                             startSnapshot="D",
                             endSnapshot="D")

    @nose.tools.nottest
    def test_Idempotency(self):
        testbackend.createNamespace("foo")
        target = testbackend.theBackend()("foo")
        backupWithMonitoring(self, self.source, target)
        backupWithMonitoring(self, self.source, target)

    @nose.tools.nottest
    def test_restore(self):
        ns1 = "xxx"
        ns2 = "yyy"
        for ns in [ns1, ns2]:
            testbackend.createNamespace(ns)

        self.makeDirs("DC2", ns2)

        DC1 = testbackend.theBackend()
        DC2 = Backend.LocalBackend(self.pathTo("DC2"))

        B.copy(self.source, DC1(ns1))
        B.copy(DC1(ns1), DC1(ns2))
        B.copy(DC1(ns2), DC2(ns2))

    @nose.tools.nottest
    def test_rename(self):
        testbackend.createNamespace("xxx")

        target = testbackend.theBackend()("xxx")
        B.copy(self.source, target)
        B.renameVolume(target, "new_name")

    @nose.tools.nottest
    def test_VolumeRoles(self):
        for ns in ["base", "inc"]:
            testbackend.createNamespace(ns)

        self.assertEqual(B.getVolumeInfo(self.source)["volume_role"],
                         ToolCut.WanBackupVolumeRole.Normal)

        for s in ["B", "C", "D"]:
            backupWithMonitoring(self,
                                 self.source,
                                 testbackend.theBackend()("base"),
                                 endSnapshot=s)
            self.assertEqual(
                B.getVolumeInfo(
                    testbackend.theBackend()("base"))["volume_role"],
                ToolCut.WanBackupVolumeRole.BackupBase)

        self.assertEqual(
            B.getVolumeSnapshots(testbackend.theBackend()("base")),
            ["B", "C", "D"])

        backupWithMonitoring(self,
                             self.source,
                             testbackend.theBackend()("inc"),
                             startSnapshot="B",
                             endSnapshot="C")
        self.assertEqual(
            B.getVolumeInfo(testbackend.theBackend()("inc"))["volume_role"],
            ToolCut.WanBackupVolumeRole.BackupIncremental)
        self.assertEqual(B.getVolumeSnapshots(testbackend.theBackend()("inc")),
                         ["B", "C"])

        backupWithMonitoring(self,
                             self.source,
                             testbackend.theBackend()("inc"))
        self.assertEqual(
            B.getVolumeInfo(testbackend.theBackend()("inc"))["volume_role"],
            ToolCut.WanBackupVolumeRole.BackupIncremental)
        self.assertEqual(B.getVolumeSnapshots(testbackend.theBackend()("inc")),
                         ["B", "C", "F"])

        self.assertRaises(Exception,
                          B.promote, testbackend.theBackend()("inc"))

        B.promote(testbackend.theBackend()("base"))
        self.assertEqual(
            B.getVolumeInfo(testbackend.theBackend()("base"))["volume_role"],
            ToolCut.WanBackupVolumeRole.Normal)

        self.makeDirs("copy")
        cpy = Backend.LocalBackend(self.getDir())("copy")
        B.copy(self.source, cpy)
        self.assertEqual(B.getVolumeInfo(cpy)["volume_role"],
                         ToolCut.WanBackupVolumeRole.Normal)

    @nose.tools.nottest
    def test_ScrubbingAndSnapshotMgmtTarget(self):
        testbackend.createNamespace("toscrub")
        target = testbackend.theBackend()("toscrub")

        # def check(allss, toscrubss):
        #     self.assertEqual(B.getVolumeSnapshots(target), allss)
        #     self.assertEqual(B.getScrubSnapshots(target), toscrubss)

        for ss in B.getVolumeSnapshots(self.source):
            backupWithMonitoring(self, self.source, target, endSnapshot=ss)

        # check(['A', 'B', 'C', 'D', 'E', 'F'], [])
        # B.deleteSnapshots(target, ["B", "C" , "E"])
        # check(['A', 'D', 'F'], ['D', 'F'])
        # B.scrubSnapshot(target, "D")
        # check(['A', 'D', 'F'], ['F'])
        # B.deleteSnapshots(target, ["A"])
        # check(['D', 'F'], ['D', 'F'])
        # B.scrubSnapshot(target, "F")
        # check(['D', 'F'], ['D'])
        # B.scrubSnapshot(target, "D")
        # check(['D', 'F'], [])
    @nose.tools.nottest
    def test_Wrongtype(self):
        wrongTarget = Backend.DSSBackend()("whatevernamespace")

        self.assertRaises(
            B.BackendTypeError, B.backup, self.source, wrongTarget)
        self.assertRaises(B.BackendTypeError, B.copy, self.source, wrongTarget)
        self.assertRaises(
            B.BackendTypeError, B.deleteSnapshots, wrongTarget, [])
        self.assertRaises(
            B.BackendTypeError, B.getVolumeSnapshots, wrongTarget)
        self.assertRaises(B.BackendTypeError, B.getScrubSnapshots, wrongTarget)
        self.assertRaises(
            B.BackendTypeError, B.renameVolume, wrongTarget, "newname")
        self.assertRaises(B.BackendTypeError, B.promote, wrongTarget)
# self.assertRaises(B.BackendTypeError, B.scrubSnapshot, wrongTarget,
# "somesnap")

    @nose.tools.nottest
    def test_snapshottool(self):
        testTool(Backend.LocalBackend(os.path.join(THISDIR, "data"))(VOLNAME))
        testTool(Backend.LocalBackend(
            os.path.join(THISDIR, "data"))(VOLNAME + "-version3"))
        testTool(Backend.LocalBackend(
            os.path.join(THISDIR, "data"))(VOLNAME + "-version4"))

    @nose.tools.nottest
    def test_Monitoring(self):
        B.testConfiguration()
        ns = "monitortest"
        testbackend.createNamespace(ns)
        target = testbackend.theBackend()(ns)

        self.assertEqual(B.getProgressInfo(target), None)
        backupWithMonitoring(self, self.source, target)

        info = B.getProgressInfo(target)
        self.assertTrue(type(info), dict)
        self.assertEqual(info["total_size"], info["seen"])
        self.assertEqual(info["still_to_be_examined"], 0)
        self.assertTrue(info["sent_to_backend"] <= info["total_size"])

if __name__ == '__main__':
    unittest.main()

# Local Variables: **
# mode : python **
# python-top-dir: "../../../../../../.." **
# End: **
