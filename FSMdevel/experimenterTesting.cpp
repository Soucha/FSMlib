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
#include <chrono>
#include <fstream>
#include <filesystem>

#ifndef PARALLEL_COMPUTING
//#define PARALLEL_COMPUTING // un/comment this if CUDA is enabled/disabled
#endif // !PARALLEL_COMPUTING
#include "../FSMlib/FSMlib.h"

using namespace FSMsequence;
using namespace FSMtesting;

#define DATA_PATH			string("../data/")
#define MINIMIZATION_DIR	string("tests/minimization/")
#define SEQUENCES_DIR		string("tests/sequences/")
#define EXAMPLES_DIR		string("examples/")
#define EXPERIMENTS_DIR		string("experiments/")
#define OUTPUT_GV			string(DATA_PATH + "tmp/output.gv").c_str()

#define PTRandSTR(f) f, #f

#define COMPUTATION_TIME(com) \
	auto start = chrono::system_clock::now(); \
	com; \
	auto end = chrono::system_clock::now(); \
	chrono::duration<double> elapsed_seconds = end - start; 

static FILE * outFile;

static vector<string> descriptions;
static vector<function<sequence_set_t(const unique_ptr<DFSM>&,int)>> TSalgorithms;
static vector<function<sequence_in_t(const unique_ptr<DFSM>&,int)>> CSalgorithms;

static void loadTSAlgorithms(unsigned int mask) {
	if (mask & 1) { // PDS-method
		descriptions.emplace_back("TS\tPDS-method\t" + to_string(descriptions.size()) + "\t");
		TSalgorithms.emplace_back(bind(PDS_method, placeholders::_1, placeholders::_2));
	}
	if (mask & 2) { // ADS-method
		descriptions.emplace_back("TS\tADS-method\t" + to_string(descriptions.size()) + "\t");
		TSalgorithms.emplace_back(bind(ADS_method, placeholders::_1, placeholders::_2));
	}
	if (mask & 4) { // SVS-method
		descriptions.emplace_back("TS\tSVS-method\t" + to_string(descriptions.size()) + "\t");
		TSalgorithms.emplace_back(bind(SVS_method, placeholders::_1, placeholders::_2));
	}
	if (mask & 8) { // W-method
		descriptions.emplace_back("TS\tW-method\t" + to_string(descriptions.size()) + "\t");
		TSalgorithms.emplace_back(bind(W_method, placeholders::_1, placeholders::_2));
	}
	if (mask & 16) { // Wp-method
		descriptions.emplace_back("TS\tWp-method\t" + to_string(descriptions.size()) + "\t");
		TSalgorithms.emplace_back(bind(Wp_method, placeholders::_1, placeholders::_2));
	}
	if (mask & 32) { // HSI-method
		descriptions.emplace_back("TS\tHSI-method\t" + to_string(descriptions.size()) + "\t");
		TSalgorithms.emplace_back(bind(HSI_method, placeholders::_1, placeholders::_2));
	}
	if (mask & 64) { // H-method
		descriptions.emplace_back("TS\tH-method\t" + to_string(descriptions.size()) + "\t");
		TSalgorithms.emplace_back(bind(H_method, placeholders::_1, placeholders::_2));
	}
	if (mask & 128) { // SPY-method
		descriptions.emplace_back("TS\tSPY-method\t" + to_string(descriptions.size()) + "\t");
		TSalgorithms.emplace_back(bind(SPY_method, placeholders::_1, placeholders::_2));
	}
	if (mask & 256) { // SPYH-method
		descriptions.emplace_back("TS\tSPYH-method\t" + to_string(descriptions.size()) + "\t");
		TSalgorithms.emplace_back(bind(SPYH_method, placeholders::_1, placeholders::_2));
	}
	if (mask & 512) { // S-method
		descriptions.emplace_back("TS\tS-method\t" + to_string(descriptions.size()) + "\t");
		TSalgorithms.emplace_back(bind(S_method, placeholders::_1, placeholders::_2));
	}
	if (mask & 1024) { // Mra-method
		descriptions.emplace_back("TS\tMra-method\t" + to_string(descriptions.size()) + "\t");
		TSalgorithms.emplace_back(bind(Mra_method, placeholders::_1, placeholders::_2));
	}
	if (mask & 2048) { // Mrg-method
		descriptions.emplace_back("TS\tMrg-method\t" + to_string(descriptions.size()) + "\t");
		TSalgorithms.emplace_back(bind(Mrg_method, placeholders::_1, placeholders::_2));
	}
	if (mask & 4096) { // Mrstar-method
		descriptions.emplace_back("TS\tMrstar-method\t" + to_string(descriptions.size()) + "\t");
		TSalgorithms.emplace_back(bind(Mrstar_method, placeholders::_1, placeholders::_2));
	}
}

