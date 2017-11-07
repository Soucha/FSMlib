/* Copyright (c) Michal Soucha, 2017
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

#include "commons.h"

extern unique_ptr<DFSM> fsm;

static void translateLearnLibDFAtoFSMformat(string fileName) {
	fsm = make_unique<DFA>();
	ifstream is(fileName);
	state_t numStates;
	input_t numInputs;
	output_t output;
	is >> numStates >> numInputs;
	fsm->create(numStates, numInputs, 2);
	for (state_t state = 0; state < numStates; state++) {
		is >> output;
		fsm->setOutput(state, output);
	}
	state_t ns;
	for (state_t state = 0; state < numStates; state++) {
		for (input_t i = 0; i < numInputs; i++) {
			is >> ns;
			fsm->setTransition(state, i, ns);
		}
	}
	fsm->minimize();
	fsm->save(DATA_PATH + "experiments/");

}

static void printSeqSet(sequence_set_t& seqSet) {
	for (auto seq : seqSet) {
		cout << FSMmodel::getInSequenceAsString(seq) << endl;
	}
	cout << endl;
}

int getCSet() {
	fsm = make_unique<Moore>();
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

void testGetSeparatingSequences(string filename, sequence_vec_t(*getSepSeq)(const unique_ptr<DFSM>& dfsm, bool), string name) {
	if (!filename.empty()) fsm->load(filename);
	auto seq = (*getSepSeq)(fsm, false);
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
	fsm = make_unique<Mealy>();

	groupTest(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_ADS.fsm");
	groupTest(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_HS.fsm");
	groupTest(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_PDS.fsm");
	groupTest(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_SCSet.fsm");
	groupTest(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_SS.fsm");
	groupTest(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_SVS.fsm");
}

void printADS(const unique_ptr<AdaptiveDS>& node, int base = 0) {
	if (node->currentStates.size() == 1) {
		printf(": %u => %u\n", node->initialStates[0], node->currentStates[0]);
	}
	else {
		printf(" -> %s\n", FSMmodel::getInSequenceAsString(node->input).c_str());
		for (map<output_t, unique_ptr<AdaptiveDS>>::iterator it = node->decision.begin();
			it != node->decision.end(); it++) {
			printf("%*u", base + 3, it->first);
			printADS(it->second, base + 3);
		}
	}
}
