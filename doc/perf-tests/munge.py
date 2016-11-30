#!/usr/bin/python

# usage: ./munge.py ORIG/*.csv
# munge CSV files into a format that asciidocFX understands

import csv, os, sys

snames = ["ntfs", "winfsp-t0", "winfsp-t1", "winfsp-inf", "dokany"]
aggregate = min

tests = {}
for arg in sys.argv[1:]:
    name = os.path.splitext(os.path.basename(arg))[0]
    if name[-1].isdigit() and name[-2] == '-':
        name = name[:-2]
    with open(arg, "r") as fin:
        for row in csv.reader(fin):
            tests.\
                setdefault(row[0], {}).\
                setdefault(name, {}).\
                setdefault(row[1], []).\
                append(row[2])

if False:
    for testname in sorted(tests.keys()):
        print "%s:" % testname
        test = tests[testname]
        for sname in (snames if snames else sorted(test.keys())):
            if sname not in test:
                continue
            print "    %s:" % sname
            series = test[sname]
            for param in sorted(series.keys()):
                print "        %s: %s -> %s" % (param, series[param], aggregate(series[param]))
else:
    for testname in sorted(tests.keys()):
        with open(testname + ".csv", "w") as fout:
            test = tests[testname]
            for sname in (snames if snames else sorted(test.keys())):
                if sname not in test:
                    continue
                fout.write("//%s\r\n" % sname)
                series = test[sname]
                for param in sorted(series.keys()):
                    fout.write("%s,%s\r\n" % (param, aggregate(series[param])))
