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
#include "stdafx.h"

#include <condition_variable>
#include <thread>
#include "TeacherGridWorld.h"

using namespace FSMlearning;

struct learning_info_t {
	thread algThread;
	sequence_in_t seqToQuery;
	sequence_out_t response;
	output_t prevOutput;
	state_t maxExtraStates;
	bool waitingForResponse = false;
	std::condition_variable cv;
	std::mutex cv_m;
};

//vector<unique_ptr<learning_info_t>> runningAlgs;

learning_info_t li;

#define DATA_PATH string("")
#define OUTPUT_GV string(DATA_PATH + "output.gv").c_str()

bool showConjecture(const unique_ptr<DFSM>& conjecture) {
	auto fn = conjecture->writeDOTfile(DATA_PATH);
	//char c;	cin >> c;
	remove(OUTPUT_GV);
	rename(fn.c_str(), OUTPUT_GV);
	return true;
}

#define PTRandSTR(f) f, #f
#define COMPUTATION_TIME(com) \
	auto start = chrono::system_clock::now(); \
	com; \
	auto end = chrono::system_clock::now(); \
	chrono::duration<double> elapsed_seconds = end - start; 

static vector<string> descriptions;
static vector<function<unique_ptr<DFSM>(const unique_ptr<Teacher>&)>> algorithms;

static void loadAlgorithms(state_t maxExtraStates, seq_len_t maxDistLen, bool isEQallowed, bool writeDot) {
#if 1 // L*
	vector<pair<function<void(const sequence_in_t& ce, ObservationTable& ot, const unique_ptr<Teacher>& teacher)>, string>>	ceFunc;
	ceFunc.emplace_back(PTRandSTR(addAllPrefixesToS));
	ceFunc.emplace_back(PTRandSTR(addAllSuffixesAfterLastStateToE));
	ceFunc.emplace_back(PTRandSTR(addSuffix1by1ToE));
	ceFunc.emplace_back(PTRandSTR(addSuffixAfterLastStateToE));
	ceFunc.emplace_back(PTRandSTR(addSuffixToE_binarySearch));
	for (size_t i = 0; i < ceFunc.size(); i++) {
		descriptions.emplace_back("L*\t" + ceFunc[i].second + "\t" + to_string(descriptions.size()) + "\t");
		algorithms.emplace_back(bind(Lstar, placeholders::_1, ceFunc[i].first, writeDot ? showConjecture : nullptr, (i == 2), (i > 2)));
	}
#endif
#if 1 // OP
	vector<pair<OP_CEprocessing, string>> opCeFunc;
	opCeFunc.emplace_back(PTRandSTR(AllGlobally));
	opCeFunc.emplace_back(PTRandSTR(OneGlobally));
	opCeFunc.emplace_back(PTRandSTR(OneLocally));
	for (size_t i = 0; i < opCeFunc.size(); i++) {
		descriptions.emplace_back("OP\t" + opCeFunc[i].second + "\t" + to_string(descriptions.size()) + "\t");
		algorithms.emplace_back(bind(ObservationPackAlgorithm, placeholders::_1, opCeFunc[i].first, writeDot ? showConjecture : nullptr));
	}
#endif
#if 1 // DT
	descriptions.emplace_back("DT\t\t" + to_string(descriptions.size()) + "\t");
	algorithms.emplace_back(bind(DiscriminationTreeAlgorithm, placeholders::_1, writeDot ? showConjecture : nullptr));
#endif
#if 1 // TTT
	descriptions.emplace_back("TTT\t\t" + to_string(descriptions.size()) + "\t");
	algorithms.emplace_back(bind(TTT, placeholders::_1, writeDot ? showConjecture : nullptr));
#endif
#if 1 // Quotient
	descriptions.emplace_back("Quotient\t\t" + to_string(descriptions.size()) + "\t");
	algorithms.emplace_back(bind(QuotientAlgorithm, placeholders::_1, writeDot ? showConjecture : nullptr));
#endif
#if 1 // GoodSplit
	descriptions.emplace_back("GoodSplit\tmaxDistLen:" + to_string(maxDistLen) + (isEQallowed ? "+EQ" : "") + "\t" + to_string(descriptions.size()) + "\t");
	algorithms.emplace_back(bind(GoodSplit, placeholders::_1, maxDistLen, writeDot ? showConjecture : nullptr, isEQallowed));
#endif
#if 1 // ObservationTreeAlgorithm
	descriptions.emplace_back("OTree\tExtraStates:" + to_string(maxExtraStates) + (isEQallowed ? "+EQ" : "") + "\t" + to_string(descriptions.size()) + "\t");
	algorithms.emplace_back(bind(ObservationTreeAlgorithm, placeholders::_1, maxExtraStates, writeDot ? showConjecture : nullptr, isEQallowed));
#endif
}

