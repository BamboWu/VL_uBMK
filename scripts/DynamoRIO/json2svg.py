#!/usr/bin/env python

import matplotlib.pyplot as plt
import matplotlib.ticker as ticker
import matplotlib.patches as patches
from matplotlib.cm import Dark2 as cm_Dark2
from matplotlib.cm import Set1 as cm_Set1
import xml.etree.ElementTree as ET
from io import BytesIO
import json
import sys
import argparse

def to_hex(x, pos):
    return '%x' % int(x)

svg_script = """
<script type="text/ecmascript">
<![CDATA[
var patch_groups = {patch_groups};
var patch_groups2 = {patch_groups2};
var opa_on = 1;
var opa_off = 0.2;

var curr_beg_x = 0;
var curr_end_x = 0;

function zoom_in(beg_x, end_x) {{
    var canvas_obj = document.getElementById("patch_2");
    var width_ratio = canvas_obj.getBBox()["width"] / (end_x - beg_x);
    var canvas_beg_x = canvas_obj.getBBox()["x"];
    //transform translate(canvas_beg_x 0) translate(-beg_x * width_ratio 0) scale(width_ratio 1) translate(-canvas_beg_x 0)
    var blk_transform_str = "translate(" + canvas_beg_x + " 0) translate(-" + (beg_x * width_ratio) + " 0) scale(" + width_ratio + " 1) translate(-" + canvas_beg_x + " 0)";
    for (evt_blk of document.getElementsByClassName("evt_blk")) {{
        evt_blk.setAttribute("transform", blk_transform_str);
    }};
    for (anno_lbl of document.getElementsByClassName("anno_lbl")) {{
        var lbl_bbox = anno_lbl.getBBox();
        var lbl_center_x = (lbl_bbox["x"] + lbl_bbox["width"] / 2) - canvas_beg_x;
        var new_center_x = (lbl_center_x - beg_x) * width_ratio;
        var lbl_transform_str = "translate(" + (new_center_x - lbl_center_x) + " 0)";
        anno_lbl.setAttribute("transform", lbl_transform_str);
    }};
    var x_ticks = document.getElementById("matplotlib.axis_1");
    for (x_tick of x_ticks.children) {{
        var tik_bbox = x_tick.getBBox();
        var tik_center_x = (tik_bbox["x"] + tik_bbox["width"] / 2) - canvas_beg_x;
        var new_center_x = (tik_center_x - beg_x) * width_ratio;
        var tik_transform_str = "translate(" + (new_center_x - tik_center_x) + " 0)";
        x_tick.setAttribute("transform", tik_transform_str);
    }};
}}

function zoom_reset() {{
    for (evt_blk of document.getElementsByClassName("evt_blk")) {{
        evt_blk.setAttribute("transform", "");
    }};
    for (anno_lbl of document.getElementsByClassName("anno_lbl")) {{
        anno_lbl.setAttribute("transform", "");
    }};
    var x_ticks = document.getElementById("matplotlib.axis_1");
    for (x_tick of x_ticks.children) {{
        x_tick.setAttribute("transform", "");
    }};
    curr_beg_x = 0;
    curr_end_x = document.getElementById("patch_2").getBBox()["width"];
}}

var down_x = 0;

function mouseMove(evt) {{
    console.log(evt.clientX + " " + evt.clientY);
}}

function mouseDown(evt) {{
    down_x = evt.clientX;
}}

function mouseUp(evt) {{
    var up_x = evt.clientX;
    if (up_x < down_x && 5 < (down_x - up_x)) {{
        zoom_reset();
        return;
        //var tmp = up_x;
        //up_x = down_x;
        //down_x = tmp;
    }}
    if (5 > (up_x - down_x)) {{
        return;
    }}

    // convert to svg coordination
    var svg_obj = document.getElementsByTagName("svg")[0];
    var svg_coo_pt = svg_obj.createSVGPoint();
    svg_coo_pt.x = down_x;
    down_x = svg_coo_pt.matrixTransform(svg_obj.getScreenCTM().inverse()).x
    svg_coo_pt.x = up_x;
    up_x = svg_coo_pt.matrixTransform(svg_obj.getScreenCTM().inverse()).x

    var canvas_obj = document.getElementById("patch_2");
    var width_ratio = canvas_obj.getBBox()["width"] / (curr_end_x - curr_beg_x);
    down_x -= canvas_obj.getBBox()["x"]
    up_x -= canvas_obj.getBBox()["x"]
    console.log(down_x + " " + up_x);
    var zoom_beg_x = curr_beg_x + (down_x - curr_beg_x) / width_ratio;
    var zoom_end_x = curr_beg_x + (up_x - curr_beg_x) / width_ratio;
    zoom_in(zoom_beg_x, zoom_end_x);
    curr_beg_x = zoom_beg_x;
    curr_end_x = zoom_end_x;
}}

document.addEventListener("mousedown", mouseDown, false);
document.addEventListener("mouseup", mouseUp, false);
//document.addEventListener("mousemove", mouseMove, false);
window.addEventListener("load", (event) => {{
    curr_end_x = document.getElementById("patch_2").getBBox()["width"];
}});

function toggle(oid, attribute, values) {{
    /* Toggle the style attribute of an object between two values.

    Parameters
    ----------
    oid : str
      Object identifier.
    attribute : str
      Name of style attribute.
    values : [on state, off state]
      The two values that are switched between.
    */

    var obj = document.getElementById(oid);
    var a = obj.style[attribute];

    a = (a == values[0] || a == "") ? values[1] : values[0];
    obj.style[attribute] = a;
    }}

function toggle_tran(obj) {{

    var fields = obj.id.split('.');
    var id_str = fields[0];

    toggle(id_str, 'opacity', [opa_on,opa_off]);

    }}

function turn_all(is_on) {{
    for ([id_str, patch_ids] of Object.entries(patch_groups)) {{
        var obj = document.getElementById(id_str);
        if (is_on) {{
            obj.style['opacity'] = opa_on;
        }} else {{
            obj.style['opacity'] = opa_off;
        }}
    }};
    for ([id_str, patch_ids] of Object.entries(patch_groups2)) {{
        var obj = document.getElementById(id_str);
        if (is_on) {{
            obj.style['opacity'] = opa_on;
        }} else {{
            obj.style['opacity'] = opa_off;
        }}
    }};
    }}
]]>
</script>
"""


