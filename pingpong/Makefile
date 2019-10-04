AR = ar
CC = gcc
CXX = g++
CFLAGS = -O3  -Wall -std=c99
CXXFLAGS = -O3 -mtune=native -Wall -std=gnu++14
SOFLAGS = -fPIC -shared

THREAD_LIB = -pthread -Wl,--whole-archive -lpthread -Wl,--no-whole-archive
# https://stackoverflow.com/questions/35116327/when-g-static-link-pthread-cause-segmentation-fault-why

BOOST_DIR ?= /benchmarks/VirtualLink/boost_1_63_0
BOOST_INC ?= -I$(BOOST_DIR)
BOOST_LIB ?= -L$(BOOST_DIR)/stage_aarch64/lib -latomic

EXE_NAME = pingpong_native

ifeq (${sim}, true)
    GEM5_DIR = /benchmarks/VirtualLink/gem5
    GEM5_DEF = -DGEM5=1
    GEM5_INC = -I$(GEM5_DIR)/include
    GEM5_LIB = -L$(GEM5_DIR)/util/m5 -lm5
    ifeq (${vl}, true)
        VL_DIR = /benchmarks/VirtualLink/libvl
        GEM5_DEF += -DVL=1
        GEM5_INC += -I$(VL_DIR)
        GEM5_LIB += -L$(VL_DIR) -lvl
        EXE_NAME = pingpong_vl
        ifeq (${verbose}, true)
            GEM5_DEF += -DVERBOSE=1
            EXE_NAME = pingpong_verbose
        endif
    else
        EXE_NAME = pingpong_boost
    endif
endif

CXXOBJS = pingpong affinity
CXXFILES= $(addsuffix .cpp, $(CXXOBJS))
OBJS    = $(addsuffix .o, $(CXXOBJS))

CFLAGS      += $(GEM5_DEF) $(BOOST_INC) $(GEM5_INC)
CXXFLAGS    += $(GEM5_DEF) $(BOOST_INC) $(GEM5_INC)

pingpong: $(CXXFILES)
	rm -rf pingpong $(EXE_NAME)
	$(CXX) $(CXXFLAGS) -c pingpong.cpp -o pingpong.o $(BOOST_LIB) $(GEM5_LIB) $(THREAD_LIB)
	$(CXX) $(CXXFLAGS) -c affinity.cpp -o affinity.o $(BOOST_LIB) $(GEM5_LIB) $(THREAD_LIB)
	$(CXX) $(CXXFLAGS) -static $(OBJS) -o pingpong $(BOOST_LIB) $(GEM5_LIB) $(THREAD_LIB)
	mv pingpong $(EXE_NAME)

.PHONY: clean
clean:
	rm -rf pingpong pingpong_native pingpong_boost pingpong_vl pingpong_verbose *.o