static void loadCSAlgorithms(unsigned int mask) {
	if (mask & 1) { // C-method
		descriptions.emplace_back("CS\tC-method\t" + to_string(descriptions.size()) + "\t");
		CSalgorithms.emplace_back(bind(C_method, placeholders::_1, placeholders::_2));
	}
	if (mask & 2) { // Ma-method
		descriptions.emplace_back("TS\tMa-method\t" + to_string(descriptions.size()) + "\t");
		CSalgorithms.emplace_back(bind(Ma_method, placeholders::_1, placeholders::_2));
	}
	if (mask & 4) { // Mg-method
		descriptions.emplace_back("TS\tMg-method\t" + to_string(descriptions.size()) + "\t");
		CSalgorithms.emplace_back(bind(Mg_method, placeholders::_1, placeholders::_2));
	}
	if (mask & 8) { // Mstar-method
		descriptions.emplace_back("TS\tMstar-method\t" + to_string(descriptions.size()) + "\t");
		CSalgorithms.emplace_back(bind(Mstar_method, placeholders::_1, placeholders::_2));
	}
}

static void printCSV(const unique_ptr<DFSM>& fsm, int extraStates, size_t numResets, seq_len_t totalLen, size_t numDiffmachines,
		double sec, const string& description) {
	fprintf(outFile, "%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%f\t%s\n", numDiffmachines, fsm->getType(),
		fsm->getNumberOfStates(), fsm->getNumberOfInputs(), fsm->getNumberOfOutputs(), 
		extraStates, numResets, totalLen, sec, description.c_str());
	fflush(outFile);
	printf(".");
}

static seq_len_t getTotalLength(const sequence_set_t& TS) {
	seq_len_t len = 0;
	for (const auto& seq : TS) {
		len += seq.size();
	}
	return len;
}

static void compareTestingAlgorithms(const unique_ptr<DFSM>& fsm, const string& fnName, state_t maxExtraStates, bool checkCorrectness) {
	size_t numIndist;
	for (state_t extraStates = 0; extraStates <= maxExtraStates; extraStates++) {
		for (size_t i = 0; i < TSalgorithms.size(); i++) {
			COMPUTATION_TIME(auto TS = TSalgorithms[i](fsm, extraStates));
			numIndist = 0;
			if (checkCorrectness && !TS.empty()) {
				auto indistinguishable = FaultCoverageChecker::getFSMs(fsm, TS, extraStates);
				if ((indistinguishable.size() > 1) || FSMmodel::areIsomorphic(fsm, indistinguishable.front())) {
					numIndist = indistinguishable.size();
				}
			}
			printCSV(fsm, extraStates, TS.size(), getTotalLength(TS), numIndist, elapsed_seconds.count(), descriptions[i] + fnName);
		}
		for (size_t i = 0; i < CSalgorithms.size(); i++) {
			COMPUTATION_TIME(auto CS = CSalgorithms[i](fsm, extraStates));
			numIndist = 0;
			if (checkCorrectness && !CS.empty()) {
				auto indistinguishable = FaultCoverageChecker::getFSMs(fsm, CS, extraStates);
				if ((indistinguishable.size() > 1) || FSMmodel::areIsomorphic(fsm, indistinguishable.front())) {
					numIndist = indistinguishable.size();
				}
			}
			printCSV(fsm, extraStates, !CS.empty(), CS.size(), numIndist, elapsed_seconds.count(), descriptions[TSalgorithms.size()+i] + fnName);
		}
		printf(" ");
	}
}

