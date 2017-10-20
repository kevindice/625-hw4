declare -A timemap
timemap[1]="2:45:00"
timemap[2]="2:20:00"
timemap[4]="1:20:00"
timemap[8]="1:00:00"
timemap[16]="0:40:00"
timemap[20]="0:35:00"
timemap[32]="0:28:00"
timemap[48]="0:24:00"
timemap[64]="0:20:00"

for x in 2 4 8 16
do
echo "qsub -N 'ring' -pe mpi-fill $x -l h_rt=${timemap[$x]} mpi-run.sh"
done;
