#!/bin/bash
# Request 16 gigabytes of real memory (RAM)
#$ -l rmem=16G -l mem=16G
#$ -l arch=intel-e5-2650v2
# Email notifications
#$ -M michal.soucha@gmail.com
# Email notifications if the job aborts
#$ -m ea
#$ -j y
#$ -l h_rt=48:00:00

# Load the modules required by our program
module load compilers/gcc/6.2

./x64/FSMlibRun.exe 4 "/data/acp15ms/exp/${MT}/" -o "/data/acp15ms/exp/learning${MT}_${STATES}_${ALG}.csv" -a ${ALG} -s ${STATES} > out/learning${MT}_${STATES}.out

