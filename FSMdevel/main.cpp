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
#ifndef PARALLEL_COMPUTING
//#define PARALLEL_COMPUTING // un/comment this if CUDA is enabled/disabled
#endif // !PARALLEL_COMPUTING

#include "commons.h"

unique_ptr<DFSM> fsm;

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

static void printTS(sequence_set_t & TS, string filename) {
	printf("Set of %d test sequences (%s):\n", TS.size(), filename.c_str());
	FSMlib::PrefixSet ps;
	seq_len_t len(0);
	for (const auto& cSeq : TS) {
		len += cSeq.size();
		ps.insert(cSeq);
		printf("%s\n", FSMmodel::getInSequenceAsString(cSeq).c_str());
	}
	printf("Total length: %d\n", len);
	auto syms = ps.getNumberOfSymbols();
	printf("%d,%d,%d,%f,%d,%f\n", TS.size(), len, TS.size()+len, double(len)/TS.size(), syms, double(syms)/len);
}

static void compTestingMethods(string fileName) {
	auto st = getSplittingTree(fsm, true);
	auto hsiST = getHarmonizedStateIdentifiersFromSplittingTree(fsm, st);
	for (state_t ES = 0; ES < 3; ES++) {
		auto TSwp = Wp_method(fsm, ES);
		auto TShsi = HSI_method(fsm, ES);
		auto TShsiST = HSI_method(fsm, ES, hsiST);
		auto TSh = H_method(fsm, ES);
		auto TSspy = SPY_method(fsm, ES);
		auto TSspyST = SPY_method(fsm, ES, hsiST);
		auto TSspyh = SPYH_method(fsm, ES);
		auto TSs = S_method(fsm, ES);
		printf("Wp,"); printTS(TSwp, fileName);
		printf("HSI,"); printTS(TShsi, fileName);
		printf("HSI-ST,"); printTS(TShsiST, fileName);
		printf("H,"); printTS(TSh, fileName);
		printf("SPY,"); printTS(TSspy, fileName);
		printf("SPY-ST,"); printTS(TSspyST, fileName);
		printf("SPYH,"); printTS(TSspyh, fileName);
		printf("S,"); printTS(TSs, fileName);
		printf("\n");
	}
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
#if DBG_MEMORY_LEAK
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG);
	//_CrtSetBreakAlloc(1045);
#endif
	//char* vals[11] = { "", "4", "../data/experiments/10multi/", "-a", "256", "-m", "12", "-sg", "9", "-sl", "49"}; testDirLearning(11, vals);
	//char* vals[13] = { "", "3", "../data/experiments/10multi/", "-ts", "512", "-cs", "0", "-m", "12", "-co", "1", "-es", "0" }; testDirTesting(12, vals);/*
	
