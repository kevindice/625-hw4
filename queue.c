#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <mpi.h>
#include "unrolled_int_linked_list.c"

#define MAX_KEYWORD_LENGTH 10
#define MAX_LINE_LENGTH 2001
#define BATCH_SIZE 100

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
  int nwords, maxwords = 50000;
  int nlines, maxlines = 1000000;
  struct Node** hithead;
  struct Node** hittail;
  char *wordmem, **word, *linemem, **line, *tempwordmem;

  int i;

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
    hithead = (struct Node**) malloc( maxwords * sizeof(struct Node *) );
    hittail = (struct Node**) malloc( maxwords * sizeof(struct Node *) );
    for( i = 0; i < maxwords; i++ ) {
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

}
