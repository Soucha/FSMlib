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

unique_ptr<DFSM> fsm, fsm2;
wchar_t message[200];

using namespace FSMsequence;
using namespace FSMtesting;
using namespace FSMlearning;

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

static void testAllMethod(string filename) {
	sequence_vec_t hint;
	sequence_in_t CS;
	int extraStates = 0;
	
	fsm->load(filename);
	printf("%d;%d;%d;%d;%d;1;%d;", fsm->getNumberOfStates(), fsm->getNumberOfInputs(), fsm->getNumberOfOutputs(),
		fsm->getType(), (int)fsm->isReduced(), extraStates);

	auto TS = SPY_method(fsm, extraStates);
	seq_len_t len = 0;
	for (sequence_in_t seq : TS) len += seq_len_t(seq.size() + 1);
	auto indist = FaultCoverageChecker::getFSMs(fsm, TS, extraStates);
	printf("%d;%d;", len, indist.size());
}

static void testAll() {
	fsm = make_unique<Mealy>();
	testAllMethod(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_ADS.fsm");
	
	fsm = make_unique<Moore>();
	testAllMethod(DATA_PATH + EXAMPLES_DIR + "Moore_R4_PDS.fsm");

	fsm = make_unique<DFSM>();
	testAllMethod(DATA_PATH + EXAMPLES_DIR + "DFSM_R5_PDS.fsm");

}

static DFSM getFSM() {
	DFSM dfsm;
	dfsm.create(3, 4, 5);
	dfsm.setTransition(0, 1, 2, 3);
	return dfsm;
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

#define OUTPUT_GV string(string(DATA_PATH) + "tmp/output.gv").c_str()

bool showConjecture(const unique_ptr<DFSM>& conjecture) {
	auto fn = conjecture->writeDOTfile(string(DATA_PATH) + "tmp/");
	//char c;	cin >> c;
	remove(OUTPUT_GV);
	rename(fn.c_str(), OUTPUT_GV);
	return true;
}

static void testLstar(const unique_ptr<Teacher>& teacher,
	function<void(const sequence_in_t& ce, ObservationTable& ot, const unique_ptr<Teacher>& teacher)> processCE,
	string fnName, bool checkConsistency = false) {
	auto model = Lstar(teacher, processCE, showConjecture, checkConsistency);
	cout << "Correct: " << FSMmodel::areIsomorphic(fsm, model) << ", reset: " << teacher->getAppliedResetCount();
	cout << ",\tOQ: " << teacher->getOutputQueryCount() << ",\tEQ: " << teacher->getEquivalenceQueryCount();
	cout << ",\tsymbols: " << teacher->getQueriedSymbolsCount() << ",\t" << fnName << endl;
}

static void testLStarAllVariants() {
	//fsm = make_unique<Moore>();
	//fsm->load(DATA_PATH + SEQUENCES_DIR + "Moore_R10_PDS.fsm");

	vector<pair<function<void(const sequence_in_t& ce, ObservationTable& ot, const unique_ptr<Teacher>& teacher)>, string>>	ceFunc;
	ceFunc.emplace_back(PTRandSTR(addAllPrefixesToS));
	ceFunc.emplace_back(PTRandSTR(addSuffixAfterLastStateToE));
	ceFunc.emplace_back(PTRandSTR(addAllSuffixesAfterLastStateToE));
	ceFunc.emplace_back(PTRandSTR(addSuffix1by1ToE));
	ceFunc.emplace_back(PTRandSTR(addSuffixToE_binarySearch));

	for (size_t i = 0; i < ceFunc.size(); i++) {
		unique_ptr<Teacher> teacher = make_unique<TeacherDFSM>(fsm, true);
		testLstar(teacher, ceFunc[i].first, ceFunc[i].second, (i == 0));
	}

	for (size_t i = 0; i < ceFunc.size(); i++) {
		unique_ptr<Teacher> teacher = make_unique<TeacherRL>(fsm);
		testLstar(teacher, ceFunc[i].first, ceFunc[i].second, (i == 0));
	}

	for (size_t i = 0; i < ceFunc.size(); i++) {
		shared_ptr<BlackBox> bb = make_shared<BlackBoxDFSM>(fsm, true);
		unique_ptr<Teacher> teacher = make_unique<TeacherBB>(bb, SPY_method);
		testLstar(teacher, ceFunc[i].first, ceFunc[i].second, (i == 0));
	}
}

int main(int argc, char** argv) {
	//getCSet();
	fsm = make_unique<Mealy>();
	fsm->load(DATA_PATH + SEQUENCES_DIR + "Mealy_R10_PDS.fsm");
	//fsm->load(DATA_PATH + EXAMPLES_DIR + "DFSM_R5_PDS.fsm");
	//testLStarAllVariants();
	shared_ptr<BlackBox> bb = make_shared<BlackBoxDFSM>(fsm, true);
	unique_ptr<Teacher> teacher = make_unique<TeacherBB>(bb, FSMtesting::SPY_method);

	//unique_ptr<Teacher> teacher = make_unique<TeacherDFSM>(fsm, true);
	auto model = DiscriminationTreeAlgorithm(teacher, showConjecture);
	cout << "Correct: " << FSMmodel::areIsomorphic(fsm, model) << ", reset: " << teacher->getAppliedResetCount();
	cout << ",\tOQ: " << teacher->getOutputQueryCount() << ",\tEQ: " << teacher->getEquivalenceQueryCount();
	cout << ",\tsymbols: " << teacher->getQueriedSymbolsCount() << ",\t" << endl;

	char c;
	cin >> c;
	return 0;
}