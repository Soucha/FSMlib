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

static FILE * outFile;

static double getMedian(sequence_vec_t& vec) {
	sort(vec.begin(), vec.end());
	const auto& size = vec.size();
	return (size % 2 != 0) ? double(vec[size / 2].size()) :
		(double(vec[size / 2].size() + vec[(size / 2) - 1].size()) / 2);
}

static void printStat(sequence_vec_t& vec) {
	seq_len_t totalLen(0);
	for (const auto& seq : vec) {
		totalLen += seq.size();
	}
	double mean = double(totalLen) / vec.size();
	double variance(0);
	seq_len_t maxLen(0);
	for (const auto& seq : vec) {
		variance += pow(seq.size() - mean, 2);
		if (maxLen < seq.size()) maxLen = seq.size();
	}
	variance /= vec.size();
	fprintf(outFile, "%d\t%f\t%f\t%f\t", maxLen, mean, getMedian(vec), variance);
}

static void analyseAll(const unique_ptr<DFSM>& fsm) {
	fprintf(outFile, "%d\t%d\t%d\t%d\t%d\t%d\t", fsm->getType(),
		fsm->getNumberOfStates(), fsm->getNumberOfInputs(), fsm->getNumberOfOutputs(),
		fsm->isReduced(), FSMmodel::isStronglyConnected(fsm));
	// acceess seq
	auto sc = getStateCover(fsm, true);
	sequence_vec_t vecSC(sc.begin(), sc.end());
	printStat(vecSC);

	// separating sequences
	auto sepSeq = getStatePairsShortestSeparatingSequences(fsm, true);
	printStat(sepSeq);

	// state verifying sequences
	auto SVSet = getVerifyingSet(fsm, true);
	for (long i = long(SVSet.size()) - 1; i >= 0; i--) {
		if (SVSet[i].empty()) {
			SVSet[i].swap(SVSet.back());
			SVSet.pop_back();
		}
	}
	fprintf(outFile, "%d\t", sc.size() - SVSet.size());
	printStat(SVSet);

	// adaptive distinguishing sequences
	auto ADSet = getAdaptiveDistinguishingSet(fsm, true);
	if (ADSet.empty()) {
		fprintf(outFile, "%d\t%d\t%d\t%d\t", -1, -1, -1, -1);
	}
	else {
		printStat(ADSet);
	}
	fflush(outFile);
}

static void analyseCounts(const unique_ptr<DFSM>& fsm, map<int, vector<size_t>>& counts) {
	auto idx = fsm->getNumberOfStates() * 30 + 3 * fsm->getType();
	// acceess seq
	auto sc = getStateCover(fsm, true);
	auto it = counts.find(idx);
	if (it == counts.end()) {
		counts.emplace(idx, vector<size_t>());
		it = counts.find(idx);
	}
	for (const auto& seq : sc) {
		if (seq.size() >= it->second.size()) it->second.resize(seq.size() + 1, 0);
		it->second[seq.size()]++;
	}
	//printf(".");
	// state verifying sequences
	auto SVSet = getVerifyingSet(fsm, true);
	idx++;
	
	it = counts.find(idx);
	if (it == counts.end()) {
		counts.emplace(idx, vector<size_t>());
		it = counts.find(idx);
	}
	for (const auto& seq : SVSet) {
		if (seq.size() >= it->second.size()) it->second.resize(seq.size() + 1, 0);
		it->second[seq.size()]++;
	}

	// separating sequences
	auto sepSeq = getStatePairsShortestSeparatingSequences(fsm, true);
	idx++;
	it = counts.find(idx);
	if (it == counts.end()) {
		counts.emplace(idx, vector<size_t>());
		it = counts.find(idx);
	}
	for (const auto& seq : sepSeq) {
		if (seq.size() >= it->second.size()) it->second.resize(seq.size() + 1, 0);
		it->second[seq.size()]++;
	}
}

static void analyse(const unique_ptr<DFSM>& fsm) {
	// acceess seq
	auto sc = getStateCover(fsm, true);
	
	// state verifying sequences
	auto SVSet = getVerifyingSet(fsm, true);
	
	auto it = sc.begin();
	for (state_t s = 0; s < sc.size(); s++, it++) {
		fprintf(outFile, "%u\t%u\n", it->size(), SVSet[s].size());
	}
	fflush(outFile);
}

static void analyseSepSeq(const unique_ptr<DFSM>& fsm) {
	// separating sequences
	auto sepSeq = getStatePairsShortestSeparatingSequences(fsm, true);
	for (const auto& seq : sepSeq) {
		fprintf(outFile, "%u\n", seq.size());
	}
	fflush(outFile);
}


using namespace std::tr2::sys;

