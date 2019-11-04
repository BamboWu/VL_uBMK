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
    parser.add_argument("-n", "--naming", dest="perfout_naming",
            default="perf{{TEST}}.{{EXE}}",
            help="How perf output files are named.")
    parser.add_argument("-t", "--tick", dest="tick_pattern",
            default="{{TICK}} ticks for MMFan",
            help="The pattern to catch the value of ticks.")
    arg_list = parser.parse_args(argv[1:])

    #perfout_naming = "perf{{TEST}}.{{EXE}}" # how perf output files are named
    #ticks_pat = "{{TICK}} ticks for MMFan"

    file_naming = arg_list.perfout_naming.replace("{{TEST}}", "(?P<test>\d+)")
    file_naming = file_naming.replace("{{EXE}}", "(?P<exe>\S+)")
    pat_file = re.compile(file_naming)

    # list files in directory and determine available test results
    all_files = os.listdir(arg_list.data_dir)
    log_files = all_files[:]
    res_list = list() # results listed per test
    exe_list = list() # all exe_name caught in perf file naming match
    for file_name in all_files:
        resf = pat_file.match(file_name)
        if resf is not None:
            test_idx = int(resf.group("test"))
            exe_name = resf.group("exe")
            while len(res_list) - 1 < test_idx: # expand the list
                res_list.append({})
            res_list[test_idx][exe_name] = { "ticks": 0 }
            if exe_name not in exe_list: # expand the list
                exe_list.append(exe_name)
            log_files.remove(file_name) # match perf output naming won't be log

    # recompile pat_file to only match exe_names seen in file naming match
    exe_or_str = "(" + ")|(".join(exe_list) + ")"
    file_naming = arg_list.perfout_naming.replace("{{TEST}}", "(?P<test>\d+)")
    file_naming = file_naming.replace("{{EXE}}", "(?P<exe>" + exe_or_str + ")")
    pat_file = re.compile(file_naming)
    tick_str = arg_list.tick_pattern.replace("{{TICK}}", "(?P<tick>\d+)")
    pat_tick = re.compile(tick_str)

    # process the files left from the naming matching for ticks
    for file_name in log_files:
        fi = open(os.path.join(arg_list.data_dir, file_name))
        for line in fi.readlines():
            resf = pat_file.search(line)
            if resf is not None:
                test_idx = int(resf.group("test"))
                exe_name = resf.group("exe")
            rest = pat_tick.search(line)
            if rest is not None:
                res_list[test_idx][exe_name]["ticks"] = int(rest.group("tick"))
        fi.close()

    # process the files getting name matched to get perf events statistics
    pat_perf = re.compile("\s+(?P<val>\d{1,3}(?:,\d{3})*)\s+(?P<evt>\S+)\s")
    evt_list = ["ticks"]
    for test_idx in range(len(res_list)):
        test_name = arg_list.perfout_naming.replace("{{TEST}}", str(test_idx))
        for exe_name in res_list[test_idx].keys():
            file_name = test_name.replace("{{EXE}}", exe_name)
            fi = open(os.path.join(arg_list.data_dir, file_name))
            for line in fi.readlines():
                resp = pat_perf.match(line)
                if resp is not None:
                    perf_val = int(resp.group("val").replace(",", ""))
                    perf_evt = resp.group("evt")
                    res_list[test_idx][exe_name][perf_evt] = perf_val
                    if perf_evt not in evt_list:
                        evt_list.append(perf_evt)
            fi.close()

    # print a table for ticks and all perf events, each test per column
    row_str = "\033[1m#threads (producer + consumer) \t"
    for i in range(len(res_list)):
        row_str += "\t1 + " + str(int(2**(i-1)))
    print(row_str + "\033[0m")
    for evt in evt_list:
        evt_str = evt
        for exe in exe_list:
            row_str = "\033[1m%-23s\t%-15s\033[0m" % (evt_str, exe)
            evt_str = ""
            for i in range(len(res_list)):
                if evt in res_list[i][exe].keys():
                    row_str += "\t" + str(res_list[i][exe][evt])
                else:
                    row_str += "\t" + "N\A"
            print(row_str)

if "__main__" == __name__:
    main(sys.argv)
