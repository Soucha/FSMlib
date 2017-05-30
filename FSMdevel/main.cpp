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

#define DBG_MEMORY_LEAK 0
#if DBG_MEMORY_LEAK
#define _CRTDBG_MAP_ALLOC  
#include <stdlib.h>  
#include <crtdbg.h>  
#endif

#include <iostream>
#include <chrono>
#include <filesystem>


#ifndef PARALLEL_COMPUTING
//#define PARALLEL_COMPUTING // un/comment this if CUDA is enabled/disabled
#endif // !PARALLEL_COMPUTING
#include "../FSMlib/FSMlib.h"

#define DATA_PATH			string("../data/")
#define MINIMIZATION_DIR	string("tests/minimization/")
#define SEQUENCES_DIR		string("tests/sequences/")
#define EXAMPLES_DIR		string("examples/")
#define EXPERIMENTS_DIR		string("experiments/")

unique_ptr<DFSM> fsm, fsm2;
wchar_t message[200];

using namespace FSMsequence;
using namespace FSMtesting;
using namespace FSMlearning;

using namespace std::tr2::sys;

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

#define OUTPUT_GV string(DATA_PATH + "tmp/output.gv").c_str()

bool showConjecture(const unique_ptr<DFSM>& conjecture) {
	auto fn = conjecture->writeDOTfile(DATA_PATH + "tmp/");
	//char c;	cin >> c;
	remove(OUTPUT_GV);
	rename(fn.c_str(), OUTPUT_GV);
	return true;
}

bool showAndStop(const unique_ptr<DFSM>& conjecture) {
	showConjecture(conjecture);
	unique_ptr<Teacher> teacher = make_unique<TeacherRL>(fsm);
	auto ce = teacher->equivalenceQuery(conjecture);
	return !ce.empty();
}

#define COMPUTATION_TIME(com) \
	auto start = chrono::system_clock::now(); \
	com; \
	auto end = chrono::system_clock::now(); \
	chrono::duration<double> elapsed_seconds = end - start; 

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

static void printTS(sequence_set_t & TS, string filename) {
	printf("Set of %d test sequences (%s):\n", TS.size(), filename.c_str());
	seq_len_t len(0);
	for (sequence_in_t cSeq : TS) {
		len += cSeq.size();
		printf("%s\n", FSMmodel::getInSequenceAsString(cSeq).c_str());
	}
	printf("Total length: %d\n", len);
}

static void printCSV(const unique_ptr<DFSM>& fsm, vector<sequence_set_t>& hsi, double sec,
	int algId, const string& algName, const string& fnName) {
	size_t seqs(0);
	seq_len_t len(0);
	for (const auto& h : hsi) {
		seqs += h.size();
		for (const auto& seq : h) {
			len += seq.size();
		}
	}
	printf("%d\t%d\t%d\t%d\t%d\t%d\t%f\t%s\t%d\t%s\n", fsm->getType(),
		fsm->getNumberOfStates(), fsm->getNumberOfInputs(), fsm->getNumberOfOutputs(),
		seqs, len, sec, algName.c_str(), algId, fnName.c_str());
}

static void compareDesignAlgoritms(const unique_ptr<DFSM>& fsm, const string& fnName) {
	{
		COMPUTATION_TIME(auto hsi = getHarmonizedStateIdentifiers(fsm, getStatePairsShortestSeparatingSequences));
		printCSV(fsm, hsi, elapsed_seconds.count(), 0, "ShortestSeparatingSequences", fnName);
	}
	{
		COMPUTATION_TIME(auto hsi = getHarmonizedStateIdentifiers(fsm, getStatePairsSeparatingSequencesFromSplittingTree));
		printCSV(fsm, hsi, elapsed_seconds.count(), 1, "SeparatingSequencesFromSplittingTree", fnName);
	}
	{
		COMPUTATION_TIME(auto hsi = getHarmonizedStateIdentifiers(fsm, getSplittingTree(fsm, true)));
		printCSV(fsm, hsi, elapsed_seconds.count(), 2, "SplittingTreeBased", fnName);
	}
	//printf(".");
}

