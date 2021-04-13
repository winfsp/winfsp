#!/usr/bin/python

import re, sys, uuid

guid_re = re.compile("[0-9A-Fa-f]{8}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{12}")
guid_dt = {}
def mk_guid(m):
    guid = m.group(0).upper()
    if guid not in guid_dt:
        guid_dt[guid] = str(uuid.uuid4()).upper()
    return guid_dt[guid]

with open(sys.argv[1]) as file:
    text = file.read()
text = guid_re.sub(mk_guid, text)
sys.stdout.write(text)
