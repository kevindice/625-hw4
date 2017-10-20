declare -A timemap
timemap[1]="4:45:00"
timemap[2]="6:00:00"
timemap[4]="4:20:00"
timemap[8]="4:00:00"
timemap[16]="4:40:00"
timemap[20]="4:35:00"
timemap[32]="4:28:00"
timemap[48]="0:24:00"
timemap[64]="0:20:00"

for x in 2 4 8 16
do
echo "qsub -N 'star' -pe mpi-fill $x -l h_rt=${timemap[$x]} mpi-run.sh"
done;
