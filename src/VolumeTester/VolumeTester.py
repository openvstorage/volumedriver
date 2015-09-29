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
               "--logfile", os.path.join(env.logDir, "volumewriter.log")]

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
               "--logfile",
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
               "--logfile", os.path.join(env.logDir, "volumewriter_reader.log")]
        logger.info("Running command " + str(cmd))

        return self._retToBool(subprocess.call(cmd))
