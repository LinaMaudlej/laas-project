#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <math.h>
 
/************************************************************
This is a simple send/receive program in MPI
************************************************************/

int main(argc,argv)
int argc;
char *argv[];
{
    int rank, numprocs;
    int tag,source,destination,count;
    char *iBuffer, *oBuffer;
    int data_size_B;
    long long int iters, i, k, j, max_j;
    int dY, dZ, dX = 1;
    double *T;
    int verbose = 0;
    char hostname[256];
    char filename[256];
    MPI_Request requests[2];
    MPI_Status  statuses[2];
    FILE *f;
 
#define SUB_ITERS 1

    if (argc != 4) {
      printf("Usage: %s <iterations> <data-size[B]> <job-name>", argv[0]);
      exit(1);
    }
    iters = strtoull(argv[1], NULL, 10);

    MPI_Init(0,&argv);
    MPI_Comm_size(MPI_COMM_WORLD,&numprocs);
    MPI_Comm_rank(MPI_COMM_WORLD,&rank);

    data_size_B = atoi(argv[2]);
    if (!(iBuffer = (char*)calloc(SUB_ITERS*data_size_B*numprocs, sizeof(char)))) {
      printf("-E- Could not allocate buffer\n");
      exit(1);
    }
    if (!(oBuffer = (char*)calloc(SUB_ITERS*data_size_B*numprocs, sizeof(char)))) {
      printf("-E- Could not allocate buffer\n");
      exit(1);
    }

    if (!(T = (double*)calloc(iters/100+2, sizeof(double)))) {
      printf("-E- Could not allocate T\n");
      exit(1);
    }

    if (rank == 0) {
      printf("Stencil_Iters X Y ranks: %d iters: %ld size: %d\n",
	     numprocs, iters, data_size_B);
    }
    gethostname(hostname, 256);
    printf("Stencil_Iters rank: %d runs on: %s\n", rank, hostname);

    dX = 1;
    dY = 1;

    tag=0;
    j = 0;
    T[0] = MPI_Wtime();
    for (i = 0; i < iters; i++) {
      /* Calc */
      usleep(50);

      /* +X */
      destination = (rank + dX) % numprocs;
      source = (numprocs +rank - dX) % numprocs;
      for (k=0; k<SUB_ITERS;k++) {
	++tag;
	if (verbose) {
	  printf("-D- Sending from %d to %d tag: %d\n", rank, destination, tag);
	}
	MPI_Isend(oBuffer+k*data_size_B,data_size_B,MPI_BYTE,destination,tag,
		  MPI_COMM_WORLD, &requests[0]);
	if (verbose) {
	  printf("-D- Receive from %d to %d tag: %d\n", source, rank, tag);
	}
	MPI_Irecv(iBuffer+k*data_size_B,data_size_B,MPI_BYTE,source,tag,
		  MPI_COMM_WORLD,&requests[1]);
	
	MPI_Waitall(2, requests, statuses);
      }
	

      MPI_Barrier(MPI_COMM_WORLD);

      /* -X */
      source = (rank + dX) % numprocs;
      destination = (numprocs +rank - dX) % numprocs;
      for (k=0; k<SUB_ITERS;k++) {
	++tag;
	if (verbose) {
	  printf("-D- Sending from %d to %d tag: %d\n", rank, destination, tag);
	}
	MPI_Isend(oBuffer+k*data_size_B,data_size_B,MPI_BYTE,destination,tag,
		  MPI_COMM_WORLD, &requests[0]);
	if (verbose) {
	  printf("-D- Receive from %d to %d tag: %d\n", source, rank, tag);
	}
	MPI_Irecv(iBuffer+k*data_size_B,data_size_B,MPI_BYTE,source,tag,
		  MPI_COMM_WORLD,&requests[1]);

	MPI_Waitall(2, requests, statuses);
      }

      MPI_Barrier(MPI_COMM_WORLD);

      /* +Y */
      destination = (rank + dY) % numprocs;
      source = (numprocs +rank - dY) % numprocs;
      for (k=0; k<SUB_ITERS;k++) {
	++tag;
	if (verbose) {
	  printf("-D- Sending from %d to %d tag: %d\n", rank, destination, tag);
	}
	MPI_Isend(oBuffer+k*data_size_B,data_size_B,MPI_BYTE,destination,tag,
		  MPI_COMM_WORLD, &requests[0]);
	if (verbose) {
	  printf("-D- Receive from %d to %d tag: %d\n", source, rank, tag);
	}
	MPI_Irecv(iBuffer+k*data_size_B,data_size_B,MPI_BYTE,source,tag,
		  MPI_COMM_WORLD, &requests[1]);

	MPI_Waitall(2, requests, statuses);
      }

      MPI_Barrier(MPI_COMM_WORLD);

      /* -Y */
      source = (rank + dY) % numprocs;
      destination = (numprocs +rank - dY) % numprocs;
      for (k=0; k<SUB_ITERS;k++) {
	++tag;
	if (verbose) {
	  printf("-D- Sending from %d to %d tag: %d\n", rank, destination, tag);
	}
	MPI_Isend(oBuffer+k*data_size_B,data_size_B,MPI_BYTE,destination,tag,
		  MPI_COMM_WORLD, &requests[0]);
	if (verbose) {
	  printf("-D- Receive from %d to %d tag: %d\n", source, rank, tag);
	}
	MPI_Irecv(iBuffer+k*data_size_B,data_size_B,MPI_BYTE,source,tag,
		  MPI_COMM_WORLD, &requests[1]);

	MPI_Waitall(2, requests, statuses);
      }

      MPI_Barrier(MPI_COMM_WORLD);
      if ((i+1) % 100 == 0) {
	T[j++] = MPI_Wtime();
      }
    }
    T[j] = MPI_Wtime();
    max_j = j;
    double elappsed = T[max_j] - T[0];
    if (rank == 0) {
	sprintf(filename, "%s.iter_t.csv", argv[3]);
      printf("All2all runtime: %f sec. Writting 100 iters CSV file: %s\n", 
	     elappsed, filename);
      f=fopen(filename, "w");
      for (i = 1; i < max_j; i++) {
        fprintf(f, "%d, %g\n", i, T[i] - T[i-1]);
      }
      close(f);
    }

    MPI_Finalize();
}
