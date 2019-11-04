#!/usr/bin/env python

import re
import os
import sys
import json
import argparse

def main(argv):

    parser = argparse.ArgumentParser(
            description="Make a table from profiles of shopping benchmark.")
    parser.add_argument("data_dir", metavar="DD", help="Data directory.")
    arg_list = parser.parse_args(argv[1:])

    naming_pat = "perf{{TEST}}.{{EXE}}"
    ticks_pat = "{{TICK}} ticks for MMFan"

    file_naming = naming_pat.replace("{{TEST}}", "(?P<test>\d+)")
    file_naming = file_naming.replace("{{EXE}}", "(?P<exe>\S+)")
    pat_file = re.compile(file_naming)

    # list files in directory and determine available test results
    file_list = os.listdir(arg_list.data_dir)
    matched_files = list()
    files_left = list()
    test_result = list()
    exe_list = list()
    for file_name in file_list:
        resf = pat_file.search(file_name)
        if resf is not None:
            test_idx = int(resf.group("test"))
            exe_name = resf.group("exe")
            if exe_name not in exe_list:
                exe_list.append(exe_name)
            while len(test_result) - 1 < test_idx:
                test_result.append({})
            test_result[test_idx][exe_name] = { "ticks": 0 }
            matched_files.append(file_name)
        else:
            files_left.append(file_name)

    # recompile pat_file to only match exe_names seen in file naming match
    exe_str = "(" + ")|(".join(exe_list) + ")"
    file_naming = naming_pat.replace("{{TEST}}", "(?P<test>\d+)")
    file_naming = file_naming.replace("{{EXE}}", "(?P<exe>" + exe_str + ")")
    pat_file = re.compile(file_naming)
    tick_str = ticks_pat.replace("{{TICK}}", "(?P<tick>\d+)")
    pat_tick = re.compile(tick_str)

    # process the files left from the naming matching for ticks
    for file_name in files_left:
        fi = open(os.path.join(arg_list.data_dir, file_name))
        for line in fi.readlines():
            resf = pat_file.search(line)
            if resf is not None:
                test_idx = int(resf.group("test"))
                exe_name = resf.group("exe")
            rest = pat_tick.search(line)
            if rest is not None:
                ticks = int(rest.group("tick"))
                test_result[test_idx][exe_name]["ticks"] = ticks
        fi.close()

    # process the files getting name matched to get perf events statistics
    pat_perf = re.compile("\s+(?P<val>\d{1,3}(?:,\d{3})*)\s+(?P<evt>\S+)\s")
    evt_list = ["ticks"]
    for test_idx in range(len(test_result)):
        for exe_name in test_result[test_idx].keys():
            file_name = naming_pat.replace("{{TEST}}", str(test_idx))
            file_name = file_name.replace("{{EXE}}", exe_name)
            fi = open(os.path.join(arg_list.data_dir, file_name))
            for line in fi.readlines():
                resp = pat_perf.match(line)
                if resp is not None:
                    perf_val = int(resp.group("val").replace(",", ""))
                    perf_evt = resp.group("evt")
                    if perf_evt not in evt_list:
                        evt_list.append(perf_evt)
                    test_result[test_idx][exe_name][perf_evt] = perf_val
            fi.close()

    # print a table for ticks and all perf events
    tests_str = ""
    for i in range(len(test_result)):
        tests_str = tests_str + "\t1 + " + str(int(2**(i-1)))
    print("#threads (producer + consumer)\t %s" % tests_str)
    for evt in evt_list:
        for exe in exe_list:
            row_str = evt + "\t" + exe
            for i in range(len(test_result)):
                if evt in test_result[i][exe].keys():
                    row_str = row_str + "\t" + str(test_result[i][exe][evt])
                else:
                    row_str = row_str + "\t" + "N\A"
            print(row_str)

if "__main__" == __name__:
    main(sys.argv)
