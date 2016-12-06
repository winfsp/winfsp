#!/usr/bin/python

# usage: ./munge.py ORIG/*.csv
# munge CSV files into a format that asciidocFX understands

import csv, os, sys

snames = ["ntfs", "winfsp-t0", "winfsp-t1", "winfsp-tinf", "dokany"]
file_tnames = [
    "file_create_test",
    "file_open_test",
    "file_overwrite_test",
    "file_list_test",
    "file_delete_test"]
    #"file_mkdir_test",
    #"file_rmdir_test"]
rdwr_tnames = [
    "rdwr_cc_read_page_test",
    "rdwr_cc_write_page_test",
    "rdwr_nc_read_page_test",
    "rdwr_nc_write_page_test",
    "mmap_read_test",
    "mmap_write_test"]
tnames = file_tnames + rdwr_tnames
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
                setdefault(int(row[1]), []).\
                append(float(row[2]))

if False:
    for tname in (tnames if tnames else sorted(tests.keys())):
        print "%s:" % tname
        test = tests[tname]
        for sname in (snames if snames else sorted(test.keys())):
            if sname not in test:
                continue
            print "    %s:" % sname
            series = test[sname]
            for param in sorted(series.keys()):
                print "        %s: %s -> %.2f" % (param, series[param], aggregate(series[param]))
else:
    for tname in (tnames if tnames else sorted(tests.keys())):
        with open(tname + ".csv", "w") as fout:
            test = tests[tname]
            for sname in (snames if snames else sorted(test.keys())):
                if sname not in test:
                    continue
                fout.write("//%s\r\n" % sname)
                series = test[sname]
                for param in sorted(series.keys()):
                    fout.write("%s,%.2f\r\n" % (param, aggregate(series[param])))
    def master_write(fname, tnames):
        with open(fname + ".csv", "w") as fout:
            for sname in snames:
                fout.write("//%s\r\n" % sname)
                for tname in (tnames if tnames else sorted(tests.keys())):
                    test = tests[tname]
                    if sname not in test:
                        continue
                    series = test[sname]
                    param = max(series.keys())
                    fout.write("%s,%.2f\r\n" % (tname, aggregate(series[param])))
    master_write("file_tests", file_tnames)
    master_write("rdwr_tests", rdwr_tnames)
