#!/bin/bash

for type in "DFSM" "Mealy" "Moore" "DFA"; do
    for N in 10 20 30 40 50 60 70 80 90 100 150 200 300 400 600 800 1000; do
	qsub -v MT=$type,STATES=$N HSIExperiment.sh
    done
done

