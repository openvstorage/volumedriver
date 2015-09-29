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
import volumedriver.toolcut.ToolCut as T
import volumedriver.toolcut.backend as Backend
from volumedriver.base import env
import nose


THISDIR = os.path.dirname(os.path.abspath(__file__))
VOLNAME = "5c17a9f4f16348b8bbc"

logger = env.getSublogger(__name__)


@nose.tools.nottest
def testSP(target):
    sp = T.SnapshotPersistor(target.params(), target.namespace())
    count = 0
    for s in sp.getSnapshots():
        logger.info(s.name())
        logger.info(s.stored())
        logger.info(s.date())
        nose.tools.eq_(s.stored(), sp.snapshotStored(s.name()))
        count += s.stored()
    count += sp.currentStored()
    nose.tools.eq_(sp.stored(), count)


@nose.tools.nottest
def testSnapshotPersistorToolCut():
    testSP(Backend.LocalBackend(os.path.join(THISDIR, "data"))(VOLNAME))
    testSP(Backend.LocalBackend(os.path.join(THISDIR, "data"))
           (VOLNAME + "-version3"))
    testSP(Backend.LocalBackend(os.path.join(THISDIR, "data"))
           (VOLNAME + "-version4"))