#if 1 // load FSM
	//string fileName = DATA_PATH + EXPERIMENTS_DIR + "DFA_R50_peterson2.fsm"; 
	//string fileName = DATA_PATH + EXPERIMENTS_DIR + "DFA_R97_sched4.fsm";
	//string fileName = DATA_PATH + EXPERIMENTS_DIR + "DFA_R241_sched5.fsm";
	//string fileName = DATA_PATH + EXPERIMENTS_DIR + "DFA_R664_pots2.fsm";
	//string fileName = DATA_PATH + EXPERIMENTS_DIR + "DFSM_R25_GW.fsm";
	//string fileName = DATA_PATH + EXPERIMENTS_DIR + "Mealy_R14_cvs.fsm";
	//string fileName = DATA_PATH + EXPERIMENTS_DIR + "esm-manual-controller/Mealy_3410_esm.fsm";
	//string fileName = DATA_PATH + SEQUENCES_DIR + "DFA_R6_ADS.fsm";
	//string fileName = DATA_PATH + EXAMPLES_DIR + "Mealy_R5.fsm";
	//string fileName = DATA_PATH + SEQUENCES_DIR + "Mealy_R100.fsm";
	//string fileName = DATA_PATH + EXPERIMENTS_DIR + "100multi/" + "Moore_R100.fsm";
	//string fileName = DATA_PATH + EXPERIMENTS_DIR + "10multi/refMachines/" + "Mealy_R60.fsm";
	//string fileName = DATA_PATH + EXPERIMENTS_DIR + "10multi/" + "Mealy_R30_GHwoR.fsm"; //Mealy_R60_WdoSu 
	//string fileName = DATA_PATH + "10multi/" + "DFSM_R20_FZZHZ.fsm";//DFA_R100_0N9k1 _1vV4O
	//string fileName = DATA_PATH + "DFSM_R20.fsm";//DFA_R100_0N9k1 DFSM_R80_1M6Qp
	//string fileName = DATA_PATH + "exp/DFA/DFA_R90_5_bCoC4.fsm";//DFA_R100_0N9k1 Mealy_R60_10_kyuGN
	//string fileName = DATA_PATH + "exp/Mealy/Mealy_R70_10_gSAMS.fsm";//DFA_R100_0N9k1 
	string fileName = DATA_PATH + EXAMPLES_DIR + "DFA_R4_SCSet.fsm";
	//string fileName = DATA_PATH + SEQUENCES_DIR + "Moore_R100.fsm";
	//string fileName = DATA_PATH + EXAMPLES_DIR + "Mealy_R4_ADS.fsm"; //DFA_R4_SCSet
	//string fileName = DATA_PATH + EXAMPLES_DIR + "Moore_R5_SVS.fsm";
	fsm = FSMmodel::loadFSM(fileName);
#endif

#if 1 // learning
	/*
	shared_ptr<BlackBox> bb = make_shared<BlackBoxDFSM>(fsm, true);
	unique_ptr<Teacher> teacher = make_unique<TeacherBB>(bb, bind(FSMtesting::HSI_method, placeholders::_1, placeholders::_2,
		bind(FSMsequence::getHarmonizedStateIdentifiersFromSplittingTree, placeholders::_1,
		bind(FSMsequence::getSplittingTree, placeholders::_1, true, false), false)), 3);
	//unique_ptr<Teacher> teacher = make_unique<TeacherBB>(bb, FSMtesting::SPY_method, 2);
	/*/
	//unique_ptr<Teacher> teacher = make_unique<TeacherRL>(fsm);
	unique_ptr<Teacher> teacher = make_unique<TeacherDFSM>(fsm, true);

	//COMPUTATION_TIME(auto model = Lstar(teacher, addAllPrefixesToS, nullptr, false, false);)
	//COMPUTATION_TIME(auto model = DiscriminationTreeAlgorithm(teacher, nullptr);)
	//COMPUTATION_TIME(auto model = ObservationPackAlgorithm(teacher, OneLocally, nullptr);)
	//COMPUTATION_TIME(auto model = TTT(teacher, nullptr);)
	//COMPUTATION_TIME(auto model = QuotientAlgorithm(teacher, nullptr);)
	//COMPUTATION_TIME(auto model = GoodSplit(teacher, 2, nullptr,true);)
	//COMPUTATION_TIME(auto model = Hlearner(teacher, 1, nullptr, true));// showAndStop);
	//COMPUTATION_TIME(auto model = SPYlearner(teacher, 1, nullptr, true));// showAndStop);
	COMPUTATION_TIME(auto model = Slearner(teacher, 1, nullptr, true));// showAndStop);
	printf("Correct\tFSMtype\tStates\tInputs\tOutputs\tResets\tOQs\tEQs\tsymbols\tseconds\n");
	printf("%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%f\n", FSMmodel::areIsomorphic(fsm, model), fsm->getType(),
		fsm->getNumberOfStates(), fsm->getNumberOfInputs(), fsm->getNumberOfOutputs(), teacher->getAppliedResetCount(),
		teacher->getOutputQueryCount(), teacher->getEquivalenceQueryCount(), teacher->getQueriedSymbolsCount(),
		elapsed_seconds.count());
#endif
	break;
	}

	char c;
	//printf("type any key to end: ");
	cin >> c;
	return 0;
}