CC = gcc
GPU = acpp

FLAGS = -O3

.PHONY: all clean

all: seq cpu gpu

seq: seq.c
	$(CC) $(FLAGS) -o seq seq.c

cpu: cpu.c
	$(CC) $(FLAGS) -fopenmp -o cpu cpu.c

gpu: gpu.cc
	$(GPU) $(FLAGS) -o gpu gpu.cc

clean:
	rm -fr *.o seq cpu gpu
