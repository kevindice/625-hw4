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
        int nwords, maxwords = 50000; //add again
        int nlines, maxlines = 1000000;
        int i, j, k, n, err, *count;
        double nchars = 0;
        int start, end;
        double tstart, ttotal, tlast;
        FILE *fd;
        char *wordmem, **word, *linemem, **line, *tempwordmem;
        //struct Node** hithead;
        //struct Node** hittail;

        // MPI related
        int numtasks, rank, len, rc;
        char hostname[MPI_MAX_PROCESSOR_NAME];

        MPI_Init(&argc, &argv);
        MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        MPI_Get_processor_name(hostname, &len);
        printf("Number of tasks= %d, My rank= %d, Running on %s\n", numtasks, rank, hostname);

        if(argc != 6) {
                printf("Usage: %s <job id> <input size> <parallel environment> <nslots> <nhosts>", argv[0]);
                return -1;
        }

        // Set up timing for ALL cores
        tstart = myclock(); // Set the zero time
        tstart = myclock(); // Start the clock
        tlast = tstart;

        // Malloc space for the word list and lines

        count = (int *) calloc( maxwords, sizeof( int ) );

        // Allocate node pools
        //allocateNodePools();

        // Contiguous memory for words
        wordmem = malloc(maxwords * MAX_KEYWORD_LENGTH * sizeof(char));
        word = (char **) malloc( maxwords * sizeof( char * ) );
        for(i = 0; i < maxwords; i++) {
                word[i] = wordmem + i * MAX_KEYWORD_LENGTH;
        }
/*
        // Allocate arrays for the heads and tails of the lists
        hithead = (struct Node**) malloc( maxwords * sizeof(struct Node *) );
        hittail = (struct Node**) malloc( maxwords * sizeof(struct Node *) );
        for( i = 0; i < maxwords; i++ ) {
                hithead[i] = hittail[i] = node_alloc();
        }
 */
        // Contiguous memory...yay
        linemem = malloc(maxlines * MAX_LINE_LENGTH * sizeof(char));
        line = (char **) malloc( maxlines * sizeof( char * ) );
        for( i = 0; i < maxlines; i++ ) {
                line[i] = linemem + i * MAX_LINE_LENGTH;
        }


        // Read in the dictionary words and sort them

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
                for(i = 0; i < maxwords; i++) {
                        for(k = 0; k < MAX_KEYWORD_LENGTH; k++) {
                                *(tempwordmem + i * MAX_KEYWORD_LENGTH + k) = word[i][k];
                        }
                        word[i] = wordmem + i * MAX_KEYWORD_LENGTH;
                }
                memcpy(wordmem, tempwordmem, maxwords * MAX_KEYWORD_LENGTH);
                free(tempwordmem); tempwordmem = NULL;
        }
        MPI_Bcast(&nwords, 1, MPI_INT, 0, MPI_COMM_WORLD);


        // Read in the lines from the data file

        //if(rank == 0)
        //{
                char *input_file = (char*)malloc(500 * sizeof(char));
                sprintf(input_file, WORKING_DIRECTORY WIKI_FILE, argv[2]);
                fd = fopen( input_file, "r" );
                // printf("\n------------------------------------\n"
                //        "\nERROR: Can't open wiki file.  Not found.\n"
                //        "\n------------------------------------\n");
                nlines = -1;
                do {
                        err = fscanf( fd, "%[^\n]\n", line[++nlines] );
                        if( line[nlines] != NULL ) nchars += (double) strlen( line[nlines] );
                } while( err != EOF && nlines < maxlines);
                fclose( fd );
                free(input_file); input_file = NULL;

                printf( "Read in %d lines averaging %.0lf chars/line\n", nlines, nchars / nlines);
        //}
//start
        //printf("test1\n");
        //MPI_Bcast(&nlines, 1, MPI_INT, 0, MPI_COMM_WORLD);
        //printf("test2\n");
