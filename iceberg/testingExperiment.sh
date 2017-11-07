#!/bin/bash
# Request 16 gigabytes of real memory (RAM)
#$ -l rmem=16G -l mem=16G
#$ -l arch=intel-e5-2650v2
# Email notifications
#$ -M michal.soucha@gmail.com
# Email notifications if the job aborts
#$ -m ea
#$ -j y

# Load the modules required by our program
module load compilers/gcc/6.2

./x64/FSMlibTesting.exe 3 "/data/acp15ms/exp/${MT}/" -o "/data/acp15ms/exp/testing${MT}_${STATES}${OUT_ID}.csv" -co 0 -at 1 -ts 1020 -s ${STATES} -fstart ${FNS} -fend ${FNE} > out/testing${MT}_${STATES}${OUT_ID}.out

#./x64/FSMlibTesting3.exe 3 "/data/acp15ms/exp/${MT}/" -o "/data/acp15ms/exp/testing${MT}_${STATES}${OUT_ID}.csv" -co 0 -at 1 -ts 768 -s ${STATES} -i 10 -esmin 2 > out/testing${MT}_${STATES}${OUT_ID}.out