#!/bin/bash

# for type in "DFSM" "Mealy" "Moore" "DFA"; do
#    for N in 10 20 30 40 50 60 70 80 90 100 150 200 300 400 600 800 1000; do
#	qsub -v MT=$type,STATES=$N testingExperiment.sh
#    done
# done

for type in "DFSM"; do
    for N in 1000 800; do
	for P in 10; do
	    fileCVS=/data/acp15ms/exp/testing${type}_${N}_${P}.csv
	    rm -f ${fileCVS}
	    #echo "line" >> $fileCVS
	    for filename in /data/acp15ms/exp/FN/testing${type}_R${N}_${P}*.csv; do
		if [ -f $fileCVS ] ; then
		    sed 1d ${filename} | cat >> "$fileCVS"
		else
		    cat "${filename}" >> "$fileCVS"
		fi
#echo $(basename "${filename}") # code if not found
		#sed -i -e '$a\' -e ${filename} /data/acp15ms/exp/testing${type}_${N}_${P}.csv
#		echo ${filename} >> /data/acp15ms/exp/testing${type}_${N}_${P}.csv
		#wc -l ${filename}
	    done
	    wc -l /data/acp15ms/exp/testing${type}_${N}_${P}.csv
	done
    done
done

