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
