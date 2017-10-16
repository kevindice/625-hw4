#include <stdio.h>
#include <mpi.h>
int main(int argc, char **argv)
{
  int rank, size, left, tag=99;
  MPI_Request request;

  MPI_Status status;
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  MPI_Isend(&rank, 1, MPI_INTEGER, (rank+1)%size, tag, MPI_COMM_WORLD, &request);
  MPI_Recv(&left, 1, MPI_INTEGER, (rank+size-1)%size, tag,
    MPI_COMM_WORLD, &status);
  MPI_Wait( &request, &status );

  printf("I am MPI process %d of %d, on my left is %d\n", rank, size, left);

  MPI_Finalize();

  return 0;
}
