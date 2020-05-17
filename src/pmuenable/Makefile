KDIR=/usr/src/`uname -r`
LIBPATH=$(KDIR)/lib
INCPATH=$(KDIR)/include

CC := gcc

obj-m += pmuenable.o

PWD:= $(shell pwd)

default:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

trypmu: trypmu.c
	gcc -o $@ $<

clean:
	make -C $(KDIR) M=$(PWD) clean
	rm -rf trypmu