using namespace std::tr2::sys;

void testDirTesting(int argc, char** argv) {
	string outFilename = "";
	auto dir = string(argv[1]);
	state_t maxExtraStates = 2;
	unsigned int machineTypeMask = unsigned int(-1);// all
	state_t statesRestrictionLess = NULL_STATE, statesRestrictionGreater = NULL_STATE;
	unsigned int TSalgorithmMask = unsigned int(-1);//all
	unsigned int CSalgorithmMask = unsigned int(-1);//all
	bool TSalgAllowed = true;
	bool CSalgAllowed = true;
	bool checkCorrectness = true;
	for (int i = 2; i < argc; i++) {
		if (strcmp(argv[i], "-o") == 0) {
			outFilename = string(argv[++i]);
		}
		else if (strcmp(argv[i], "-es") == 0) {
			maxExtraStates = state_t(atoi(argv[++i]));
		}
		else if (strcmp(argv[i], "-m") == 0) {//machine type
			machineTypeMask = atoi(argv[++i]);
		}
		else if (strcmp(argv[i], "-s") == 0) {//states
			statesRestrictionLess = state_t(atoi(argv[++i]));
			statesRestrictionGreater = statesRestrictionLess - 1;
			statesRestrictionLess++;
		}
		else if (strcmp(argv[i], "-sl") == 0) {//states
			statesRestrictionLess = state_t(atoi(argv[++i]));
		}
		else if (strcmp(argv[i], "-sg") == 0) {//states
			statesRestrictionGreater = state_t(atoi(argv[++i]));
		}
		else if (strcmp(argv[i], "-at") == 0) {//algorithm type
			auto algorithmTypeMask = atoi(argv[++i]);
			TSalgAllowed = 1 & algorithmTypeMask;
			CSalgAllowed = 2 & algorithmTypeMask;
		}
		else if (strcmp(argv[i], "-ts") == 0) {//algorithm TS
			TSalgorithmMask = atoi(argv[++i]);
		}
		else if (strcmp(argv[i], "-cs") == 0) {//algorithm CS
			CSalgorithmMask = atoi(argv[++i]);
		}
		else if (strcmp(argv[i], "-co") == 0) {//check correctness
			checkCorrectness = atoi(argv[++i]);
		}
	}
	if (outFilename.empty()) outFilename = dir + "resultsTesting.csv";
	if (fopen_s(&outFile, outFilename.c_str(), "w") != 0) {
		cerr << "Unable to open file " << outFilename << " for results!" << endl;
		return;
	}
	fprintf(outFile, "Correct/IndistMachines\tFSMtype\tStates\tInputs\tOutputs\tES\tResets\tSymbols\tseconds\t"
		"AlgorithmType\tAlgorithm\tAlgId\tfileName\n");
	if (TSalgAllowed) loadTSAlgorithms(TSalgorithmMask);
	if (CSalgAllowed) loadCSAlgorithms(CSalgorithmMask);
	path dirPath(dir);
	directory_iterator endDir;
	for (directory_iterator it(dirPath); it != endDir; ++it) {
		if (is_regular_file(it->status())) {
			path fn(it->path());
			if (fn.extension().compare(".fsm") == 0) {
				auto fsm = FSMmodel::loadFSM(fn.string());
				if ((fsm) && (machineTypeMask & (1 << fsm->getType())) &&
					((statesRestrictionLess == NULL_STATE) || (fsm->getNumberOfStates() < statesRestrictionLess)) &&
					((statesRestrictionGreater == NULL_STATE) || (statesRestrictionGreater < fsm->getNumberOfStates()))) {
					compareTestingAlgorithms(fsm, fn.filename(), maxExtraStates, checkCorrectness);
					printf("%s tested\n", fn.filename().c_str());
				}
			}
		}
	}
	fclose(outFile);
	printf("complete.\n");
}

