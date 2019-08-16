AR = ar
CC = gcc
CXX = g++
CCFLAGS = -O3 -g -Wall -std=c89
CXXFLAGS = -O3 -g -Wall -std=c++11
SOFLAGS = -fPIC -shared

THREAD_LIB = -pthread -Wl,--whole-archive -lpthread -Wl,--no-whole-archive
# https://stackoverflow.com/questions/35116327/when-g-static-link-pthread-cause-segmentation-fault-why

BOOST_PATH = /benchmarks/VirtualLink/boost_1_63_0
BOOST_INC = -I$(BOOST_PATH)
BOOST_LIB = -L$(BOOST_PATH)/stage_aarch64/lib -latomic

VL_PATH = /benchmarks/VirtualLink/libvl
VL_INC = -I$(VL_PATH)
VL_LIB = -L$(VL_PATH) -lvl

GEM5_PATH = /benchmarks/VirtualLink/gem5
ifdef GEM5
	GEM5_DEF = -DGEM5=1
	GEM5_INC = -I$(GEM5_PATH)/include
	GEM5_LIB = -L$(GEM5_PATH)/util/m5 -lm5
endif

CCFLAGS += $(GEM5_DEF) $(BOOST_INC) $(VL_INC) $(GEM5_INC)
CXXFLAGS += $(GEM5_DEF) $(BOOST_INC) $(VL_INC) $(GEM5_INC)

pingponga: pingpong.cpp
	$(CXX) $(CXXFLAGS) -static $< -o $@ $(BOOST_LIB) $(VL_LIB) $(GEM5_LIB) $(THREAD_LIB)

.PHONY: clean
clean:
	rm -rf pingponga *.o
