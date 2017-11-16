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

./x64/FSMlibRun.exe 2 "/data/acp15ms/exp/${MT}/" -o "/data/acp15ms/exp/ma${MT}.csv" > out/ma${MT}.out

