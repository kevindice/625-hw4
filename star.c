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
        int nwords, maxwords = 5000; //add again
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

        if(rank == 0)
        {
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
        }
//start
        //printf("test1\n");
        MPI_Bcast(&nlines, 1, MPI_INT, 0, MPI_COMM_WORLD);
        //printf("test2\n");
//Division of work
        if(rank != 0) {
                start = (rank-1) * (nlines/(numtasks-1));
                end = rank  * (nlines/(numtasks-1));
                if(rank == numtasks - 1) end = nlines;
        }
        //printf("Rank: %i, Start: %i, End: %i\n", rank, start, end);

        MPI_Status status;
        int lineNumbers[nwords][100];
        int keywordCount[nwords];

        int totalCount[nwords];
        int receivedLineNumbers[100];
        int receivedKeywordCount = 0;
        int hasFoundKeyword[100];
        int index = 0;
        int it = 0;
        char *keyword = (char*) malloc((MAX_KEYWORD_LENGTH +1) * sizeof(char));
        tlast = myclock();

        while(1) {
                //printf("[Rank %i] Loop\n", rank);
                if(rank != 0 ) {
                        printf("[Rank %i] asking for keyword #%i\n", rank, index);
                        //MPI_Sendrecv(&index, 1, MPI_INT, 0, 1, keyword, MAX_KEYWORD_LENGTH, MPI_CHAR, 0, 10, MPI_COMM_WORLD, &status);
                        MPI_Send(&index, 1, MPI_INT, 0, 1, MPI_COMM_WORLD); //tag 1 is for keyword
                        MPI_Recv(keyword, MAX_KEYWORD_LENGTH, MPI_CHAR, 0, 10, MPI_COMM_WORLD, &status);
                        printf("[Rank %i] got keyword: %s\n", rank, keyword);
                        for(i = start; i < end; i++) {
                                if(strstr(line[i], keyword) != NULL) {
                                        lineNumbers[index][keywordCount[index]] = i;
                                        keywordCount[index]++;
                                }
                                //printf("[Rank %i] forloop\n", rank);
                        }


                        index++;
                        if(index == nwords) {
                                printf("[Rank %i] Finished looking for all keywords, preparing to send to rank 0 for printing.\n", rank);
                                // All keywords shoudld be counted, time to send back data to 0 to print
                                for (i = 0; i < nwords; i++) {
                                        printf("[Rank %i] Sending count for keyword #%i (%i).\n", rank, i, keywordCount[i]);
                                        printf("[Rank %i] \n", rank);
                                        MPI_Ssend(&lineNumbers[i], 100, MPI_INT, 0, 15, MPI_COMM_WORLD);
                                        MPI_Ssend(&keywordCount[i], 1, MPI_INT, 0, 20, MPI_COMM_WORLD);
                                }
                                break;
                        }
                }



                if (rank== 0) {
                        //memset(finalLineNum, 0, sizeof(int)*100);
                        //memset(lineNumbers, 0, sizeof(int)*100);
                        MPI_Recv(&index, 1, MPI_INT, MPI_ANY_SOURCE, 1, MPI_COMM_WORLD, &status);
                        printf("[Rank %i] request from rank %i for keyword #%i\n", rank, status.MPI_SOURCE, index);
                        MPI_Send(word[index], strlen(word[index])+1, MPI_CHAR, status.MPI_SOURCE, 10, MPI_COMM_WORLD);
                        printf("[Rank %i] sent keyword: %s\n", rank, word[index]);
                        //totalCount = 0;

                        it++;
                        if (it == nwords * (numtasks-1)) {
                                printf("[Rank %i] Entering print loop.\n", rank);
                                for (i = 0; i < nwords; i++) { // For each keyword
                                        for (j = 1; j < numtasks; j++) { // For each Node
                                                MPI_Recv(receivedLineNumbers, 100, MPI_INT, j, 15, MPI_COMM_WORLD, &status); // tag 15 for line numbers
                                                MPI_Recv(&receivedKeywordCount, 1, MPI_INT, j, 20, MPI_COMM_WORLD, &status); // tag 20 for keywordcount
                                                if (!hasFoundKeyword[i] && receivedKeywordCount) {
                                                        printf("%s: ", word[i]);
                                                        hasFoundKeyword[i] = 1;
                                                }
                                                for (k = 0; k < receivedKeywordCount; k++) {
                                                        printf("%i, ",receivedLineNumbers[k]);
                                                }
                                                // Print the backspace things
                                        }
                                }

                                break;
                        }
                }

        }


        //printf("IT WORKED!");
        /*MPI_Bcast(&nwords, 1, MPI_INT, 0, MPI_COMM_WORLD);
           MPI_Bcast(&nlines, 1, MPI_INT, 0, MPI_COMM_WORLD);
           MPI_Bcast(wordmem, nwords * MAX_KEYWORD_LENGTH, MPI_CHAR, 0, MPI_COMM_WORLD);
           MPI_Bcast(linemem, nlines * MAX_LINE_LENGTH, MPI_CHAR, 0, MPI_COMM_WORLD);

           if(rank == 0)
           {
           printf("Read in and MPI comm overhead for %d lines and %d procs = %lf seconds\n", nlines, numtasks, myclock() - tlast);
           printf("OHEAD\t%d\t%d\t%lf\t%s\t%s", nlines, numtasks, myclock() -tlast, argv[3], argv[5]);
           printf("\n******  Starting work  ******\n\n");
           }


           //

           printf("------- Proc: %d, Start: %d, End: %d, Nwords: %d, Num tasks: %d --------\n", rank, start, end, nwords, numtasks);


           // Loop over the word list
           for( k = 0; k < nlines; k++ ) {
           for( i = start; i < end; i++ ) {
           if( strstr( line[k], word[i] ) != NULL ) {
            count[i]++;
            hittail[i] = add(hittail[i], k);
           }
           }
           }

           printf("\n\nPART_DONE\trank %d\tafter %lf seconds\twith %s slots\ton %s hosts\twith pe %s\n",
           rank, myclock() - tlast, argv[4], argv[5], argv[3]); fflush(stdout);

           // Dump out the word counts
         */
        /*char *output_file = (char*) malloc(500 * sizeof(char));
           sprintf(output_file, WORKING_DIRECTORY OUTPUT_FILE, argv[1], rank);

           fd = fopen( output_file, "w" );
           for( i = start; i < end; i++ ) {
                if(count[i] != 0) {
                        fprintf( fd, "%s: ", word[i] );
                        int *line_numbers;
                        int len;
                        // this function mallocs for line_numbers
                        toArray(hithead[i], &line_numbers, &len);

                        for (k = 0; k < len - 1; k++) {
                                fprintf( fd, "%d, ", line_numbers[k]);
                        }
                        fprintf( fd, "%d\n", line_numbers[len - 1]);

                        // so we free it
                        free(line_numbers);  line_numbers = NULL;
                }
           }
           fclose( fd );

           free(output_file);  output_file = NULL;

           // Take end time when all are finished writing the file

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
         */
        MPI_Barrier(MPI_COMM_WORLD);

        if(rank == 0)
        {
                ttotal = myclock() - tstart;
        }

        // Clean up after ourselves

/*
        // Linked list counts
        destroyNodePools();
        free(hithead);  hithead = NULL;
        free(hittail);  hittail = NULL;
 */
        // Words
        free(word);     word = NULL;
        free(wordmem);  wordmem = NULL;

        // Lines
        free(line);     line = NULL;
        free(linemem);  linemem = NULL;

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
