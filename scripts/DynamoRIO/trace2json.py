#!/usr/bin/env python3

import sys
import argparse
import json
import re
import pandas as pd

def main(argv):
    parser = argparse.ArgumentParser(
            description="Process libut scheduling log")
    parser.add_argument("sch_json", metavar="SJ", help="scheduling json")
    parser.add_argument("-o", "--out", dest="foname", default="test",
            help="output file base name")
    arg_list = parser.parse_args(argv[1:])

    #spawn_pat = re.compile(
    #        "(?P<time>\d+\.\d+)\] CPU (?P<cpu_id>\d+)\| " +
    #        "<1> (?P<tid>\d+) 0x(?P<addr>[0-9a-f]+)")
    #finish_pat = re.compile(
    #        "(?P<time>\d+\.\d+)\] CPU (?P<cpu_id>\d+)\| " +
    #        "<1> finish 0x(?P<addr>[0-9a-f]+)")
    #sch_pat = re.compile(
    #        "(?P<time>\d+\.\d+)\] CPU (?P<cpu_id>\d+)\| " +
    #        "<1> (?P<sch>jmp|jd|yield|park) 0x(?P<addr>[0-9a-f]+)")
    #mq_pat = re.compile(
    #        "(?P<time>\d+\.\d+)\] CPU (?P<cpu_id>\d+)\| " +
    #        "<1> (?P<mq>peeked|pushing|pushed|processed) 0x(?P<addr>[0-9a-f]+)")

    sch_json = json.load(open(arg_list.sch_json))

    cpu_record = {
            2: [
                   {
                       "time": 0,
                       "evt": "spawn",
                       "tid": 0
                   }
               ]
            }
    task_record = {
            0: [
                   {
                       "time": 0,
                       "evt": "spawn",
                       "cpu": 2
                   }
               ]
            }

    for csv_f in sch_json["csvs"]:
        cpu_id = int(csv_f[0])
        if cpu_id not in cpu_record.keys():
            cpu_record[cpu_id] = []
        fi = open(csv_f[1])
        fi.readline()
        for line in fi.readlines():
            alist = line[:-1].split(',')
            evt_tmp = sch_json["pc_map"][alist[0]]
            time_tmp = int(alist[1])
            tid_tmp = sch_json["task_map"][alist[2]] if \
                    alist[2] in sch_json["task_map"].keys() else 0
            if tid_tmp not in task_record.keys():
                task_record[tid_tmp] = []
            task_record[tid_tmp].append(
                        {
                           "time": time_tmp,
                           "evt": evt_tmp,
                           "cpu": cpu_id
                        })
            cpu_record[cpu_id].append({
                    "time": time_tmp,
                    "evt": evt_tmp,
                    "tid": tid_tmp
                    })
        fi.close()

    fo = open(arg_list.foname + ".json", "w")
    json.dump({"cpu": cpu_record, "task": task_record}, fo,
            sort_keys=True, indent=2)
    fo.close()

if "__main__" == __name__:
    main(sys.argv)
