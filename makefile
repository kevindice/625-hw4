WD=$(shell pwd)
HOST=$(shell hostname)

FLAGS := -D HOST=$(HOST)
FLAGS := -D WORKING_DIRECTORY=\"$(WD)\"

all: mpi ring star queue

mpi: mpi.c
	mpicc mpi.c -o mpi $(FLAGS)

ring: ring.c
	mpicc ring.c -o ring $(FLAGS)

star: star.c
	mpicc star.c -o star $(FLAGS)

queue: queue.c
	mpicc queue.c -o queue $(FLAGS)

clean:
	rm mpi ring star queue
