all: serial mpi ring star queue

serial: serial.c
	cc serial.c -o serial

mpi: mpi.c
	mpicc mpi.c -o mpi

ring: ring.c
	mpicc ring.c -o ring

star: star.c
	mpicc star.c -o star

queue: queue.c
	mpicc queue.c -o queue
