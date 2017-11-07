#!/bin/bash

# for type in "DFSM" "Mealy" "Moore" "DFA"; do
#    for N in 10 20 30 40 50 60 70 80 90 100 150 200 300 400 600 800 1000; do
#	qsub -v MT=$type,STATES=$N testingExperiment.sh
#    done
# done

for type in "DFSM"; do
    for N in 1000; do
	for P in 5; do
	    for filename in /data/acp15ms/exp/FN/${type}_R${N}_${P}*.csv; do
		#echo $(basename "${filename}") # code if not found
		wc -l ${filename} 
		#if ! grep -q $(basename "${filename}") /data/acp15ms/exp/testing${type}_${N}.csv
		#then
		 #   qsub -v MT=$type,STATES=${N},FN=$(basename "${filename}") testingExperimentFN.sh
		#fi
	    done
	done
    done
done

