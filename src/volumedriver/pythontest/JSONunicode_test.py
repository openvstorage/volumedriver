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
