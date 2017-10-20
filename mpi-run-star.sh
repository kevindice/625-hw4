#!/bin/bash
#$ -l mem=8G
#$ -q \*@@elves
#$ -cwd
#$ -j y
#$ -m abe
#$ -M cbald24@ksu.edu,walkerg@ksu.edu


# Iterate through input sizes (from 10^3 up to 10^6)
for x in 6 5 4 3
do
    mpirun -np $NSLOTS /homes/cbald24/HW4/star "${JOB_ID}_${x}" $x "$PE" $NSLOTS $NHOSTS
done
