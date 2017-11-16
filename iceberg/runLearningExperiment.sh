#!/bin/bash

# for type in "DFSM" "Mealy" "Moore" "DFA"; do
#    for N in 10 20 30 40 50 60 70 80 90 100 150 200 300 400 600 800 1000; do
#	qsub -v MT=$type,STATES=$N testingExperiment.sh
#    done
# done

for type in "DFSM" "Mealy" "Moore" "DFA"; do
    for N in 10 20 30 40 50 60 70 80 90 100; do # 150 200 300 400 600 800 1000
	qsub -v MT=$type,STATES=$N,ALG=127 learningExperiment.sh
    done
done

#qsub -v MT="DFA",STATES=70,OUT_ID="_1",FNS="DFA_R70_10_AQblq.fsm",FNE="a" testingExperiment.sh
#qsub -v MT="DFA",STATES=80,OUT_ID="_1",FNS="DFA_R80_10_XvTRH.fsm",FNE="a" testingExperiment.sh
#qsub -v MT="Mealy",STATES=80,OUT_ID="_1",FNS="Mealy_R80_10_OxDt4.fsm",FNE="a" testingExperiment.sh

#for type in "DFSM"; do
#    for N in 600 400 300 200; do
#	for filename in /data/acp15ms/exp/${type}/${type}_R${N}*.fsm; do
#	    if ! grep -q $(basename "${filename}") /data/acp15ms/exp/testing${type}_${N}.csv
#	    then
#		echo $(basename "${filename}") # code if not found
#		qsub -v MT=$type,STATES=${N},FN=$(basename "${filename}") testingExperimentFN.sh
#	    fi
#	done
#    done
#done

#for type in "Mealy" "DFA"; do
#    for N in 100 90; do
#	for filename in /data/acp15ms/exp/${type}/${type}_R${N}*.fsm; do # 100 matches even 1000
#	    if ! grep -q $(basename "${filename}") /data/acp15ms/exp/testing${type}_${N}.csv
#	    then
#		echo $(basename "${filename}") # code if not found
#		qsub -v MT=$type,STATES=${N},FN=$(basename "${filename}") testingExperimentFN.sh
#	    fi
#	done
#    done
#done

#qsub -v MT="DFSM",STATES=150,OUT_ID="_1",FNS="DFSM_R150_10_hAn4o.fsm",FNE="a" testingExperiment.sh
#qsub -v MT="DFSM",STATES=200,OUT_ID="_1",FNS="DFSM_R200_10_w3ZO2.fsm",FNE="a" testingExperiment.sh
#qsub -v MT="DFSM",STATES=300,OUT_ID="_1",FNS="DFSM_R300_10_SZZFL.fsm",FNE="a" testingExperiment.sh
#qsub -v MT="DFSM",STATES=400,OUT_ID="_1",FNS="DFSM_R400_10_LJfTe.fsm",FNE="a" testingExperiment.sh
#qsub -v MT="DFSM",STATES=600,OUT_ID="_1",FNS="DFSM_R600_10_dbqig.fsm",FNE="a" testingExperiment.sh
#qsub -v MT="DFSM",STATES=800,OUT_ID="_2",FNS="DFSM_R800_10.fsm",FNE="a" testingExperiment.sh
#qsub -v MT="DFSM",STATES=1000,OUT_ID="_1",FNS="DFSM_R1000_5_GsHYe.fsm",FNE="DFSM_R1000_10.fsm" testingExperiment.sh
#qsub -v MT="DFSM",STATES=1000,OUT_ID="_2",FNS="DFSM_R1000_10.fsm",FNE="a" testingExperiment.sh

#qsub -v MT="Mealy",STATES=70,OUT_ID="_1",FNS="Mealy_R70_10_l5u7w.fsm",FNE="a" testingExperiment.sh
#qsub -v MT="Mealy",STATES=80,OUT_ID="_1",FNS="Mealy_R80_10_1KHod.fsm",FNE="a" testingExperiment.sh
#qsub -v MT="Mealy",STATES=90,OUT_ID="_1",FNS="Mealy_R90_10_i58zQ.fsm",FNE="a" testingExperiment.sh
#qsub -v MT="Mealy",STATES=100,OUT_ID="_1",FNS="Mealy_R100_10_7GjRV.fsm",FNE="a" testingExperiment.sh

#qsub -v MT="Moore",STATES=150,OUT_ID="_1",FNS="Moore_R150_10_p8SCy.fsm",FNE="a" testingExperiment.sh
#qsub -v MT="Moore",STATES=200,OUT_ID="_1",FNS="Moore_R200_10_SqTh7.fsm",FNE="a" testingExperiment.sh
#qsub -v MT="Moore",STATES=300,OUT_ID="_1",FNS="Moore_R300_10_6MgeS.fsm",FNE="a" testingExperiment.sh
#qsub -v MT="Moore",STATES=400,OUT_ID="_1",FNS="Moore_R400_10_SxxPT.fsm",FNE="a" testingExperiment.sh
#qsub -v MT="Moore",STATES=600,OUT_ID="_2",FNS="Moore_R600_10.fsm",FNE="a" testingExperiment.sh
#qsub -v MT="Moore",STATES=800,OUT_ID="_1",FNS="Moore_R800_5_JErcz.fsm",FNE="Moore_R800_10.fsm" testingExperiment.sh
#qsub -v MT="Moore",STATES=800,OUT_ID="_2",FNS="Moore_R800_10.fsm",FNE="a" testingExperiment.sh
#qsub -v MT="Moore",STATES=1000,OUT_ID="_1",FNS="Moore_R1000_5_giYQy.fsm",FNE="Moore_R1000_10.fsm" testingExperiment.sh
#qsub -v MT="Moore",STATES=1000,OUT_ID="_2",FNS="Moore_R1000_10.fsm",FNE="a" testingExperiment.sh

#qsub -v MT="DFA",STATES=90,OUT_ID="_1",FNS="DFA_R90_10_YsCA8.fsm",FNE="a" testingExperiment.sh
#qsub -v MT="DFA",STATES=100,OUT_ID="_1",FNS="DFA_R100_10_ovaOd.fsm",FNE="a" testingExperiment.sh
#qsub -v MT="DFA",STATES=150,OUT_ID="_1",FNS="DFA_R150_10_dgzeV.fsm",FNE="a" testingExperiment.sh