#define ARE_EQUAL(expected, actual, format, ...) {\
	if (expected != actual) printf(format, ##__VA_ARGS__); }

extern void generateMachines(int argc, char** argv);
extern void analyseDirMachines(int argc, char** argv);
extern void testDirTesting(int argc, char** argv);
extern void testDirLearning(int argc, char** argv);
extern void compareHSIdesigns(int argc, char** argv);

//extern void testBBport();

int main(int argc, char** argv) {
	int prog = (argc > 1) ? atoi(argv[1]) : 0;
	switch (prog) {
	case 1:
		generateMachines(argc, argv);
		break;
	case 2:
		analyseDirMachines(argc, argv);
		break;
	case 3:
		testDirTesting(argc, argv);
		break;
	case 4:
		testDirLearning(argc, argv);
		break;
	case 5:
		compareHSIdesigns(argc, argv);
		break;
	default:
	//*
#if DBG_MEMORY_LEAK
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG);
	//_CrtSetBreakAlloc(1045);
#endif
	//char* vals[11] = { "", "4", "../data/experiments/10multi/", "-a", "128", "-m", "12", "-sg", "9", "-sl", "49"}; testDir(10, vals);
	//char* vals[13] = { "", "3", "../data/experiments/10multi/", "-ts", "512", "-cs", "0", "-m", "12", "-co", "1", "-es", "0" }; testDirTesting(12, vals);/*
	//getCSet();
	//fsm = make_unique<Mealy>();
	//string fileName = DATA_PATH + EXPERIMENTS_DIR + "DFA_R50_peterson2.fsm"; 
	//string fileName = DATA_PATH + EXPERIMENTS_DIR + "DFA_R97_sched4.fsm";
	//string fileName = DATA_PATH + EXPERIMENTS_DIR + "DFA_R241_sched5.fsm";
	//string fileName = DATA_PATH + EXPERIMENTS_DIR + "DFA_R664_pots2.fsm";
	//string fileName = DATA_PATH + EXPERIMENTS_DIR + "DFSM_R25_GW.fsm";
	//string fileName = DATA_PATH + EXPERIMENTS_DIR + "Mealy_R14_cvs.fsm";
	//string fileName = DATA_PATH + SEQUENCES_DIR + "Moore_R10_PDS.fsm";
	//string fileName = DATA_PATH + EXAMPLES_DIR + "Mealy_R5.fsm";
	//string fileName = DATA_PATH + SEQUENCES_DIR + "Mealy_R100.fsm";
	//string fileName = DATA_PATH + EXPERIMENTS_DIR + "100multi/" + "Moore_R100.fsm";
	//string fileName = DATA_PATH + EXPERIMENTS_DIR + "10multi/refMachines/" + "Mealy_R60.fsm";
	//string fileName = DATA_PATH + EXPERIMENTS_DIR + "10multi/" + "Mealy_R60_7YTZQ.fsm"; //Mealy_R60_WdoSu 
	string fileName = DATA_PATH + "10multi/" + "DFA_R10_lwEcI.fsm";//DFA_R100_0N9k1
	// ES0 Mealy_R50_n4FnI Mealy_R60_o7cia 
	// ES1 Mealy_R60_WdoSu Moore_R40_1zxZn  Mealy_R40_xeCCe Mealy_R60_97Nbx Moore_R50_ylWfw
	// solved Mealy_R40_xeCCe Mealy_R50_j40nK Mealy_R50_p3m9k Mealy_R60 Mealy_R60_97Nbx Mealy_R50
	//string fileName = DATA_PATH + EXAMPLES_DIR + "DFA_R4_SCSet.fsm";
	//string fileName = DATA_PATH + SEQUENCES_DIR + "Moore_R100.fsm";
	//string fileName = DATA_PATH + EXAMPLES_DIR + "Moore_R4_SCSet.fsm"; //DFA_R4_SCSet
	//string fileName = DATA_PATH + EXAMPLES_DIR + "Moore_R5_SVS.fsm";
	//fsm->load(fileName);
	auto fsm = FSMmodel::loadFSM(fileName);
 	auto st = getSplittingTree(fsm, true);
	compareDesignAlgoritms(fsm, fileName);

	bool test = true;
	for (int extraStates = 0; extraStates <= 2; extraStates++) {
		auto TS = S_method(fsm, extraStates);
		printTS(TS, fileName);
		if (test) {
			ARE_EQUAL(false, TS.empty(), "Obtained TS is empty.");
			auto indistinguishable = FaultCoverageChecker::getFSMs(fsm, TS, extraStates);
			ARE_EQUAL(1, int(indistinguishable.size()), "The SPY-method (%d extra states) has not complete fault coverage,"
				" it produces %d indistinguishable FSMs.", extraStates, indistinguishable.size());
			ARE_EQUAL(true, FSMmodel::areIsomorphic(fsm, indistinguishable.front()), "FCC found a machine different from the specification.");
		}
	}

	/* /
	//shared_ptr<BlackBox> bb = make_shared<BlackBoxDFSM>(fsm, true);
	//unique_ptr<Teacher> teacher = make_unique<TeacherBB>(bb, FSMtesting::SPY_method, 2);
	//unique_ptr<Teacher> teacher = make_unique<TeacherRL>(fsm);
	//auto model = Lstar(teacher, addSuffixAfterLastStateToE, showConjecture, false, true);
	unique_ptr<Teacher> teacher = make_unique<TeacherDFSM>(fsm, true);//
	COMPUTATION_TIME(auto model = SPYlearner(teacher, 1, showConjecture, true));// showAndStop);
	//COMPUTATION_TIME(auto model = ObservationTreeAlgorithm(teacher, 0, nullptr, true));// showAndStop);
	//COMPUTATION_TIME(auto model = Lstar(teacher, addAllPrefixesToS, nullptr, false, false);)
	//COMPUTATION_TIME(auto model = DiscriminationTreeAlgorithm(teacher, nullptr);)
	//COMPUTATION_TIME(auto model = ObservationPackAlgorithm(teacher, OneLocally, nullptr);)
	//COMPUTATION_TIME(auto model = TTT(teacher, nullptr);)
	//COMPUTATION_TIME(auto model = QuotientAlgorithm(teacher, nullptr);)
	//COMPUTATION_TIME(auto model = GoodSplit(teacher, 2, nullptr,true);)
	printf("Correct\tFSMtype\tStates\tInputs\tOutputs\tResets\tOQs\tEQs\tsymbols\tseconds\n");
	printf("%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%f\n", FSMmodel::areIsomorphic(fsm, model), fsm->getType(),
		fsm->getNumberOfStates(), fsm->getNumberOfInputs(), fsm->getNumberOfOutputs(), teacher->getAppliedResetCount(),
		teacher->getOutputQueryCount(), teacher->getEquivalenceQueryCount(), teacher->getQueriedSymbolsCount(),
		elapsed_seconds.count());
	/* /
	//*/
	//translateLearnLibDFAtoFSMformat(DATA_PATH + "sched5.dfa");
	/*
	sequence_set_t TS;
	int extraStates = 0;
	TS.insert({ 0, 0 });
	TS.insert({ 1, 1, 1, 1, 0, 1, 1, 0, 1 });
	TS.insert({ 0, 1 });
	TS.insert({ 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1 });

	auto indistinguishable = FaultCoverageChecker::getFSMs(fsm, TS, extraStates);
	cout << (indistinguishable.size()) << endl;
	if (indistinguishable.size() > 1) {
		for (auto& f : indistinguishable) {
			auto s = f->writeDOTfile(DATA_PATH + "tmp/");
			cout << s << endl;
		}
	}

	TS.insert({ 0, 0, 0, 0, 1 });
	//TS.insert({ 1, 1, 0, 0, 1 });
	TS.insert({ 1, 1, 1, 0, 1, 1, 1, 1 });
	TS.insert({ 1, 0, 0, 1 });
	TS.insert({ 1, 0, 1 });
	TS.insert({ 1, 0, 0, 0, 1, 0, 0, 1 });

	TS.clear();
	TS.insert({ 0, 0, 1, 0, 1, 0, 0, 0, 1 });
	TS.insert({ 0, 0, 1, 1, 0, 1 });
	TS.insert({ 0, 1, 0, 0, 1, 1, 0, 1 });
	TS.insert({ 1, 0, 0, 0, 1, 0, 0, 1 });
	TS.insert({ 1, 0, 0, 1, 1, 1 });
	TS.insert({ 1, 0, 1, 1, 0, 0, 1 });
	TS.insert({ 1, 1, 1, 0, 1 });


	extraStates = 1;
	indistinguishable = FaultCoverageChecker::getFSMs(fsm, TS, extraStates);
	cout << (indistinguishable.size()) << endl;
	if (indistinguishable.size() > 1) {
		for (auto& f : indistinguishable) {
			auto s = f->writeDOTfile(DATA_PATH + "tmp/");
			cout << s << endl;
		}
	}
	*/
	break;
	}

	char c;
	//printf("type any key to end: ");
	cin >> c;
	return 0;
}