CC = gcc
CFLAGS = -g -pthread -lnuma

timestamp = $(shell date +"%d%b%Y%Z%H%M")
counter = 0

targets = atomicFSh atomicPad directFSh directPad

atomicFSh : shopping.c
	$(CC) $(CFLAGS) -o $@ shopping.c -DATOMIC

atomicPad : shopping.c
	$(CC) $(CFLAGS) -o $@ shopping.c -DATOMIC -DPADDING

directFSh : shopping.c
	$(CC) $(CFLAGS) -o $@ shopping.c

directPad : shopping.c
	$(CC) $(CFLAGS) -o $@ shopping.c -DPADDING

test_%: %.config atomicPad
	while IFS= read line; do \
	  echo -e "\033[92m$$line\033[0m" >> $@.log; \
	  ./atomicPad $$line >> $@.log; \
	done < $<

perf_%: %.config all
	mkdir perf_${timestamp}
	cp $< perf_${timestamp}
	counter=$(counter); \
	while IFS= read line; do \
	  echo -e "\033[92m$$line\033[0m" >> $@.log; \
	  perf c2c record ./atomicFSh $$line >> $@.log; \
	  mv perf.data perf_${timestamp}/atomicFSh$$counter.perf.data; \
	  perf c2c record ./atomicPad $$line >> $@.log; \
	  mv perf.data perf_${timestamp}/atomicPad$$counter.perf.data; \
	  perf c2c record ./directFSh $$line >> $@.log; \
	  mv perf.data perf_${timestamp}/directFSh$$counter.perf.data; \
	  perf c2c record ./directPad $$line >> $@.log; \
	  mv perf.data perf_${timestamp}/directPad$$counter.perf.data; \
	  counter=`expr $$counter + 1`; \
	done < $<
	mv $@.log perf_${timestamp}/

clean:
	rm -rf ${targets}

.DEFAULT_GOAL :=
all: clean ${targets}

.PHONY : clean all
