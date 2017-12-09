#!/bin/bash

# for type in "DFSM" "Mealy" "Moore" "DFA"; do
#    for N in 10 20 30 40 50 60 70 80 90 100 150 200 300 400 600 800 1000; do
#	qsub -v MT=$type,STATES=$N testingExperiment.sh
#    done
# done

for type in "DFSM"; do
    for N in 1000 800 600 400 300 200; do
	for P in 10; do
	    fileCVS=/data/acp15ms/exp/testing${type}_${N}_${P}.csv
	    rm -f ${fileCVS}
	    incompFN=/data/acp15ms/exp/testing${type}_${N}_${P}_todo.txt
	    rm -f ${incompFN}
	    #echo "line" >> $fileCVS
	    for filename in /data/acp15ms/exp/FN/testing${type}_R${N}_${P}*.csv; do
		c=`wc -l < ${filename}`
		if [ "$c" -eq "37" ]; then
		    if [ -f $fileCVS ] ; then
			sed 1d ${filename} | cat >> "$fileCVS"
		    else
			cat "${filename}" >> "$fileCVS"
		    fi
		    #c=0
		else
		    echo $c $(basename "${filename}") >> "${incompFN}"
		fi
#echo $(basename "${filename}") # code if not found
		#sed -i -e '$a\' -e ${filename} /data/acp15ms/exp/testing${type}_${N}_${P}.csv
#		echo ${filename} >> /data/acp15ms/exp/testing${type}_${N}_${P}.csv
		#wc -l ${filename}
	    done
	    wc -l ${fileCVS}
	    if [ -f ${incompFN} ]; then
		wc -l ${incompFN}
	    else
		echo "no incomplete tests"
	    fi
	done
    done
done