void analyseDirMachines(int argc, char** argv) {
	string outDir = "";
	auto dir = string(argv[2]);
	unsigned int machineTypeMask = unsigned int(-1);// all
	state_t statesRestrictionLess = NULL_STATE, statesRestrictionGreater = NULL_STATE;
	input_t inputsRestrictionLess = STOUT_INPUT, inputsRestrictionGreater = STOUT_INPUT;
	output_t outputsRestrictionLess = DEFAULT_OUTPUT, outputsRestrictionGreater = DEFAULT_OUTPUT;
	bool reducedOnly = false;
	for (int i = 3; i < argc; i++) {
		if (strcmp(argv[i], "-dir") == 0) {
			outDir = string(argv[++i]);
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
		else if (strcmp(argv[i], "-i") == 0) {//inputs
			inputsRestrictionLess = state_t(atoi(argv[++i]));
			inputsRestrictionGreater = inputsRestrictionLess - 1;
			inputsRestrictionLess++;
		}
		else if (strcmp(argv[i], "-il") == 0) {//inputs
			inputsRestrictionLess = state_t(atoi(argv[++i]));
		}
		else if (strcmp(argv[i], "-ig") == 0) {//inputs
			inputsRestrictionGreater = state_t(atoi(argv[++i]));
		}
		else if (strcmp(argv[i], "-o") == 0) {//outputs
			outputsRestrictionLess = state_t(atoi(argv[++i]));
			outputsRestrictionGreater = outputsRestrictionLess - 1;
			outputsRestrictionLess++;
		}
		else if (strcmp(argv[i], "-ol") == 0) {//outputs
			outputsRestrictionLess = state_t(atoi(argv[++i]));
		}
		else if (strcmp(argv[i], "-og") == 0) {//outputs
			outputsRestrictionGreater = state_t(atoi(argv[++i]));
		}
		else if (strcmp(argv[i], "-re") == 0) {//reduced only?
			reducedOnly = bool(atoi(argv[++i]) != 0);
		}
	}
	if (outDir.empty()) outDir = dir + "machinesAnalysis/";
	map<int, FILE*> files;
	map<int, vector<size_t>> counts;
	bool severalFiles = false;
	path dirPath(dir);
	directory_iterator endDir;
	for (directory_iterator it(dirPath); it != endDir; ++it) {
		if (is_regular_file(it->status())) {
			path fn(it->path());
			if (fn.extension().compare(".fsm") == 0) {
				auto fsm = FSMmodel::loadFSM(fn.string());
				if ((fsm) && (machineTypeMask & (1 << fsm->getType())) && (!reducedOnly || fsm->isReduced()) &&
					((statesRestrictionLess == NULL_STATE) || (fsm->getNumberOfStates() < statesRestrictionLess)) &&
					((statesRestrictionGreater == NULL_STATE) || (statesRestrictionGreater < fsm->getNumberOfStates())) &&
					((inputsRestrictionLess == STOUT_INPUT) || (fsm->getNumberOfInputs() < inputsRestrictionLess)) &&
					((inputsRestrictionGreater == STOUT_INPUT) || (inputsRestrictionGreater < fsm->getNumberOfInputs())) &&
					((outputsRestrictionLess == DEFAULT_OUTPUT) || (fsm->getNumberOfOutputs() < outputsRestrictionLess)) &&
					((outputsRestrictionGreater == DEFAULT_OUTPUT) || (outputsRestrictionGreater < fsm->getNumberOfOutputs())))
				{
					if (severalFiles) {
						auto it = files.find(fsm->getNumberOfStates() * 20 + 2 * fsm->getType());
						if (it == files.end()) {
							auto outFilename = outDir + machineTypeNames[fsm->getType()] + "_" + to_string(fsm->getNumberOfStates()) + ".csv";
							if (fopen_s(&outFile, outFilename.c_str(), "w") != 0) {
								cerr << "Unable to open file " << outFilename << " for analysis!" << endl;
								return;
							}
							fprintf(outFile, "AccessSeq\tSVS\n");
							files.emplace(fsm->getNumberOfStates() * 20 + 2 * fsm->getType(), outFile);
						}
						else {
							outFile = it->second;
						}
						analyse(fsm);
						it = files.find(fsm->getNumberOfStates() * 20 + 2 * fsm->getType() + 1);
						if (it == files.end()) {
							auto outFilename = outDir + machineTypeNames[fsm->getType()] + "_" + to_string(fsm->getNumberOfStates()) + "_SepSeq.csv";
							if (fopen_s(&outFile, outFilename.c_str(), "w") != 0) {
								cerr << "Unable to open file " << outFilename << " for analysis!" << endl;
								return;
							}
							fprintf(outFile, "SepSeq\n");
							files.emplace(fsm->getNumberOfStates() * 20 + 2 * fsm->getType() + 1, outFile);
						}
						else {
							outFile = it->second;
						}
						analyseSepSeq(fsm);
					}
					else {
						analyseCounts(fsm, counts);
					}
					printf(".");
				}
			}
		}
	}
	for (auto p : files) {
		fclose(p.second);
	}
	if (!severalFiles) {
		auto outFilename = outDir + "SeqCounts.csv";
		if (fopen_s(&outFile, outFilename.c_str(), "w") != 0) {
			cerr << "Unable to open file " << outFilename << " for analysis!" << endl;
			return;
		}
		for (auto p : counts) {
			auto numStates = p.first / 30;
			auto machineType = p.first % 30;
			auto seqType = machineType % 3;
			machineType /= 3;
			fprintf(outFile, "%s_%d_%s", machineTypeNames[machineType], numStates, 
				(seqType == 0) ? "as" : ((seqType == 1) ? "svs" : "sepSeq"));
			for (auto num : p.second) {
				fprintf(outFile, "\t%d", num);
			}
			fprintf(outFile, "\n");
		}
	}
	printf("complete.\n");
}
