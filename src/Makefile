CC = gcc
GPU = acpp

FLAGS = -O3

.PHONY: all clean

all: seq cpu gpu

seq: seq.c
	$(CC) $(FLAGS) -o seq seq.c

cpu: cpu.c
	$(CC) $(FLAGS) -fopenmp -o cpu cpu.c
	
gpu: gpu.c
	$(GPU) $(FLAGS) -o gpu gpu.c

clean:
	rm -fr *.o seq cpu gpu
