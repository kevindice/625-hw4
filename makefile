all: serial mpi

serial: serial.c
	cc serial.c -o serial

mpi: mpi.c
	mpicc mpi.c -o mpi
