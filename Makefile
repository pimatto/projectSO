CC=gcc
CCOPTS=--std=gnu99 -Wall -D_LIST_DEBUG_ 
AR=ar

OBJS=linked_list.o\
     fake_process.o\
     fake_os.o\
     fake_cpu.o

HEADERS=linked_list.h  fake_process.h  fake_cpu.h

BINS=fake_process_test sched_sim

#disastros_test

.phony: clean all


all:	$(BINS)

%.o:	%.c $(HEADERS)
	$(CC) $(CCOPTS) -c -o $@  $<

fake_process_test:	fake_process_test.c $(OBJS)
	$(CC) $(CCOPTS) -o $@ $^

sched_sim:	sched_sim.c $(OBJS)
	$(CC) $(CCOPTS) -o $@ $^ -lm -g

clean:
	rm -rf *.o *~ $(OBJS) $(BINS)
