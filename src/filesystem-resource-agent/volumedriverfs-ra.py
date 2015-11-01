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

#!/usr/bin/env python

import os
import sys
import volumedriver.storagerouter.storagerouterclient as S
import xml.dom.minidom as M

# References:
# http://www.opencf.org/cgi-bin/viewcvs.cgi/specs/ra/resource-agent-api.txt?rev=HEAD
# http://linux-ha.org/doc/dev-guides/ra-dev-guide.html

OCF_SUCCESS = 0
OCF_ERR_GENERIC = 1
OCF_ERR_ARGS = 2
OCF_ERR_UNIMPLEMENTED = 3
OCF_ERR_PERM = 4
OCF_ERR_INSTALLED = 5
OCF_ERR_CONFIGURED = 6
OCF_NOT_RUNNING = 7
OCF_RUNNING_MASTER = 8
OCF_FAILED_MASTER = 9

ACT_START = "start"
ACT_STOP = "stop"
ACT_MONITOR = "monitor"
ACT_METADATA = "meta-data"

XML_NODE_ID_KEY = "vrouter_id"

RESKEY_PFX = "OCF_RESKEY_"
NODE_ID_KEY = "node_id"
VOLUME_PATH_KEY = "volume_path"
CONFIG_FILE_KEY = "config_file"

def getenv(key):
    val = os.getenv(key)
    if val == None:
        raise Exception("key %s not found in environment" % key)
    return val

def get_config():
    return getenv(RESKEY_PFX + CONFIG_FILE_KEY)

def get_volume_path():
    return getenv(RESKEY_PFX + VOLUME_PATH_KEY)

def get_node_id():
    return getenv(RESKEY_PFX + NODE_ID_KEY)

def start():
    client = S.LocalStorageRouterClient(get_config())
    maybe_id = client.get_volume_id(get_volume_path())
    if maybe_id == None:
        return OCF_ERR_GENERIC
    client.migrate_volume(maybe_id, get_node_id())
    return OCF_SUCCESS

def stop():
    client = S.LocalStorageRouterClient(get_config())
    maybe_id = client.get_volume_id(get_volume_path())
    if maybe_id == None:
        return OCF_ERR_GENERIC
    client.stop_volume(maybe_id)
    return OCF_SUCCESS

def monitor():
    client = S.LocalStorageRouterClient(get_config())
    maybe_id = client.get_volume_id(get_volume_path())
    if maybe_id == None:
        return OCF_NOT_RUNNING
    info = client.info_volume(maybe_id)
    doc = M.parseString(str(info))
    node_id = str(doc.getElementsByTagName(XML_NODE_ID_KEY)[0].firstChild.nodeValue)
    if node_id == None:
        return OCF_ERR_GENERIC
    elif node_id == get_node_id:
        return OCF_SUCCESS
    else:
        return OCF_NOT_RUNNING

def metadata():
    mdata = '<?xml version="1.0\?>\n' + \
        '<!DOCTYPE resource-agent SYSTEM "ra-api-1.dtd">\n' + \
        '<resource-agent name="volumedriverfs" version="0.1">\n' + \
        '<version>0.1</version>\n' + \
        '<longdesc lang="en">Resource agent for volumedriverfs volumes.</longdesc>\n' + \
        '<shortdesc lang="en">Resource agent for volumedriverfs volumes.</shortdesc>\n' + \
        '<parameters>\n' + \
        '<parameter name="%s" unique="1" required="1">\n' % VOLUME_PATH_KEY + \
        '<longdesc lang="en">Volume path</longdesc>\n' + \
        '<shortdesc lang="en">Volume path</shortdesc>\n' + \
        '<content type="string"/>\n' + \
        '</parameter>\n' + \
        '<parameter name="%s" unique="1" required="1">\n' % NODE_ID_KEY + \
        '<longdesc lang="en">Node Identifier</longdesc>\n' + \
        '<shortdesc lang="en">Node Identifier</shortdesc>\n' + \
        '<content type="string"/>\n' + \
        '</parameter>\n' + \
        '<parameter name="%s" unique="0" required="1">\n' % CONFIG_FILE_KEY + \
        '<longdesc lang="en">Path to the config file used by LocalStorageClient</longdesc>\n' + \
        '<shortdesc lang="en">Path to the config file used by LocalStorageClient</shortdesc>\n' + \
        '<content type="string" default="false"/>\n' + \
        '</parameter>\n' + \
        '</parameters>\n' + \
        '<actions>\n' + \
        '<action name="start"        timeout="300" />\n' + \
        '<action name="stop"         timeout="300" />\n' + \
        '<action name="monitor"      timeout="20" interval="10" depth="0" />\n' + \
        '<action name="meta-data"    timeout="5" />\n' + \
        '</actions>\n' + \
        '</resource-agent>'
    print mdata
    return OCF_SUCCESS

def help():
    return "Usage: " + sys.argv[0] + " (start|stop|monitor|meta_data)"

def main():
    if len(sys.argv) != 2:
        print >> sys.stderr, ("Missing action.\n" + help());
        return OCF_ERR_ARGS

    action = sys.argv[1]

    try:
        if action == ACT_START:
            return start()
        elif action == ACT_STOP:
            return stop()
        elif action == ACT_MONITOR:
            return monitor()
        elif action == ACT_METADATA:
            return metadata()
        else:
            return OCF_ERR_UNIMPLEMENTED
    except Exception, e:
        print >> sys.stderr, (action + " failed: " + str(e))
        return OCF_ERR_GENERIC

if __name__ == "__main__":
    exit(main())
