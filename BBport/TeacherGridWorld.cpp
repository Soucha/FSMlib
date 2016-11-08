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


struct learning_info_t {
	thread algThread;
	sequence_in_t seqToQuery;
	sequence_out_t response;
	output_t prevOutput;
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


void learning(bool isResettable, input_t numberOfInputs, state_t maxExtraStates) {
	unique_ptr<Teacher> teacher = make_unique<TeacherGridWorld>(isResettable, numberOfInputs);
	auto model = FSMlearning::ObservationTreeAlgorithm(teacher, maxExtraStates, showConjecture, false);

	std::unique_lock<std::mutex> lk(li.cv_m);
	li.cv.wait(lk, []{return li.seqToQuery.empty() && !li.waitingForResponse; });
	li.seqToQuery.emplace_back(LEARNING_COMPLETED);
	lk.unlock();
	li.cv.notify_one();
}


int initLearning(bool isResettable, input_t numberOfInputs, state_t maxExtraStates) {
	//int id(0);
	li.prevOutput = 0;
	li.algThread = thread(learning, isResettable, numberOfInputs, maxExtraStates);
	return 0;// id;
}

input_t getNextInput(int id) {
	if (li.waitingForResponse) return RESPONSE_REQUESTED;
	if (li.seqToQuery.empty()) {
		std::unique_lock<std::mutex> lk(li.cv_m);
		printf("get: wait\n");
		li.cv.wait(lk, []{return !li.seqToQuery.empty(); });
	}
	auto input = li.seqToQuery.front();
	if (input != LEARNING_COMPLETED)
		li.waitingForResponse = true;
	else {
		li.algThread.join();
	}
	li.seqToQuery.pop_front();
	return input;
}

void setResponse(int id, output_t output) {
	li.response.emplace_back(output);
	{
		printf("set: wait = false\n");
		std::lock_guard<std::mutex> lk(li.cv_m);
		li.waitingForResponse = false;
	}
	li.cv.notify_one();
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

vector<input_t> TeacherGridWorld::getNextPossibleInputs() {
	vector<input_t> inputs;
	for (input_t i = 0; i < _numOfInputs; i++) {
		inputs.push_back(i);
	}
	return inputs;
}

void TeacherGridWorld::resetBlackBox() {
	if (!_isResettable) {
		//ERROR_MESSAGE("TeacherGridWorld::reset - cannot be reset!");
		printf("TeacherGridWorld::reset - cannot be reset!");
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
	sequence_out_t output;
	for (const auto& input : inputSequence) {
		output.emplace_back(outputQuery(input));
		_outputQueryCounter--;
	}
	return output;
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
	return sequence_in_t();
}