//Division of work
        if(rank != 0) {
                start = (rank-1) * (nlines/(numtasks-1));
                end = rank  * (nlines/(numtasks-1));
                if(rank == numtasks - 1) end = nlines;
        }
        //printf("Rank: %i, Start: %i, End: %i\n", rank, start, end);

        MPI_Status status;
        //int lineNumbers[nwords][1];
        //int **lineNumbers = malloc(nwords * sizeof *lineNumbers + (nwords * (100 * sizeof(int))));
        int **lineNumbers;
        lineNumbers = malloc( nwords * sizeof( int * ) );
        for(i = 0; i < nwords; i++) {
                lineNumbers[i] = malloc(100 * (sizeof(int)));
        }

        int *keywordCount = (int*) malloc(nwords * sizeof(int));

        int *totalCount = (int*) malloc(nwords * sizeof(int));
        int receivedLineNumbers[100];
        int receivedKeywordCount = 0;

        int *hasFoundKeyword = (int*) malloc(nwords * sizeof(int));
        int index = 0;
        int it = 0;
        char *keyword = (char*) malloc((MAX_KEYWORD_LENGTH +1) * sizeof(char));
        tlast = myclock();
 //printf("Rank: %i entering loop\n", rank);
        while(1) {
                ////printf("[Rank %i] Loop\n", rank);
                if(rank != 0 ) {
                        ////printf("[Rank %i] asking for keyword #%i\n", rank, index);
                        //MPI_Sendrecv(&index, 1, MPI_INT, 0, 1, keyword, MAX_KEYWORD_LENGTH, MPI_CHAR, 0, 10, MPI_COMM_WORLD, &status);
                        MPI_Send(&index, 1, MPI_INT, 0, 1, MPI_COMM_WORLD); //tag 1 is for keyword
                        MPI_Recv(keyword, MAX_KEYWORD_LENGTH, MPI_CHAR, 0, 10, MPI_COMM_WORLD, &status);
                        ////printf("[Rank %i] got keyword: %s\n", rank, keyword);
                        for(i = start; i < end; i++) {
                                if(strstr(line[i], keyword) != NULL) {
                                        lineNumbers[index][keywordCount[index]] = i;
                                        if (keywordCount[index] < 99) {
                                          keywordCount[index]++;
                                        }
                                }
                                ////printf("[Rank %i] forloop\n", rank);
                        }


                        index++;
                        if(index == nwords) {
                                //printf("[Rank %i] Finished looking for all keywords, preparing to send to rank 0 for printing.\n", rank);
                                // All keywords shoudld be counted, time to send back data to 0 to print
                                for (i = 0; i < nwords; i++) {
                                        MPI_Ssend(&keywordCount[i], 1, MPI_INT, 0, 20, MPI_COMM_WORLD);
                                        if (keywordCount[i]) {
                                            MPI_Ssend(lineNumbers[i], keywordCount[i], MPI_INT, 0, 15, MPI_COMM_WORLD);
                                        }
                                }
                                break;
                        }
                }



                if (rank== 0) {
                        MPI_Recv(&index, 1, MPI_INT, MPI_ANY_SOURCE, 1, MPI_COMM_WORLD, &status);
                        MPI_Send(word[index], strlen(word[index])+1, MPI_CHAR, status.MPI_SOURCE, 10, MPI_COMM_WORLD);

                        it++;
                        if (it == nwords * (numtasks-1)) {
                                //printf("[Rank %i] Entering print loop.\n", rank);
                                for (i = 0; i < nwords; i++) { // For each keyword
                                        for (j = 1; j < numtasks; j++) { // For each Node
                                                //printf("[Rank %i] Waiting for counts from rank %i.\n", rank, j);
                                                MPI_Recv(&receivedKeywordCount, 1, MPI_INT, j, 20, MPI_COMM_WORLD, &status); // tag 20 for keywordcount
                                                if (receivedKeywordCount) {
                                                        MPI_Recv(&receivedLineNumbers, 100, MPI_INT, j, 15, MPI_COMM_WORLD, &status); // tag 15 for line numbers
                                                        //printf("[Rank %i] Received from rank %i keywordcount of %i.\n",rank, j, receivedKeywordCount);
                                                        if (!hasFoundKeyword[i]) {
                                                                //printf("[Rank %i] Found Keyword... Printing\n", rank);
                                                                printf("\b\b \n%s: ", word[i]);
                                                                hasFoundKeyword[i] = 1;
                                                        }
                                                        for (k = 0; k < receivedKeywordCount; k++) {
                                                                printf("%i, ", receivedLineNumbers[k]);
                                                        }
                                                }
                                        }
                                }

                                break;
                        }
                }

        }

        MPI_Barrier(MPI_COMM_WORLD);

        if(rank == 0)
        {
                ttotal = myclock() - tstart;
        }

        // Clean up after ourselves

        // Words
        free(word);     word = NULL;
        free(wordmem);  wordmem = NULL;

        // Lines
        free(line);     line = NULL;
        free(linemem);  linemem = NULL;

        // free(keywordCount); keywordCount = NULL;
        // free(totalCount); totalCount = NULL;
        // free(receivedLineNumbers); receivedLineNumbers = NULL;
        // free(receivedKeywordCount); receivedKeywordCount = NULL;
        // free(lineNumbers); lineNumbers = NULL;



        if(rank == 0)
        {
                printf("priniting data\n");
                sleep(1);
                fflush(stdout);
                printf( "DATA\t%lf\t%d\t%d\t%s\t%s\t%s\t%s\n",
                        ttotal, nwords, nlines, argv[2], argv[3], argv[4], argv[5]);
                fflush(stdout);
                printf("IT WORKED!\n");
        }

        MPI_Finalize();
        printf("Finalize WORKED!\n");
        return 0;
}

double myclock() {
        static time_t t_start = 0; // Save and subtract off each time

        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        if( t_start == 0 ) t_start = ts.tv_sec;

        return (double) (ts.tv_sec - t_start) + ts.tv_nsec * 1.0e-9;
}
