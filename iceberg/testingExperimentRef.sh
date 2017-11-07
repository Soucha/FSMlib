#!/bin/bash
# Request 16 gigabytes of real memory (RAM)
#$ -l rmem=32G -l mem=64G
#$ -l arch=intel-e5-2650v2
# Email notifications
#$ -M michal.soucha@gmail.com
# Email notifications if the job aborts
#$ -m ea
#$ -j y

# Load the modules required by our program
module load compilers/gcc/6.2

./x64/FSMlibTesting3.exe 3 "/data/acp15ms/ref/" -o "/data/acp15ms/ref/testing${FN}.csv" -co 0 -at 1 -ts 1016 -f ${FN} -esmin 1 > out/testing${FN}.out