def main(argv):

    parser = argparse.ArgumentParser(
            description="Visualize scheduling")
    parser.add_argument("sch_json", metavar="SJ",
            help="scheduling json file")
    parser.add_argument("--vsize", dest="vsize", default=9, type=float,
            help="figure vertical size")
    parser.add_argument("--hsize", dest="hsize", default=32, type=float,
            help="figure horizental size")
    parser.add_argument("--xmin", dest="xmin", type=int,
            help="figure x min (may clip)")
    parser.add_argument("--xmax", dest="xmax", type=int,
            help="figure x max (may clip)")
    parser.add_argument("-b", "--beg", dest="beg", default=0, type=int,
            help="beginning time of the ROI")
    parser.add_argument("-e", "--end", dest="end", default=0, type=int,
            help="end time of the ROI")
    parser.add_argument("-c", "--cpus", dest="cpus", default=[], type=int,
            nargs="*", help="list of cpu ids of the ROI")
    parser.add_argument("-o", "--out", dest="foname", default="tmp",
            help="output file base name")
    arg_list = parser.parse_args(argv[1:])
    roi = arg_list.end > arg_list.beg
    cpu_roi = len(arg_list.cpus) > 0

    with open(arg_list.sch_json) as fi:
        sch_json = json.load(fi)

    toffset = 0
    if "time_offset" in sch_json.keys():
        toffset = sch_json["time_offset"]

    cpu_voffset_sep = 2
    cpu_vspan = 1.6
    cpu_voffset_ann = 0.7
    cpu_voffset = []
    cpu_id_lbl = []
    cpu_lbl2voffset = {}

    color_table = cm_Dark2.colors + cm_Set1.colors
    #color_table = ["#FFFAC8", "#FFE119", "#F58231", "#FFD8B1",
    #               "#FABEBE", "#E6BEFF", "#BFEF45", "#F032E6",
    #               "#E6194B", "#A9A9A9", "#911EB4", "#9A6324",
    #               "#42D4F4", "#AAFFC3", "#800000", "#4363DB",
    #               "#3CB44B", "#808000", "#469990", "#000075",
    #               "#000000"]

    plt.rcParams['svg.fonttype'] = 'none'
    plt.rcParams['lines.linewidth'] = 1
    #plt.rcParams["font.size"] = 18
    plt.rc('xtick', labelsize=18)
    plt.rc('ytick', labelsize=18)
    plt.rcParams["font.weight"] = "bold"
    plt.figure(figsize=(arg_list.hsize, arg_list.vsize))
    patch_groups = {}

    for cpu_id in sorted([int(x) for x in sch_json["cpu"].keys()]):
        if cpu_roi and cpu_id not in arg_list.cpus:
            continue

        cpu_id_str = str(cpu_id)

        if 0 == len(cpu_voffset):
            cpu_voffset.append(1)
        else:
            cpu_voffset.append(cpu_voffset[-1] + cpu_voffset_sep)
        cpu_id_lbl.append(cpu_id)
        cpu_lbl2voffset[cpu_id] = cpu_voffset[-1]

        last_occupied = -1
        last_tid = -1
        for evt in sch_json["cpu"][cpu_id_str]:
            draw_block = False
            period_beg = last_occupied
            period_end = evt["time"] - toffset
            period_tid = last_tid
            last_occupied_bak = last_occupied
            id_str = "{}_{}_{}_{}".format(cpu_id, last_tid,
                                          period_beg, period_end)
            if roi and last_occupied < arg_list.beg:
                period_beg = arg_list.beg

            if evt["evt"] in ["jmp"]:
                last_occupied = period_end
                last_tid = evt["tid"]
            elif evt["evt"] in ["jd"]:
                last_occupied = period_end
                last_tid = evt["tid"]
                draw_block = True
            elif evt["evt"] in ["yield", "park"]:
                if -1 != last_tid and last_tid != evt["tid"]:
                    print("Warning: begin with {} but hit {}?".format(
                                                       last_tid, evt["tid"]))
                if -1 == period_tid:
                    period_tid = evt["tid"]
                last_occupied = period_end
                last_tid = -1
                draw_block = True
            if roi and (evt["time"] - toffset) > arg_list.end:
                period_end = min(period_end, arg_list.end)
                draw_block = True

            if draw_block:
                rect = patches.Rectangle((period_beg, # x0
                                          cpu_voffset[-1]-cpu_vspan/2), # y0
                                         period_end - period_beg, # x1-x0
                                         cpu_vspan, # y1-y0
                                         color=color_table[period_tid %
                                                           len(color_table)],
                                         linewidth=0,
                                         alpha=0.3)
                hdl = plt.gca().add_patch(rect)
                hdl.set_gid(id_str + ".cpu")
                hdl = plt.annotate("{} {}-{}".format(period_tid,
                                                     last_occupied_bak,
                                                     evt["time"] - toffset),
                                   ((period_beg + period_end) / 2,
                                     cpu_voffset[-1] + cpu_vspan/2),
                                   textcoords="offset points", xytext=(0,0),
                                   va="top", ha="center")
                hdl.set_gid(id_str + ".cpuLBL")
                patch_groups[id_str] = [id_str + ".cpu"]
            if roi and (evt["time"] - toffset) > arg_list.end:
                break; #assuming the records are sorted by time


    mq_state2voffset = {
            "popping": 0.3,
            "computing": 0,
            "pushing": -0.3,
            "label": -0.7
            }
    task_vspan = 0.1

    patch_groups2 = {}
    for task_id in sch_json["task"].keys():
        last_on_cpu = -1
        last_cpu_id = -1
        mq_state = "unknown"
        last_mq_evt = -1
        for evt in sorted(sch_json["task"][task_id], key=lambda d : d["time"]):
            id_str = "{}_{}_{}".format(evt["cpu"], task_id, evt["time"] - toffset)
            period_beg = max(last_mq_evt, last_on_cpu)
            voffset = 0
            if evt["evt"] in ["jmp", "jd"]:
                last_on_cpu = evt["time"] - toffset
                last_cpu_id = evt["cpu"]
                continue
            elif evt["evt"] in ["yield", "park"]:
                if last_cpu_id != evt["cpu"]:
                    print("Warning: T{} last on CPU {} but yield on CPU {}?".format(
                        task_id, last_cpu_id, evt["cpu"]))
                last_on_cpu = -1
                if mq_state == "unknown":
                    continue;
                voffset = cpu_lbl2voffset[evt["cpu"]] + mq_state2voffset[mq_state]
                #hdl = plt.plot([period_beg, evt["time"]],
                #               [voffset, voffset],
                #               color=color_table[int(task_id) % len(color_table)],
                #               linewidth=3)
            elif evt["evt"] in ["peeked", "pushing", "pushed", "processed"]:
                last_mq_evt = evt["time"] - toffset
                mq_old_state = mq_state
                if "peeked" == evt["evt"]:
                    mq_state = "computing"
                elif "pushing" == evt["evt"]:
                    mq_state = "pushing"
                elif "pushed" == evt["evt"]:
                    mq_state = "computing"
                elif "processed" == evt["evt"]:
                    mq_state = "popping";
                if mq_old_state == "unknown":
                    continue
                voffset = cpu_lbl2voffset[evt["cpu"]] + mq_state2voffset[mq_old_state]
            else:
                continue

            if roi and (evt["time"] - toffset) > arg_list.end:
                break
            if roi and period_beg < arg_list.beg:
                continue;
            rect = patches.Rectangle((period_beg, # x0
                                      voffset-task_vspan/2), # y0
                                     evt["time"] - toffset - period_beg, # x1-x0
                                     task_vspan, # y1-y0
                                     color=color_table[int(task_id) %
                                                       len(color_table)],
                                     linewidth=0)
            hdl = plt.gca().add_patch(rect)
            hdl.set_gid(id_str + "." + evt["evt"])
            #hdl = plt.plot([period_beg, evt["time"] - toffset],
            #               [voffset, voffset],
            #               color=color_table[int(task_id) % len(color_table)],
            #               linewidth=5, markersize=0)
            #hdl[0].set_gid(id_str + "." + evt["evt"])
            hdl = plt.annotate("{}-{}".format(period_beg, evt["time"] - toffset),
                               ((evt["time"] - toffset + period_beg) / 2,
                                 cpu_lbl2voffset[evt["cpu"]] + mq_state2voffset["label"]),
                               textcoords="offset points", xytext=(0,0),
                               ha="center")
            hdl.set_gid(id_str + "." + evt["evt"] + "LBL")
            patch_groups2[id_str] = [id_str + "." + evt["evt"]]


    plt.gca().set_yticks(cpu_voffset)
    plt.gca().set_yticklabels(cpu_id_lbl)
    plt.gca().set_ylim(bottom=1+mq_state2voffset["label"]-0.1,
                       top=cpu_voffset[-1]+cpu_vspan/2+0.1)

    #offsets = [use_yoffset, fill_yoffset, empty_yoffset, recv_yoffset,
    #        req_yoffset]
    #yticks = [addr + offset for addr in sorted(cl_addrs) for offset in offsets]
    #ylabels = sum([["1st data use", "fill in $line",
    #    "{:>X}\n$line vacate".format(addr), "data arrive\n& sendout",
    #    "requests\narrive"]
    #    for addr in sorted(cl_addrs)], [])
    #plt.gca().set_yticks(yticks)
    #plt.gca().set_yticklabels(ylabels)
    if arg_list.xmin:
        plt.gca().set_xlim(left=arg_list.xmin)
    if arg_list.xmax:
        plt.gca().set_xlim(right=arg_list.xmax)

    #plt.savefig("trace.svg")

    hdl = plt.gcf().text(0.02, 0.01, "on", fontsize=8, va='bottom',
            bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5))
    hdl.set_gid("turn_all_on")
    hdl = plt.gcf().text(0.04, 0.01, "off", fontsize=8, va='bottom',
            bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5))
    hdl.set_gid("turn_all_off")

    #plt.gca().get_yaxis().set_major_formatter(ticker.FuncFormatter(to_hex))

    plt.tight_layout()

    #plt.show()

    # Save SVG in a fake file object.
    ff = BytesIO()
    plt.savefig(ff, format="svg")

    # Create XML tree from the SVG file.
    ET.register_namespace("", "http://www.w3.org/2000/svg")
    tree, xmlid = ET.XMLID(ff.getvalue())

    canvas_svg = ET.Element('g', attrib={"id": "myevts"})
    xmlid["figure_1"].insert(5, canvas_svg)

    # Add attributes to the patch objects.
    for id_str, patch_ids in patch_groups2.items():
        tran_group = ET.Element('g', attrib={"id": id_str})
        for patch_id in patch_ids:
            el_pair = ET.Element('g', attrib={"class": "pair"})
            el = xmlid[patch_id]
            el.set('cursor', 'pointer')
            el.set('onclick', "toggle_tran(this)")
            el.set('class', 'evt_blk')
            el_pair.append(el)
            xmlid["axes_1"].remove(el)
            if (patch_id + "LBL") in xmlid.keys():
                el = xmlid[patch_id + "LBL"]
                el.set('visibility', 'hidden')
                el.set('class', 'anno_lbl')
                #el.set('preserveAspectRatio', 'xMidyMid meet')
                el_pair.append(el)
                xmlid["axes_1"].remove(el)
            tran_group.append(el_pair)
        #if id_str not in highlight_groups:
        #    tran_group.set('style', 'opacity:0.3;')
        #xmlid["figure_1"].insert(3, tran_group) # after canvas and x/y axises
        canvas_svg.insert(0, tran_group)

    for id_str, patch_ids in patch_groups.items():
        tran_group = ET.Element('g', attrib={"id": id_str})
        for patch_id in patch_ids:
            el_pair = ET.Element('g', attrib={"class": "pair"})
            el = xmlid[patch_id]
            el.set('cursor', 'pointer')
            el.set('onclick', "toggle_tran(this)")
            el.set('class', 'evt_blk')
            el_pair.append(el)
            xmlid["axes_1"].remove(el)
            if (patch_id + "LBL") in xmlid.keys():
                el = xmlid[patch_id + "LBL"]
                el.set('visibility', 'hidden')
                el.set('class', 'anno_lbl')
                #el.set('preserveAspectRatio', 'xMidyMid meet')
                el_pair.append(el)
                xmlid["axes_1"].remove(el)
            tran_group.append(el_pair)
        #if id_str not in highlight_groups:
        #    tran_group.set('style', 'opacity:0.3;')
        #xmlid["figure_1"].insert(3, tran_group) # after canvas and x/y axises
        canvas_svg.insert(0, tran_group)

    el = xmlid["turn_all_on"]
    el.set('cursor', 'pointer')
    el.set('onclick', "turn_all(true)")
    el = xmlid["turn_all_off"]
    el.set('cursor', 'pointer')
    el.set('onclick', "turn_all(false)")

    # Add a transition effect
    css = tree.getchildren()[0][0]
    css.text = css.text + "g {-webkit-transition:opacity 0.4s ease-out;" + \
        "-moz-transition:opacity 0.4s ease-out;}"
    css.text = css.text + ".pair:hover text {visibility: visible;}"

    # Insert the script and save to file.
    #print(svg_script.format(patch_groups=json.dumps(patch_groups)))
    tree.insert(0, ET.XML(svg_script.format(patch_groups=patch_groups,
                                            patch_groups2=patch_groups2)))

    ET.ElementTree(tree).write(arg_list.foname + ".svg")

if __name__ == "__main__":
    main(sys.argv)