static void printCSV(const unique_ptr<Teacher>& teacher, const unique_ptr<DFSM>& fsm,
	double sec, const string& description, shared_ptr<BlackBox> bb = nullptr) {
	printf("%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%f\t%s\n", (fsm->getNumberOfStates() == 25), fsm->getType(),
		fsm->getNumberOfStates(), fsm->getNumberOfInputs(), fsm->getNumberOfOutputs(), teacher->getAppliedResetCount(),
		teacher->getOutputQueryCount(), teacher->getEquivalenceQueryCount(), teacher->getQueriedSymbolsCount(), sec, description.c_str());
}

void learning(bool isResettable, input_t numberOfInputs, int algId) {
	unique_ptr<Teacher> teacher = make_unique<TeacherGridWorld>(isResettable, numberOfInputs);
	COMPUTATION_TIME(auto model = algorithms[algId](teacher));
	printCSV(teacher, model, elapsed_seconds.count(), descriptions[algId]);
	std::unique_lock<std::mutex> lk(li.cv_m);
	li.cv.wait(lk, []{return li.seqToQuery.empty() && !li.waitingForResponse; });
	li.seqToQuery.emplace_back(LEARNING_COMPLETED);
	lk.unlock();
	li.cv.notify_one();
}


int initLearning(bool isResettable, input_t numberOfInputs, state_t maxExtraStates, int algId, bool writeDot) {
	//int id(0);
	printf("Correct\tFSMtype\tStates\tInputs\tOutputs\tResets\tOQs\tEQs\tsymbols\tseconds\t"
		"Algorithm\tCEprocessing\tAlgId\n");
	loadAlgorithms(maxExtraStates, maxExtraStates, false, writeDot);
	li.prevOutput = 0;
	li.maxExtraStates = maxExtraStates;
	li.algThread = thread(learning, isResettable, numberOfInputs, algId);
	return algId;// id;
}

input_t getNextInput(int id) {
	while (li.seqToQuery.empty() || (li.seqToQuery.front() == STOUT_INPUT)) {
		if (li.seqToQuery.empty()) {
			li.cv.notify_one();
			std::unique_lock<std::mutex> lk(li.cv_m);
			li.cv.wait(lk, []{return !li.seqToQuery.empty(); });
		}
		if (li.seqToQuery.front() == STOUT_INPUT) {
			li.response.emplace_back(li.prevOutput);
			li.seqToQuery.pop_front();
		}
	}
	auto input = li.seqToQuery.front();
	if (input == LEARNING_COMPLETED) {
		li.algThread.join();
		input = input_t(-2);
	}
	else if (input == RESET_INPUT) {
		input = input_t(-1);
	}
	return input;
}

void setResponse(int id, output_t output) {
	if (li.seqToQuery.empty()) {
		std::unique_lock<std::mutex> lk(li.cv_m);
		li.cv.wait(lk, []{return !li.seqToQuery.empty(); });
	}
	if ((li.seqToQuery.front() != RESET_INPUT) && (li.seqToQuery.front() != STOUT_INPUT)) {
		if (output == output_t(-2)) {
			li.response.emplace_back(2);
			output = li.prevOutput;
		}
		else if ((output != 0) && (li.seqToQuery.front() == 5)) {
			li.response.emplace_back(1);
		}
		else {
			li.response.emplace_back(0);
		}
	}
	li.seqToQuery.pop_front();
	if (!li.seqToQuery.empty() && (li.seqToQuery.front() == STOUT_INPUT)) {
		li.response.emplace_back(output);
		li.seqToQuery.pop_front();
	}
	li.prevOutput = output;
}

input_t updateAndGetNextInput(int id, output_t output) {
	if (li.seqToQuery.empty()) {
		std::unique_lock<std::mutex> lk(li.cv_m);
		li.cv.wait(lk, []{return !li.seqToQuery.empty(); });
	}
	if ((li.seqToQuery.front() != RESET_INPUT) && (li.seqToQuery.front() != STOUT_INPUT)) {
		if (output == output_t(-2)) {
			li.response.emplace_back(2);
			output = li.prevOutput;
		} else if ((output != 0) && (li.seqToQuery.front() == 5)) {
			li.response.emplace_back(1);
		} else {
			li.response.emplace_back(0);
		}
	}
	li.seqToQuery.pop_front();
	if (!li.seqToQuery.empty() && (li.seqToQuery.front() == STOUT_INPUT)) {
		li.response.emplace_back(output);
		li.seqToQuery.pop_front();
	}
	li.prevOutput = output;

	while (li.seqToQuery.empty() || (li.seqToQuery.front() == STOUT_INPUT)) {
		if (li.seqToQuery.empty()) {
			li.cv.notify_one();
			std::unique_lock<std::mutex> lk(li.cv_m);
			li.cv.wait(lk, []{return !li.seqToQuery.empty(); });
		}
		if (li.seqToQuery.front() == STOUT_INPUT) {
			li.response.emplace_back(output);
			li.seqToQuery.pop_front();
		}
	}
	auto input = li.seqToQuery.front();
	if (input == LEARNING_COMPLETED) {
		li.algThread.join();
		input = input_t(-2);
	} else if (input == RESET_INPUT) {
		input = input_t(-1);
	}
	return input;
}

