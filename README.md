Virtual Link Microbenchmarks
============================

pingpong
--------
Two threads transfer data back and forth.
To compile and run on real hardware (needs boost library installed):
~~~bash
make pingpong
./pingpong_native 10 7
~~~
Binary takes two optional position arguments, first is round, second is burst.
To compile for Gem5 simulation:
~~~bash
make sim=true pingpong
make sim=true vl=true pingpong
make sim=true vl=true verbose=true pingpong
~~~
Three binaries:
- `pingpong_boost` uses `boost::lockfree::queue`;
- `pingpong_vl` requires Virtual Link support in Gem5,
and should compile with [libvl](https://github.com/jonathan-beard/libvl.git);
- `pingpong_verbose` is the verbose version of `pingpong_vl`
to validate the functionality of Virtual Link.

shopping
--------
Multiple threads (candy lovers) access a shared structure (cart) concurrently.
There several predefined access patterns:
- Watcher (W): only read the price from cart
- MM (M), KitKat (K), SNICKERS (S), Hershey's (H) Fan:
only read the price of a single candy and add to cart subtotal
- Round Robin (R): all behaviors (including watch) in turn
- Biased (B): MM and another iteratively

There are two compile switches to explore the different cache behaviors:
- `ATOMIC`: use atomic memory access to trigger cache coherence activities
- `PADDING`: avoid producer-consumer false sharing by add padding in structure

To compile on real hardware (needs boost library installed):
~~~bash
make
~~~
To test, write a file with the configurations, then `make test_<filename>`:
~~~bash
cat > examples.config << "EOF"
25 Watcher W 10 26 MMFan M 30
25 Watcher W 10 26 MMFan M 30 24 KitKatFan K 30 23 SNICKERSFan S 30 22 "Hensery'sFan" H 30
EOF
make test_examples
cat test_examples.log
~~~
To profile (needs `perf c2c` tool available):
~~~bash
make perf_examples
perf c2c report --stdio -i perf_30Sep2019CDT1855/atomicFSh0.perf.data
~~~
Four binaries:
- `atomicFSh` uses atomic memory access intrinsic to get correct subtotal,
while it has producer-consumer false sharing;
- `atomicPad` also uses atomic memory access intrinsic,
but false sharing is avoided by add padding to the cart data structure.
- `directFSh` and `directPad` does not use atomic memory access,
so should have no cacheline pingpong, but give wrong subtotal.
