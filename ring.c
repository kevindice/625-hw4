#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <mpi.h>

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

        // MPI related
        int numtasks, rank, len, rc;
        char hostname[MPI_MAX_PROCESSOR_NAME];

        MPI_Init(&argc, &argv);
        MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        MPI_Get_processor_name(hostname, &len);
        printf("Number of tasks= %d, My rank= %d, Running on %s\n", numtasks, rank, hostname);

        if(argc != 6) {
                printf("Usage: %s <job id> <input size> <parallel environment> <nslots> <nhosts>\n", argv[0]);
                return -1;
        }

        // Set up timing for ALL cores
        tstart = myclock(); // Set the zero time
        tstart = myclock(); // Start the clock
        tlast = tstart;

        // Contiguous memory for words (only need this on rank 0)
        if (rank == 0) {
                wordmem = malloc(maxwords * MAX_KEYWORD_LENGTH * sizeof(char));
                word = (char **) malloc( maxwords * sizeof( char * ) );
                for(i = 0; i < maxwords; i++) {
                        word[i] = wordmem + i * MAX_KEYWORD_LENGTH;
                }
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
        char *keyword = (char*) malloc((MAX_KEYWORD_LENGTH + 1) * sizeof(char));

        // Initialize it
        keyword[0] = '\0';

        int keywordCount = 0;
        int keywordIterator = 0;
        MPI_Status status;
        int lineNumbers[100];
        int flag = 0;
        int receivedKeywords = 0;

        //printf("[Rank %i] Entering main loop.\n", rank);
        // Create a loop for doing the work
        while (1) {
                // If we are rank 0, get the keyword from the array we can access.
                if (rank == 0) {
                        keyword = word[keywordIterator];
                        keywordCount = 0;

                // If we aren't, wait for the keyword to come from the previous rank
                } else {
                        // Not rank 0, so listen for keyword using tag 1
                        MPI_Recv(keyword, MAX_KEYWORD_LENGTH, MPI_CHAR, previous, 1, MPI_COMM_WORLD, &status);

                        // Listen for the count using tag 2
                        MPI_Recv(&keywordCount, 1, MPI_INT, previous, 2, MPI_COMM_WORLD, &status);

                        // Listen for line number array using tag 3
                        MPI_Recv(&lineNumbers, 100, MPI_INT, previous, 3, MPI_COMM_WORLD, &status);
                        //printf("[Rank %i] Received keyword '%s' from rank %i with %i found so far.\n", rank, keyword, previous, keywordCount);
                }

                //printf("[Rank %i] Searching for keyword '%s' between lines %i and %i\n", rank, keyword, start, end);
                // Search through the set for the current rank, searching for the keyword
                for( i = start; i < end; i++ ) {
                        if( strstr( line[i], keyword ) != NULL ) {
                                // Store the line number
                                lineNumbers[keywordCount] = i;
                                // Increase counter
                                keywordCount++;
                        }
                }
                //printf("[Rank %i] Found keyword '%s' %i times. Sending this to rank %i.\n", rank, keyword, keywordCount, next);

                // Send the keyword to the next process using tag 1
                MPI_Ssend(keyword, strlen(keyword) + 1, MPI_CHAR, next, 1, MPI_COMM_WORLD);

                // Send the count to the next process using tag 2
                MPI_Ssend(&keywordCount, 1, MPI_INT, next, 2, MPI_COMM_WORLD);

                // Send the lines found to the next process using tag 3
                MPI_Ssend(&lineNumbers, 100, MPI_INT, next, 3, MPI_COMM_WORLD);

                // If we are rank 0 and we have
                //if (rank == 0) printf("\n\nkeywordIterator: %i, receivedKeywords: %i, numtasks: %i\n\n", keywordIterator, receivedKeywords, numtasks);
                if (rank == 0 && ((keywordIterator - receivedKeywords >= (numtasks - 2)))) {
                        //printf("[Rank %i] BLOCKING until receive from rank %i\n", rank, previous);
                        MPI_Recv(keyword, MAX_KEYWORD_LENGTH, MPI_CHAR, previous, 1, MPI_COMM_WORLD, &status);
                        MPI_Recv(&keywordCount, 1, MPI_INT, previous, 2, MPI_COMM_WORLD, &status);
                        MPI_Recv(&lineNumbers, 100, MPI_INT, previous, 3, MPI_COMM_WORLD, &status);
                        //printf("[Rank %i] Printing '%s' received from rank %i.\n", rank, keyword, previous);
                        receivedKeywords++;

                        // Print the stuff
                        if (keywordCount) {
                                printf("%s: ", keyword);
                                i = 0;
                                while (keywordCount) {
                                        printf("%i, ", lineNumbers[i]);
                                        keywordCount--;
                                        i++;
                                }
                                // Get rid of the last comma & space
                                printf("\b\b \n");
                        }
                }

                // If this was the last keyword
                if (keywordIterator >= nwords - 1) {
                        // break the Loop
                        break;
                }

                keywordIterator++;
                // Clear the keyword for next Loop
                keyword[0] = '\0';
        }
        //printf("[Rank %i] Leaving main loop.\n", rank);

        if (rank == 0) MPI_Abort(MPI_COMM_WORLD, 0);

        printf("------- Proc: %d, Start: %d, End: %d, Nwords: %d, Num tasks: %d --------\n", rank, start, end, nwords, numtasks);

        printf("\n\nPART_DONE\trank %d\tafter %lf seconds\twith %s slots\ton %s hosts\twith pe %s\n",
               rank, myclock() - tlast, argv[4], argv[5], argv[3]); fflush(stdout);

        // Take end time when all are finished
        MPI_Barrier(MPI_COMM_WORLD);

        if(rank == 0)
        {
                ttotal = myclock() - tstart;
        }


        // Clean up after ourselves

        // Words
        free(word);     word = NULL;
        if (rank == 0) {
                free(wordmem);  wordmem = NULL;
        }

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
        static time_t t_start = 0; // Save and subtract off each time

        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        if( t_start == 0 ) t_start = ts.tv_sec;

        return (double) (ts.tv_sec - t_start) + ts.tv_nsec * 1.0e-9;
}