void stopLearning(int id) {
	if (li.algThread.joinable()) {
		li.algThread.detach();
		li.algThread.~thread();
	}
}

vector<input_t> TeacherGridWorld::getNextPossibleInputs() {
	vector<input_t> inputs;
	for (input_t i = 0; i < _numOfInputs; i++) {
		inputs.push_back(i);
	}
	return inputs;
}

void TeacherGridWorld::resetBlackBox() {
	if (!_isResettable) {
		ERROR_MESSAGE("TeacherGridWorld::reset - cannot be reset!");
		return;
	}
	_resetCounter++;
	
	std::unique_lock<std::mutex> lk(li.cv_m);
	li.seqToQuery.push_back(RESET_INPUT);
	li.cv.notify_one();
	li.cv.wait(lk, []{return li.seqToQuery.empty() && !li.waitingForResponse; });
	li.response.clear();
}

output_t TeacherGridWorld::outputQuery(input_t input) {
	_outputQueryCounter++;
	_querySymbolCounter++;
	std::unique_lock<std::mutex> lk(li.cv_m);
	li.seqToQuery.push_back(input);
	li.cv.notify_one();
	li.cv.wait(lk, []{return li.seqToQuery.empty() && !li.waitingForResponse; });
	output_t output = li.response.back();
	li.response.clear();
	if (_numOfObservedOutputs < output + 1) {
		_numOfObservedOutputs = output + 1;
	}
	return output;
}

sequence_out_t TeacherGridWorld::outputQuery(const sequence_in_t& inputSequence) {
	_outputQueryCounter++;
	if (inputSequence.empty()) return sequence_out_t();
	std::unique_lock<std::mutex> lk(li.cv_m);
	li.seqToQuery = inputSequence;
	li.cv.notify_one();
	li.cv.wait(lk, []{return li.seqToQuery.empty() && !li.waitingForResponse; });
	auto outputSequence = move(li.response);
	for (auto output : outputSequence) {
		if (_numOfObservedOutputs < output + 1) {
			_numOfObservedOutputs = output + 1;
		}
	}
	_querySymbolCounter += outputSequence.size();
	return outputSequence;
}

output_t TeacherGridWorld::resetAndOutputQuery(input_t input) {
	resetBlackBox();
	return outputQuery(input);
}

sequence_out_t TeacherGridWorld::resetAndOutputQuery(const sequence_in_t& inputSequence) {
	resetBlackBox();
	return outputQuery(inputSequence);
}

output_t TeacherGridWorld::resetAndOutputQueryOnSuffix(const sequence_in_t& prefix, input_t input) {
	resetAndOutputQuery(prefix);
	_outputQueryCounter--;
	return outputQuery(input);
}

sequence_out_t TeacherGridWorld::resetAndOutputQueryOnSuffix(const sequence_in_t& prefix, const sequence_in_t& suffix) {
	resetAndOutputQuery(prefix);
	_outputQueryCounter--;
	return outputQuery(suffix);
}

sequence_in_t TeacherGridWorld::equivalenceQuery(const unique_ptr<DFSM>& conjecture) {
	_equivalenceQueryCounter++;
	if (_isResettable) {
		auto model = FSMmodel::duplicateFSM(conjecture);
		if (!model->isReduced()) model->minimize();
		auto TS = FSMtesting::SPY_method(model, li.maxExtraStates);
		for (const auto& test : TS) {
			auto bbOut = resetAndOutputQuery(test);
			_outputQueryCounter--;
			sequence_out_t modelOut;
			state_t state = 0;
			for (const auto& input : test) {
				auto ns = model->getNextState(state, input);
				if ((ns != NULL_STATE) && (ns != WRONG_STATE)) {
					modelOut.emplace_back(model->getOutput(state, input));
					state = ns;
				}
				else modelOut.emplace_back(WRONG_OUTPUT);
			}
			if (bbOut != modelOut) {
				return test;
			}
		}
	}
	return sequence_in_t();
}
