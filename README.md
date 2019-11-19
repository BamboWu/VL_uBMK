Virtual Link Microbenchmarks
============================

Compile
-------
~~~shell
mkdir -p build
cd build
cmake -DBOOST_ROOT=<path to boost library> ../
make
~~~
The executables of each microbenchmark are located in their own folders
under `build/apps`.
If boost library is installed in the system and is the preferred version,
there is not need to set `-DBOOST_ROOT` in `cmake` command.

Microbenchmarks
---------------

### pingpong
Two threads transfer data back and forth.
To run on real hardware (needs boost library installed):
~~~shell
./pingpong_native 10 7
~~~
Binary takes two optional position arguments, first is round, second is burst.

Four binaries:
- `pingpong_native` is the one for testing and profiling on real hardware.
- `pingpong_boost` uses `boost::lockfree::queue`,
and Gem5 simulation support is enabled;
- `pingpong_vl` requires Virtual Link support in Gem5,
and should compile with [libvl](https://github.com/jonathan-beard/libvl.git);
- `pingpong_verbose` is the verbose version of `pingpong_vl`
to validate the functionality of Virtual Link.

### shopping
Multiple threads (candy lovers) access a shared structure (cart) concurrently.
There several predefined access patterns:
- Watcher (W): only read the price from cart
- MM (M), KitKat (K), SNICKERS (S), Hershey's (H) Fan:
only read the price of a single candy and add to cart subtotal
- Round Robin (R): all behaviors (including watch) in turn
- Biased (B): MM and another iteratively

Two compiler switches (cmake has taken care of)
to explore the different cache behaviors:
- `ATOMIC`: use atomic memory access to trigger cache coherence activities
- `PADDING`: avoid producer-consumer false sharing by add padding in structure

Three binaries:
- `atomicFSh` uses atomic memory access intrinsic to get correct subtotal,
while it has producer-consumer false sharing;
- `atomicPad` also uses atomic memory access intrinsic,
but false sharing is avoided by add padding to the cart data structure.
- `directFSh` and `directPad` does not use atomic memory access,
so should have no cacheline pingpong, but give wrong subtotal.

One profiling script: `scripts/perf_shopping.slurm`.
The variable `ROOT_DIR` in the script is the path where the repo cloned.
Another variable `PLATFORM` informs the script to look for a corresponding
hardware configuration file (including the performance counters,
number of sockets, number of cores, level of hyperthreading,
and preferred order to utilize cores).
Example hardware configurations are also available in `scripts` folder:
`scripts/SKX.cfg` for TACC Stampede2 Skylake node,
`scripts/CLX.cfg` for TACC Frontera Cascade Lake node,
`scripts/jupiter1.cfg` for LCA jupiter1 server.
After making the hardware configuration file and modifying the variables,
you may have a preview of the commands the script is going to execute:
~~~shell
scripts/perf_shopping.slurm preview
~~~
To conduct the profiling:
~~~
scripts/perf_shopping.slurm >& perf_shopping.o.123
~~~
or on a platform managed by Slurm:
~~~
sbatch scripts/perf_shopping.slurm
~~~
The profiling result would be in a subfolder (latest timestamp in its name)
under `data` folder.

One script to parse data and make table: `scripts/tab_shopping.py`
The script operates on the perf outputs and the output from the microbenchmark.
For example, assume the perf outputs are in `data/perf_shopping_SKX_30Sep1855`
and the output is redirected to `perf_shopping.o.123`:
~~~shell
mv perf_shopping.o.123 data/perf_shopping_SKX_30Sep1855/
scripts/tab_shopping.py data/perf_shopping_SKX_30Sep1855
~~~
The script will print out a table in Tab Seperated Value (TSV) format,
so you can copy and paste into a Google spreadsheet or Excel,
or just redirect the output to `.tsv` file.

### shuffler
This is designed to generate enough random accesses,
in order to "reinitialize" the state of cache hierarchy.
It takes number of physical cores and LLC capacity as argument.
Each physical core runs a thread to update a shared table,
until every entry in the table counts up to target, 256.
The totoal memory consumption should sum up to about the given capacity of LLC.
For example, to run it on a 6-core processor with 15MB LLC:
~~~shell
./shuffler SpD 6 15728640
~~~
