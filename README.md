# CIS 625 HW4

Homework 4: Getting fancier with distributed programming

Implimenting our keyword search program with the following communication paradigms:

1. Star/centralized
1. Ring
1. Work Queue

## Tasks

1. *Star/centralized* - Designate rank 0 as the source of keywords.  Divide the set of lines among the worker processes.  Each worker process will request the next individual keyword from rank 0, search for matches in its Wikipedia entries, and return an array listing any line numbers the keyword appears in (up to 100).  Rank 0 should print out all the results.
1. *Ring* – the same idea, but pass the keyword from process n to process rank n+1, updating a count of the total times the word appears. When it gets back to rank 0, print out the word and the total, if the total is greater than 0.
1. *Work queue* – Batch up the keywords into groups of 100 on rank 0 as you read them in from the file. Put the batched keywords on a list/queue of tasks to be completed. Each worker spins, requesting the next batch of keywords from Rank 0, completing the search from that batch, and at the end of the program run returns the results (up to 100 appearances per keyword) to rank 0 for printing out.

Plot the CPU time expended by the various versions of the program running with 2, 4, 8, and 16 threads (feel free to combine these into one graph). Note that the x axis of your plot should be threads used, and the y-axis of your plot should be time expended in seconds. Discuss the results. Are there any race conditions? How much communication (in terms of # of messages and bytes/message) is there per variant?

## Getting Started

First we need to generate the input files of various size.

If on beocat: `sh generate-test-files`

If on your local machine (assuming you already have test10-6.txt: `sh local-generate-test-files.sh`

## Resources

 - MPI - [https://computing.llnl.gov/tutorials/mpi/](https://computing.llnl.gov/tutorials/mpi/)
 - Beocat Docs - [https://support.beocat.ksu.edu/BeocatDocs/index.php/Main_Page](https://support.beocat.ksu.edu/BeocatDocs/index.php/Main_Page)
 - Compute Node Specs - [https://support.beocat.ksu.edu/BeocatDocs/index.php/Compute_Nodes](https://support.beocat.ksu.edu/BeocatDocs/index.php/Compute_Nodes)


