#!/bin/bash
#$ -l mem=3G
#$ -q \*@@elves
#$ -cwd
#$ -j y
#$ -m abe
#$ -M kevin.dice1@gmail.com,walkerg@ksu.edu


# Iterate through input sizes (from 10^3 up to 10^6)
for x in 6 5 4 3
do
    mpirun -np $NSLOTS /homes/walkerg/CIS625/homework4/ring "${JOB_ID}_${x}" $x "$PE" $NSLOTS $NHOSTS
done
