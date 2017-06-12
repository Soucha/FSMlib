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
#include <iostream>
#include <chrono>
#include <fstream>
#include <filesystem>

#ifndef PARALLEL_COMPUTING
//#define PARALLEL_COMPUTING // un/comment this if CUDA is enabled/disabled
#endif // !PARALLEL_COMPUTING
#include "../FSMlib/FSMlib.h"

using namespace FSMsequence;

#define COMPUTATION_TIME(com) \
	auto start = chrono::system_clock::now(); \
	com; \
	auto end = chrono::system_clock::now(); \
	chrono::duration<double> elapsed_seconds = end - start; 

static FILE * outFile;

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
	fprintf(outFile, "%d\t%d\t%d\t%d\t%d\t%d\t%f\t%s\t%d\t%s\n", fsm->getType(),
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
		COMPUTATION_TIME(auto hsi = getHarmonizedStateIdentifiersFromSplittingTree(fsm, getSplittingTree(fsm, true)));
		printCSV(fsm, hsi, elapsed_seconds.count(), 2, "SplittingTreeBased", fnName);
	}
	fflush(outFile);
	printf(".");
}

using namespace std::tr2::sys;

void compareHSIdesigns(int argc, char** argv) {
	string outFilename = "";
	auto dir = string(argv[2]);
	unsigned int machineTypeMask = unsigned int(-1);// all
	state_t statesRestrictionLess = NULL_STATE, statesRestrictionGreater = NULL_STATE;
	for (int i = 3; i < argc; i++) {
		if (strcmp(argv[i], "-o") == 0) {
			outFilename = string(argv[++i]);
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
	}
	if (outFilename.empty()) outFilename = dir + "_HSIdesingComparison.csv";
	if (fopen_s(&outFile, outFilename.c_str(), "w") != 0) {
		cerr << "Unable to open file " << outFilename << " for results!" << endl;
		return;
	}
	fprintf(outFile, "FSMtype\tStates\tInputs\tOutputs\tSequences\tSymbols\tSeconds\tAlgorithm\tAlgId\tfileName\n");
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
					compareDesignAlgoritms(fsm, fn.filename());
					//printf("%s tested\n", fn.filename().c_str());
				}
			}
		}
	}
	fclose(outFile);
	printf("complete.\n");
}