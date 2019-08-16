Virtual Link Microbenchmarks
============================

pingpong
--------
Two threads transfer data back and forth.
To compile and run on real hardware (needs boost library installed):
~~~
make pingpong
./pingpong_native 10 7
~~~
Binary takes two optional position arguments, first is round, second is burst.
To compile for Gem5 simulation:
~~~
make sim=true pingpong
make sim=true vl=true pingpong
make sim=true vl=true verbose=true pingpong
~~~
Three binaries:
`pingpong_boost` uses `boost::lockfree::queue`;
`pingpong_vl` requires Virtual Link support in Gem5,
and should compile with [libvl](https://github.com/jonathan-beard/libvl.git);
`pingpong_verbose` is the verbose version of `pingpong_vl`
to validate the functionality of Virtual Link.
