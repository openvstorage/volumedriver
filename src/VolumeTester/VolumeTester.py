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
import subprocess
from volumedriver.base import env
import shutil

logger = env.getSublogger(__name__)


class VolumeTester(object):

    def _retToBool(self, ret):
        if ret == 0:
            return True
        elif ret == 1:
            return False
        else:
            raise Exception("Got exit code: " + str(ret))

    def writeToVolume(self,
                      deviceFile,
                      referenceFile,
                      strategy="random",
                      pattern=""):
        """Writes same data to the deviceFile and the referenceFile
        @param deviceFile: string, device to write to.
        @param referenceFile: string, file to write to
        @param strategy: string, random or pattern"""
        if not os.path.exists(deviceFile):
            raise Exception("writeToVolume: " + deviceFile + " does not exist")

        cmd = ["volumewriter",
               "--device", deviceFile,
               "--reference-file", referenceFile,
               "--strategy", strategy,
               "--logsink", os.path.join(env.logDir, "volumewriter.log")]

        if strategy == "pattern":
            cmd += ["--pattern", pattern]

        logger.info("Running command " + str(cmd))
        subprocess.check_call(cmd)
        # for compatibility with tests that assert_true(writeToVolume)
        return True

    def writeToVolumeRandom(self,
                            deviceFile,
                            currentFile,
                            targetFile,
                            prob=0.3,
                            strategy="random",
                            pattern=""):
        """Writes same data to the deviceFile and the referenceFile
        @param deviceFile: string, device to write to
        @param referenceFile: string, file to write to
        @param prob: float, probability to write a cluster"""
        for f in [deviceFile, currentFile]:
            if not os.path.exists(f):
                raise Exception(
                    "writeToVolumeRandom: " + f + " does not exist")

        if currentFile != targetFile:
            shutil.copy(currentFile, targetFile)

        cmd = ["volumewriter_random",
               "--device", deviceFile,
               "--reference-file", targetFile,
               "--strategy", strategy,
               "--rand", str(prob),
               "--logsink",
               os.path.join(env.logDir, "volumewriter_random.log")]

        if strategy == "pattern":
            cmd += ["--pattern", pattern]

        logger.info("Running command " + str(cmd))
        subprocess.check_call(cmd)
        # for compatibility with tests that assert_true(writeToVolume)
        return True

    def checkVolume(self, deviceFile, referenceFile):
        for f in [deviceFile, referenceFile]:
            if not os.path.exists(f):
                raise Exception("CheckVolume: " + f + " does not exist")

        # a fraction of (1 - 1/e) = 63% of the file will be read

        cmd = ["volumereader", "--full-sweep",
               "--device-file", deviceFile,
               "--reference-file", referenceFile,
               "--logsink", os.path.join(env.logDir, "volumewriter_reader.log")]
        logger.info("Running command " + str(cmd))

        return self._retToBool(subprocess.call(cmd))
