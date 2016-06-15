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

import unittest
from volumedriver.base import auxiliary
from volumedriver.toolcut import ToolCut
import json

json_str = """
{
"backend_connection_type" : "BUCHLA",
"buchla_connection_host" : "1.2.3.4",
"buchla_connection_port" : 123456
}
"""


class UnicodeTest(unittest.TestCase):

    def test_unicode(self):
        dct = json.loads(json_str, object_hook=auxiliary.noUnicode)
        ToolCut.BackendInterface(auxiliary.mapDict(str, dct), "bla")

# Local Variables: **
# mode : python **
# python-top-dir: "../../../../../../.." **
# End: **
