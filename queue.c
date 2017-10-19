#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <mpi.h>
#include "unrolled_int_linked_list.c"

#define MAX_KEYWORD_LENGTH 10
#define MAX_LINE_LENGTH 2001
#define BATCH_SIZE 10

#define WIKI_FILE "/test10-%s.txt"
#define KEYWORD_FILE "/keywords.txt"
#define OUTPUT_FILE "/output/wiki-%s-part-%03d.out"

double myclock();

int compare(const void* a, const void* b) {
  const char **ia = (const char **)a;
  const char **ib = (const char **)b;
  return strcmp(*ia, *ib);
}

void read_in_words();
void read_in_wiki();

int main(int argc, char * argv[])
{
  int nwords, maxwords = 100;
  int nlines, maxlines = 1000000;
  struct Node** hithead;
  struct Node** hittail;
  char *wordmem, **word, *linemem, **line, *tempwordmem;
  FILE *fd;
  double nchars = 0;

  int i, k;

  int numtasks, rank, len, rc;
  char hostname[MPI_MAX_PROCESSOR_NAME];

  MPI_Init(&argc, &argv);
  MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Get_processor_name(hostname, &len);
  printf("Number of tasks= %d, My rank= %d, Running on %s\n", numtasks, rank, hostname);

  if(argc != 6){
    printf("Usage: %s <job id> <input size> <parallel environment> <nslots> <nhosts>", argv[0]);
    return -1;
  }

  
  if(rank)
  {
    allocateNodePools();

    // Allocate arrays for the heads and tails of the lists
    hithead = (struct Node**) malloc( BATCH_SIZE * sizeof(struct Node *) );
    hittail = (struct Node**) malloc( BATCH_SIZE * sizeof(struct Node *) );
    for( i = 0; i < BATCH_SIZE; i++ ) {
      hithead[i] = hittail[i] = node_alloc();
    }

  }

  // Set memory footprint for words depending on worker or master
  int my_num_words;
  if(rank) {
    my_num_words = BATCH_SIZE;
  }
  else {
    my_num_words = maxwords;
  }

  // Contiguous memory for words
  wordmem = malloc(my_num_words * MAX_KEYWORD_LENGTH * sizeof(char));
  word = (char **) malloc( maxwords * sizeof( char * ) );
  for(i = 0; i < my_num_words; i++){
    word[i] = wordmem + i * MAX_KEYWORD_LENGTH;
  }

  // Contiguous memory for lines
  linemem = malloc(maxlines * MAX_LINE_LENGTH * sizeof(char));
  line = (char **) malloc( maxlines * sizeof( char * ) );
  for( i = 0; i < maxlines; i++ ) {
    line[i] = linemem + i * MAX_LINE_LENGTH;
  }

  
  // Read in the words and sort them
  if(!rank)
  {
    int err;
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
  if(!rank)
  {
    int err;
    char *input_file = (char*)malloc(500 * sizeof(char));
    sprintf(input_file, WORKING_DIRECTORY WIKI_FILE, argv[2]);
    fd = fopen( input_file, "r" );
    if(!fd) {
      printf("\n------------------------------------\n"
           "\nERROR: Can't open wiki file.  Not found.\n"
           "\n------------------------------------\n");
    }
    nlines = -1;
    do {
      err = fscanf( fd, "%[^\n]\n", line[++nlines] );
      if( line[nlines] != NULL ) nchars += (double) strlen( line[nlines] );
    } while( err != EOF && nlines < maxlines);
    fclose( fd );
    free(input_file); input_file = NULL;

    printf( "Read in %d lines averaging %.0lf chars/line\n", nlines, nchars / nlines);
  } 


  // Broadcast lines
    MPI_Bcast(&nlines, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(linemem, nlines * MAX_LINE_LENGTH, MPI_CHAR, 0, MPI_COMM_WORLD);


  // Do work...

  // Workers
  if(rank) {
    MPI_Status stat;
    int someval = 1;
    int batches = nwords / BATCH_SIZE;
    int my_num_batches = 0;
    int batch_number = 0;
    
    int *result_size = malloc(nwords * sizeof(int));
    int *result_id = malloc(nwords * sizeof(int));
    int **result_arr = malloc(nwords * sizeof(int *));
    int num_results = 0;

    while(1) {

      MPI_Sendrecv(&someval, 1, MPI_INT, 0, 1, wordmem, BATCH_SIZE * MAX_KEYWORD_LENGTH, MPI_CHAR, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &stat);

      if(*wordmem == 0) {
	printf("Rank %d:  No more batches available.\n", rank);
	break;
      }

      my_num_batches++;
      MPI_Recv(&batch_number, 1, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &stat);

      for(i = 0; i < nlines; i++) {
        for(k = 0; k < BATCH_SIZE; k++) {
	  if( strstr( line[i], word[k] ) != NULL ) {
	    hittail[k] = add(hittail[k], i);
	  }
        }
      }

      // Send back results here
      
      printf("\n\nBatch %d on Rank %d:\n", batch_number, rank);
      for(i = 0; i < BATCH_SIZE; i++) {
        int *current_result;
	int len;
        
        printf("%s: ", word[i]);

	// this function mallocs for line_numbers...
	toArray(hithead[i], &current_result, &len);

        for (k = 0; k < len - 1; k++) {
	  printf("%d, ", current_result[k]);
	}

	printf("%d\n", current_result[len-1]);

	// ...so we must free it
	free(current_result); current_result = NULL;
      }

      // Reset linked list for next batch, ignore previous garbage.
      for(i = 0; i < BATCH_SIZE; i++) {
        hithead[i] = hittail[i] = node_alloc();
      }
    }

    printf("Rank %d had %d batches!\n", rank, my_num_batches);


    for(i = 0; i < num_results; i++)
    {
      free(result_arr[i]);
    }
    free(result_id);
    free(result_arr);
    free(result_size);
  } 
  
  // Rank 0
  else {
    MPI_Status stat;
    int someval = 1;
    int batches = nwords / BATCH_SIZE;
    for(k = 0; k < batches; k++) {
      MPI_Recv(&someval, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &stat);
      MPI_Send(wordmem + (k * MAX_KEYWORD_LENGTH * BATCH_SIZE), BATCH_SIZE * MAX_KEYWORD_LENGTH, MPI_CHAR, stat.MPI_SOURCE, someval, MPI_COMM_WORLD);
      MPI_Send(&k, 1, MPI_INT, stat.MPI_SOURCE, someval, MPI_COMM_WORLD);
    }
    *wordmem = 0;
    for(k = 0; k < numtasks - 1; k++) {
      MPI_Recv(&someval, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &stat);
      MPI_Send(wordmem, BATCH_SIZE * MAX_KEYWORD_LENGTH, MPI_CHAR, stat.MPI_SOURCE, someval, MPI_COMM_WORLD);
    }

    printf("Cake 123!\n");
  }


  // Clean up

  // Lines
  free(line);    line = NULL;
  free(linemem); linemem = NULL;

  // Words
  free(word);    word = NULL;
  free(wordmem); wordmem = NULL;

  if(rank)
  {
    destroyNodePools();
    free(hithead); hithead = NULL;
    free(hittail); hittail = NULL;
  }

  MPI_Finalize();
  return 0;
}
