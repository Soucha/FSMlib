/* Copyright (c) Michal Soucha, 2016
*
* This file is part of FSMlib
*
* FSMlib is free software: you can redistribute it and/or modify it under
* the terms of the GNU General Public License as published by the Free Software
* Foundation, either version 3 of the License, or (at your option) any later
* version.
*
* FSMlib is distributed in the hope that it will be useful, but WITHOUT ANY
* WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
* A PARTICULAR PURPOSE. See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with
* FSMlib. If not, see <http://www.gnu.org/licenses/>.
*/

#include <iostream>

#ifndef PARALLEL_COMPUTING
//#define PARALLEL_COMPUTING // un/comment this if CUDA is enabled/disabled
#endif // !PARALLEL_COMPUTING
#include "../FSMlib/FSMlib.h"

#define DATA_PATH			"../data/"
#define MINIMIZATION_DIR	string("tests/minimization/")
#define SEQUENCES_DIR		string("tests/sequences/")
#define EXAMPLES_DIR		string("examples/")

DFSM * fsm, *fsm2;
wchar_t message[200];

using namespace FSMsequence;
using namespace FSMtesting;

static void printSeqSet(sequence_set_t& seqSet) {
	for (auto seq : seqSet) {
		cout << FSMmodel::getInSequenceAsString(seq) << endl;
	}
	cout << endl;
}

int getCSet() {
	Moore moore;
	fsm = &moore;
	if (!fsm->load("../data/tests/sequences/Moore_R6_ADS.fsm")) return 1;
	sequence_set_t outCSet;
	cout << "filter prefix: YES, no reduction" << endl;
	outCSet = getCharacterizingSet(fsm);
	printSeqSet(outCSet);
	outCSet.clear();
	cout << "filter prefix: NO, no reduction" << endl;
	outCSet = getCharacterizingSet(fsm, getStatePairsShortestSeparatingSequences, false);
	printSeqSet(outCSet);
	outCSet.clear();
	cout << "filter prefix: YES, LS_SL" << endl;
	outCSet = getCharacterizingSet(fsm, getStatePairsShortestSeparatingSequences, true, reduceCSet_LS_SL);
	printSeqSet(outCSet);
	outCSet.clear();
	cout << "filter prefix: NO, LS_SL" << endl;
	outCSet = getCharacterizingSet(fsm, getStatePairsShortestSeparatingSequences, false, reduceCSet_LS_SL);
	printSeqSet(outCSet);
	outCSet.clear();
	cout << "filter prefix: YES, EqualLength" << endl;
	outCSet = getCharacterizingSet(fsm, getStatePairsShortestSeparatingSequences, true, reduceCSet_EqualLength);
	printSeqSet(outCSet);
	outCSet.clear();
	cout << "filter prefix: NO, EqualLength" << endl;
	outCSet = getCharacterizingSet(fsm, getStatePairsShortestSeparatingSequences, false, reduceCSet_EqualLength);
	printSeqSet(outCSet);
	outCSet.clear();
	return 0;
}

void testGetSeparatingSequences(string filename, sequence_vec_t(*getSepSeq)(DFSM * dfsm), string name) {
	if (!filename.empty()) fsm->load(filename);
	auto seq = (*getSepSeq)(fsm);
	printf("Separating sequences (%s) of %s:\n", name.c_str(), filename.c_str());
	vector<state_t> states = fsm->getStates();
	state_t k = 0, N = fsm->getNumberOfStates();
	for (state_t i = 0; i < N - 1; i++) {
		for (state_t j = i + 1; j < N; j++, k++) {
			printf("%d[%d] x %d[%d]: %s\n", states[i], i, states[j], j, FSMmodel::getInSequenceAsString(seq[k]).c_str());
		}
	}
}

#define PTRandSTR(f) f, #f

void groupTest(string filename) {
	testGetSeparatingSequences(filename, PTRandSTR(getStatePairsShortestSeparatingSequences));
#ifdef PARALLEL_COMPUTING
	testGetSeparatingSequences(filename, PTRandSTR(getStatePairsShortestSeparatingSequences_ParallelSF));
	testGetSeparatingSequences(filename, PTRandSTR(getStatePairsShortestSeparatingSequences_ParallelQueue));
#endif
}

void testSepSeqs() {
	Mealy mealy;
	fsm = &mealy;

	groupTest(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_ADS.fsm");
	groupTest(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_HS.fsm");
	groupTest(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_PDS.fsm");
	groupTest(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_SCSet.fsm");
	groupTest(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_SS.fsm");
	groupTest(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_SVS.fsm");
}

static void testAllMethod(string filename) {
	sequence_vec_t hint;
	sequence_in_t CS;
	sequence_set_t TS;
	int len;
	vector<DFSM*> indist;
	int extraStates = 0;
	
	fsm->load(filename);
	printf("%d;%d;%d;%d;%d;1;%d;", fsm->getNumberOfStates(), fsm->getNumberOfInputs(), fsm->getNumberOfOutputs(),
		fsm->getType(), (int)fsm->isReduced(), extraStates);

	SPY_method(fsm, TS, extraStates);
	len = 0;
	for (sequence_in_t seq : TS) len += seq.size() + 1;
	FaultCoverageChecker::getFSMs(fsm, TS, indist, extraStates);
	printf("%d;%d;", len, indist.size());
	for (auto f : indist) delete f;
	indist.clear();
}

static void testAll() {
	Mealy mealy;
	fsm = &mealy;

	testAllMethod(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_ADS.fsm");
	Moore moore;
	fsm = &moore;
	testAllMethod(DATA_PATH + EXAMPLES_DIR + "Moore_R4_PDS.fsm");

	DFSM dfsm;
	fsm = &dfsm;
	testAllMethod(DATA_PATH + EXAMPLES_DIR + "DFSM_R5_PDS.fsm");

}

static DFSM getFSM() {
	DFSM dfsm;
	dfsm.create(3, 4, 5);
	dfsm.setTransition(0, 1, 2, 3);
	return dfsm;
}

int main(int argc, char** argv) {
	
	DFSM ff = getFSM();// move assignment
	ff.setTransition(1, 2, 3, 4);

	ff = getFSM();

	DFSM fg(getFSM());// move constructor
	fg.setTransition(2, 0, 1, 0);

	ff = fg;// copy assignment
	auto out = ff.getOutput(2, 0);
	ff.setTransition(1, 2, 3, 4);

	out = fg.getOutput(2, 0);

	DFSM fh(fg);// copy constructor

	out = fh.getNextState(2, 0);

	char c;
	cin >> c;
	return 0;
}