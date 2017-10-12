#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <mpi.h>
#include "unrolled_int_linked_list.c"

#define MAX_KEYWORD_LENGTH 10
#define MAX_LINE_LENGTH 2001

#define WIKI_FILE "/test10-%s.txt"
#define KEYWORD_FILE "/keywords.txt"
#define OUTPUT_FILE "/output/wiki-%s-part-%03d.out"

double myclock();

int compare(const void* a, const void* b) {
  const char **ia = (const char **)a;
  const char **ib = (const char **)b;
  return strcmp(*ia, *ib);
}

int main(int argc, char * argv[])
{
  int nwords, maxwords = 50000;
  int nlines, maxlines = 1000000;
  int i, k, n, err, *count;
  double nchars = 0;
  int start, end;
  double tstart, ttotal, tlast;
  FILE *fd;
  char *wordmem, **word, *linemem, **line, *tempwordmem;
  struct Node** hithead;
  struct Node** hittail;

  // MPI related
  int numtasks, rank, len, rc;
  char hostname[MPI_MAX_PROCESSOR_NAME];

  MPI_Init(&argc, &argv);
  MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Get_processor_name(hostname, &len);
  printf("Number of tasks= %d, My rank= %d, Running on %s\n", numtasks, rank, hostname);

  if(argc != 6){
     printf("Usage: %s <job id> <input size> <parallel environment> <nslots> <nhosts>\n", argv[0]);
     return -1;
  }

  // Set up timing for ALL cores
  tstart = myclock();  // Set the zero time
  tstart = myclock();  // Start the clock
  tlast = tstart;

  // Malloc space for the word list and lines

  count = (int *) calloc( maxwords, sizeof( int ) );

  // Allocate node pools
  allocateNodePools();

  // Contiguous memory for words
  wordmem = malloc(maxwords * MAX_KEYWORD_LENGTH * sizeof(char));
  word = (char **) malloc( maxwords * sizeof( char * ) );
  for(i = 0; i < maxwords; i++){
    word[i] = wordmem + i * MAX_KEYWORD_LENGTH;
  }

  // Allocate arrays for the heads and tails of the lists
  hithead = (struct Node**) malloc( maxwords * sizeof(struct Node *) );
  hittail = (struct Node**) malloc( maxwords * sizeof(struct Node *) );
  for( i = 0; i < maxwords; i++ ) {
    hithead[i] = hittail[i] = node_alloc();
  }

  // Contiguous memory...yay
  linemem = malloc(maxlines * MAX_LINE_LENGTH * sizeof(char));
  line = (char **) malloc( maxlines * sizeof( char * ) );
  for( i = 0; i < maxlines; i++ ) {
    line[i] = linemem + i * MAX_LINE_LENGTH;
  }

  // Read in the keywords and sort them

  if(rank == 0)
  {
    fd = fopen( WORKING_DIRECTORY KEYWORD_FILE, "r" );
    if(!fd) {
        printf("\n------------------------------------\n"
	       "\nERROR: Can't open keyword file.  Not found.\n"
	       "\n------------------------------------\n");
    }
    nwords = -1;
    do {
      err = fscanf( fd, "%[^\n]\n", word[++nwords] );
    } while( err != EOF && nwords < maxwords );
    fclose( fd );

    printf( "Read in %d words\n", nwords);

    // sort and copy back to original contiguous memory
    //
    // qsort throws your pointers around and doesn't sort your
    // strings in place.  To do this, we sort, copy over to temp memory
    // by accessing our shuffled/sorted (depends on your perspective)
    // pointers, memcpy back to original memory block

    qsort(word, nwords, sizeof(char *), compare);
    tempwordmem = malloc(maxwords * MAX_KEYWORD_LENGTH * sizeof(char));
    for(i = 0; i < maxwords; i++){
      for(k = 0; k < MAX_KEYWORD_LENGTH; k++){
        *(tempwordmem + i * MAX_KEYWORD_LENGTH + k) = word[i][k];
      }
      word[i] = wordmem + i * MAX_KEYWORD_LENGTH;
    }
    memcpy(wordmem, tempwordmem, maxwords * MAX_KEYWORD_LENGTH);
    free(tempwordmem); tempwordmem = NULL;
  }


  // Read in the lines from the data file

  if(rank == 0)
  {
    char *input_file = (char*)malloc(500 * sizeof(char));
    sprintf(input_file, WORKING_DIRECTORY WIKI_FILE, argv[2]);
    fd = fopen( input_file, "r" );
    printf("\n------------------------------------\n"
           "\nERROR: Can't open wiki file.  Not found.\n"
           "\n------------------------------------\n");
    nlines = -1;
    do {
      err = fscanf( fd, "%[^\n]\n", line[++nlines] );
      if( line[nlines] != NULL ) nchars += (double) strlen( line[nlines] );
    } while( err != EOF && nlines < maxlines);
    fclose( fd );
    free(input_file); input_file = NULL;

    printf( "Read in %d lines averaging %.0lf chars/line\n", nlines, nchars / nlines);
  }

  if(rank == 0)
  {
    printf("Read in and MPI comm overhead for %d lines and %d procs = %lf seconds\n", nlines, numtasks, myclock() - tlast);
    printf("OHEAD\t%d\t%d\t%lf\t%s\t%s", nlines, numtasks, myclock() -tlast, argv[3], argv[5]);
    printf("\n******  Starting work  ******\n\n");
  }
  tlast = myclock();


  int next, previous;
  // Calculate the previous and next ranks (wrap around using modulus)
  next = (rank + 1) % numtasks;
  previous = (rank + numtasks - 1) % numtasks;

  // Calculate the set of the wiki dump for the current process to work on
  start = rank * (nwords/numtasks);
  end = (rank + 1) * (nwords/numtasks);
  if(rank == numtasks - 1) end = nwords;

  // Malloc the variable for the current keyword
  char *keyword = (char*) malloc(MAX_KEYWORD_LENGTH * sizeof(char));

  int keywordIterator, keywordCount;
  // If the current rank is 0, set the first keyword.
  if (rank == 0) {
    keywordIterator = 0;
    keyword = word[keywordIterator];
  } else {
    keyword = '\0';
  }

  // Malloc a variable for the last keyword so other ranks know where to stop
  char *lastKeyword = (char*) malloc(MAX_KEYWORD_LENGTH * sizeof(char));

  // Get the last keyword
  if (rank == 0) {
    lastKeyword = word[maxwords];
  }

  // Broadcast it
  MPI_Bcast(lastKeyword, MAX_KEYWORD_LENGTH, MPI_CHAR, 0, MPI_COMM_WORLD);

  MPI_Status status;

  // Create a loop for doing the work
  while (1) {
    // Check if we (don't) have a keyword
    if (keyword == '\0') {
      // If we don't and this is rank 0, get the next keyword
      if (rank == 0) {
        keywordIterator++;
        keyword = word[keywordIterator];
      } else {
        // Not rank 0, so listen for keyword and set it
        // Use tag 1
        MPI_Recv(keyword, MAX_KEYWORD_LENGTH, MPI_CHAR, previous, 1, MPI_COMM_WORLD, &status);

        // TODO: Listen for line numbers and set them
        // Use tag 2
        // MPI_Recv()
      }
    }

    // Set the count for the current keyword in the current set to 0
    keywordCount = 0;

    // Search through the set for the current rank, searching for the keyword
    for( i = start; i < end; i++ ) {
      if( strstr( line[i], keyword ) != NULL ) {
        // Increase counter
        keywordCount++;

        // TODO: Store the line number

      }
    }

    // Send the keyword to the next process
    // Use tag 1
    MPI_Send(keyword, MAX_KEYWORD_LENGTH, MPI_CHAR, next, 1, MPI_COMM_WORLD);

    // TODO: Send the lines found to the next process
    // Use tag 2
    // MPI_Send();

    // TODO: Listen for keyword to come back, then print it and all lines it was on
    if (rank == 0) {
      // Print the stuff

      // If the message was the last keyword, break the loop

    } else {
      // If this was the last keyword
      if (keyword == lastKeyword) {
        // break the Loop
        break;
      }
    }
      // Clear the keyword for next Loop
      keyword = '\0';
  }

  // TODO: Make sure there are no more keywords to listen for here

  printf("------- Proc: %d, Start: %d, End: %d, Nwords: %d, Num tasks: %d --------\n", rank, start, end, nwords, numtasks);

  printf("\n\nPART_DONE\trank %d\tafter %lf seconds\twith %s slots\ton %s hosts\twith pe %s\n",
      rank, myclock() - tlast, argv[4], argv[5], argv[3]); fflush(stdout);

  // Take end time when all are finished

  printf("================\n"
    "Rank %d --- Unrolled linked list stats:\n\n"
    "Node Pools: %d\n"
    "Current Node Count: %d\n"
    "Total Nodes Allocated: %d\n"
    "Nodes in Use: %d\n"
    "==================\n",
    rank,
    _num_node_pools,
    _current_node_count,
    _num_node_pools * MEMORY_POOL_SIZE,
    nodes_in_use
  ); fflush(stdout);

  MPI_Barrier(MPI_COMM_WORLD);

  if(rank == 0)
  {
    ttotal = myclock() - tstart;
  }


  // Clean up after ourselves


  // Linked list counts
  destroyNodePools();
  free(hithead);  hithead = NULL;
  free(hittail);  hittail = NULL;

  // Words
  free(word);     word = NULL;
  free(wordmem);  wordmem = NULL;

  // Lines
  free(line);     line = NULL;
  free(linemem);  linemem = NULL;

  if(rank == 0)
  {
    sleep(1);
    fflush(stdout);
    printf( "DATA\t%lf\t%d\t%d\t%s\t%s\t%s\t%s\n",
       ttotal, nwords, nlines, argv[2], argv[3], argv[4], argv[5]);
    fflush(stdout);
  }

  MPI_Finalize();
  return 0;
}

double myclock() {
  static time_t t_start = 0;  // Save and subtract off each time

  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  if( t_start == 0 ) t_start = ts.tv_sec;

  return (double) (ts.tv_sec - t_start) + ts.tv_nsec * 1.0e-9;
}
