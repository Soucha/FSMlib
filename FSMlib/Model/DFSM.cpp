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

#include <iostream>
#include <time.h>

#include "DFSM.h"

static bool isReacheableWithoutEdge(DFSM* fsm, num_states_t start, num_inputs_t input) {
	vector<bool> isReachable(fsm->getNumberOfStates(), false);
	queue<num_states_t> fifo;
	num_states_t state, nextState, end = fsm->getNextState(start, input);
	isReachable[0] = true;
	fifo.push(0);
	while (!fifo.empty()) {
		state = fifo.front();
		fifo.pop();
		for (num_inputs_t i = 0; i < fsm->getNumberOfInputs(); i++) {
			nextState = num_states_t(fsm->getNextState(state_t(state), input_t(i)));
			if (!isReachable[nextState]) {
				if ((nextState == end) && ((start != state) || (input != i))) {
					return true;
				}
				isReachable[nextState] = true;
				fifo.push(nextState);
			}
		}
	}
	return false;
}

state_t DFSM::getNextState(state_t state, input_t input) {
	if ((num_states_t(state) >= _usedStateIDs.size()) || (!_usedStateIDs[num_states_t(state)])) {
		cerr << typeNames[_type] << "::getNextState - bad state id" << endl;
		return WRONG_STATE;
	}
	if (input == STOUT_INPUT) {
		return state;
	}
	if (num_inputs_t(input) >= _numberOfInputs) {
		cerr << typeNames[_type] << "::getNextState - bad input" << endl;
		return WRONG_STATE;
	}
	return _transition[num_states_t(state)][num_inputs_t(input)];
}

state_t DFSM::getEndPathState(state_t state, sequence_in_t path) {
	for (sequence_in_t::iterator inputIt = path.begin(); inputIt != path.end(); inputIt++) {
		state = this->getNextState(state, *inputIt);
		if (state == WRONG_STATE) return state;
	}
	return state;
}

output_t DFSM::getOutput(state_t state, input_t input) {
	if ((num_states_t(state) >= _usedStateIDs.size()) || (!_usedStateIDs[num_states_t(state)])) {
		cerr << typeNames[_type] << "::getOutput - bad state id" << endl;
		return WRONG_OUTPUT;
	}
	if (input == STOUT_INPUT) {
		return _outputState[num_states_t(state)];
	}
	if (num_inputs_t(input) >= _numberOfInputs) {
		cerr << typeNames[_type] << "::getOutput - bad input" << endl;
		return WRONG_OUTPUT;
	}
	num_states_t nextState = num_states_t(_transition[num_states_t(state)][num_inputs_t(input)]);
	if ((state_t(nextState) == NULL_STATE) || (nextState >= _usedStateIDs.size()) || (!_usedStateIDs[nextState])) {
		cerr << typeNames[_type] << "::getOutput - there is no such transition" << endl;
		return WRONG_OUTPUT;
	}
	return _outputTransition[num_states_t(state)][num_inputs_t(input)];
}

sequence_out_t DFSM::getOutputAlongPath(state_t state, sequence_in_t path) {
	sequence_out_t sOut;
	for (sequence_in_t::iterator inputIt = path.begin(); inputIt != path.end(); inputIt++) {
		sOut.push_back(this->getOutput(state, *inputIt));
		state = this->getNextState(state, *inputIt);
		if ((state == WRONG_STATE) || (sOut.back() == WRONG_OUTPUT)) {
			sOut.clear();
			sOut.push_back(WRONG_OUTPUT);
			return sOut;
		}
	}
	return sOut;
}

bool DFSM::removeUnreachableStates() {
	vector<bool> isReachable(_usedStateIDs.size(), false);
	queue<num_states_t> fifo;
	isReachable[0] = true;
	fifo.push(0);
	while (!fifo.empty()) {
		auto state = fifo.front();
		fifo.pop();
		for (num_inputs_t input = 0; input < _numberOfInputs; input++) {
			state_t& nextState = _transition[state][input];
			if ((nextState != NULL_STATE) && (!isReachable[num_states_t(nextState)])) {
				isReachable[num_states_t(nextState)] = true;
				fifo.push(num_states_t(nextState));
			}
		}
	}
	for (num_states_t s = 0; s < _usedStateIDs.size(); s++) {
		if (_usedStateIDs[s] && !isReachable[s]) {
			for (num_inputs_t i = 0; i < _numberOfInputs; i++) {
				_transition[s][i] = NULL_STATE;
				if (!_outputTransition.empty()) _outputTransition[s][i] = DEFAULT_OUTPUT;
			}
			if (!_outputState.empty()) _outputState[s] = DEFAULT_OUTPUT;
			_usedStateIDs[s] = false;
			_numberOfStates--;
		}
	}
	return true;
}

bool DFSM::distinguishByStateOutputs(queue< vector< num_states_t > >& blocks) {
	auto block = blocks.front();
	blocks.pop();
	vector< vector< num_states_t > > sameOutput(_numberOfOutputs + 1);
	for each (num_states_t state in block) {
		auto output = this->getOutput(state_t(state), STOUT_INPUT);
		if (output == WRONG_OUTPUT) return false;
		if (output == DEFAULT_OUTPUT) {
			sameOutput[_numberOfOutputs].push_back(state);
		}
		else {
			sameOutput[num_outputs_t(output)].push_back(state);
		}
	}
	for (num_outputs_t output = 0; output < _numberOfOutputs + 1; output++) {
		if (!sameOutput[output].empty()) {
			vector< num_states_t > tmp(sameOutput[output]);
			blocks.push(tmp);
		}
	}
	return true;
}

bool DFSM::distinguishByTransitionOutputs(queue< vector< num_states_t > >& blocks) {
	num_states_t stop, counter;
	vector< vector< num_states_t > > sameOutput(_numberOfOutputs + 2);
	stop = blocks.size();
	for (num_inputs_t input = 0; input < _numberOfInputs; input++) {
		counter = 0;
		do {
			auto block = blocks.front();
			blocks.pop();
			for each (num_states_t state in block) {
				auto output = this->getOutput(state_t(state), input_t(input));
				if (output == WRONG_OUTPUT) {// there is no transition
					sameOutput[_numberOfOutputs + 1].push_back(state);
				} else if (output == DEFAULT_OUTPUT) {
					sameOutput[_numberOfOutputs].push_back(state);
				}
				else {
					sameOutput[num_outputs_t(output)].push_back(state);
				}
			}
			for (num_outputs_t output = 0; output < _numberOfOutputs + 2; output++) {
				if (!sameOutput[output].empty()) {
					vector< num_states_t > tmp(sameOutput[output]);
					blocks.push(tmp);
					sameOutput[output].clear();
					counter++;
				}
			}
		} while (--stop != 0);
		stop = counter;
		if (counter == _numberOfStates) return true;// FSM reduced
	}
	return true;
}

bool DFSM::distinguishByTransitions(queue< vector< num_states_t > >& blocks) {
	vector< num_states_t > stateGroup(_usedStateIDs.size());
	vector< vector< num_states_t > > sameOutput(_numberOfStates + 1);
	set<num_states_t> outputGroups;
	num_states_t counter, stop, nextStateGroup, group;
	num_inputs_t input, inputStop;
	stop = blocks.size();
	for (counter = 0; counter < stop; counter++) {
		auto block = blocks.front();
		blocks.pop();
		for each (num_states_t state in block) {
			stateGroup[state] = counter;
		}
		if (block.size() > 1) blocks.push(block);
	}
	stop = blocks.size() + 1;// +1 due to the stop condition (--stop)
	group = counter;
	counter = 0;// the number of blocks for the next round
	
	bool newGroup = false;
	input = 0;
	inputStop = _numberOfInputs;
	while (!blocks.empty()) {
		auto block = blocks.front();
		blocks.pop();
		if (--stop == 0) {// iterate input
			if (newGroup) {
				newGroup = false;
				inputStop = _numberOfInputs;
			}
			else if (--inputStop == 0) {
				blocks.push(block);
				break;// blocks contain only indistinguishable blocks of states
			}
			input++;
			input %= _numberOfInputs;
			stop = counter;
			counter = 0;
		}
		if (block.size() == 1) continue;
		outputGroups.clear();
		for each (num_states_t state in block) {
			auto nextState = this->getNextState(state_t(state), input_t(input));
			if (nextState == NULL_STATE) {
				nextStateGroup = _numberOfStates;
			}
			else {
				nextStateGroup = stateGroup[num_states_t(nextState)];
			}
			sameOutput[nextStateGroup].push_back(state);
			outputGroups.insert(nextStateGroup);
		}
		for (set<num_states_t>::iterator it = outputGroups.begin(); it != outputGroups.end(); it++) {
			if (it != outputGroups.begin()) {
				for each (num_states_t state in sameOutput[*it]) {
					stateGroup[state] = group;
				}
				group++;
				newGroup = true;
				//if (group == _numberOfStates) return true;
				//   there are only singletons in blocks but they must be popped first
			}
			if (sameOutput[*it].size() != 1) {
				vector<state_t> tmp(sameOutput[*it]);
				blocks.push(tmp);
				counter++;
			}
			sameOutput[*it].clear();
		}
	}
	return true;
}

void DFSM::mergeEquivalentStates(queue< vector< num_states_t > >& equivalentStates) {
	vector< num_states_t > stateEquiv(_usedStateIDs.size());
	for (num_states_t state = 0; state < stateEquiv.size(); state++) {
		stateEquiv[state] = state;
	}
	while (!equivalentStates.empty()) {
		auto block = equivalentStates.front();
		equivalentStates.pop();
		for (num_states_t state = 1; state < block.size(); state++) {
			stateEquiv[block[state]] = block[0];
			if (!_outputState.empty()) _outputState[block[state]] = DEFAULT_OUTPUT;
		}
		_numberOfStates -= (block.size() - 1);
	}
	for (num_states_t s = 0; s < _usedStateIDs.size(); s++) {
		if (_usedStateIDs[s]) {
			for (num_inputs_t i = 0; i < _numberOfInputs; i++) {
				state_t& nextState = _transition[s][i];
				if (stateEquiv[s] != s) {
					_transition[s][i] = NULL_STATE;
					if (!_outputTransition.empty()) _outputTransition[s][i] = DEFAULT_OUTPUT;
				} else if ((nextState != NULL_STATE) && 
					(stateEquiv[num_states_t(nextState)] != num_states_t(nextState))) {
					_transition[s][i] = state_t(stateEquiv[num_states_t(nextState)]);
				}
			}
		}
	}
}

void DFSM::makeCompact() {
	if (_numberOfStates == _usedStateIDs.size()) return;

	vector< num_states_t > stateEquiv(_usedStateIDs.size());
	for (num_states_t state = 0; state < stateEquiv.size(); state++) {
		stateEquiv[state] = state;
	}
	num_states_t oldState = _usedStateIDs.size()-1, newState = 0;
	while (_usedStateIDs[newState]) newState++;
	while (!_usedStateIDs[oldState]) oldState--;
	while (newState < oldState) {
		// transfer state
		if (!_outputState.empty()) _outputState[newState] = _outputState[oldState];
		for (num_inputs_t i = 0; i < _numberOfInputs; i++) {
			_transition[newState][i] = _transition[oldState][i];
			if (!_outputTransition.empty()) _outputTransition[newState][i] = _outputTransition[oldState][i];
		}
		stateEquiv[oldState] = newState;
		_usedStateIDs[newState] = true;
		_usedStateIDs[oldState] = false;
		while (_usedStateIDs[newState]) newState++;
		while (!_usedStateIDs[oldState]) oldState--;
	}

	if (!_outputState.empty()) _outputState.resize(_numberOfStates);
	if (!_outputTransition.empty()) _outputTransition.resize(_numberOfStates);
	_transition.resize(_numberOfStates);
	_usedStateIDs.resize(_numberOfStates);
	num_inputs_t greatestInput = 0;
	num_outputs_t greatestOutput = 0;
	for (num_states_t s = 0; s < _numberOfStates; s++) {
		for (num_inputs_t i = 0; i < _numberOfInputs; i++) {
			state_t& nextState = _transition[s][i];
			if ((nextState != NULL_STATE) &&
				(stateEquiv[num_states_t(nextState)] != num_states_t(nextState))) {
				_transition[s][i] = state_t(stateEquiv[num_states_t(nextState)]);
			}
			if ((nextState != NULL_STATE) && (i > greatestInput)) {
				greatestInput = i;
			}
			if (!_outputTransition.empty() && (_outputTransition[s][i] != DEFAULT_OUTPUT)
				&& (num_outputs_t(_outputTransition[s][i]) > greatestOutput)) {
				greatestOutput = num_outputs_t(_outputTransition[s][i]);
			}
		}
		if (!_outputState.empty() && (_outputState[s] != DEFAULT_OUTPUT) 
			&& (num_outputs_t(_outputState[s]) > greatestOutput)) {
			greatestOutput = num_outputs_t(_outputState[s]);
		}
	}
	if (greatestInput < _numberOfInputs) {
		_numberOfInputs = greatestInput;
		for (num_states_t s = 0; s < _numberOfStates; s++) {
			_transition[s].resize(_numberOfInputs);
			if (!_outputTransition.empty()) _outputTransition[s].resize(_numberOfInputs);
		}
	}
	_numberOfOutputs = greatestOutput;
}

bool DFSM::mimimize() {
	if (!removeUnreachableStates()) return false;

	queue< vector< num_states_t > > blocks;
	vector< num_states_t > block;	
	for (num_states_t state = 0; state < _usedStateIDs.size(); state++) {
		if (_usedStateIDs[state]) block.push_back(state);
	}
	blocks.push(block);

	if (!_outputState.empty()) {
		if (!distinguishByStateOutputs(blocks)) return false;
		if (blocks.size() == _numberOfStates) {
			_isReduced = true;
			return true;
		}
	}
	if (!_outputTransition.empty()) {
		if (!distinguishByTransitionOutputs(blocks)) return false;
		if (blocks.size() == _numberOfStates) {
			_isReduced = true;
			return true;
		}
	}
	if (!distinguishByTransitions(blocks)) return false;
	if (blocks.empty()) {
		_isReduced = true;
		return true;
	}
	mergeEquivalentStates(blocks);
	makeCompact();

	_isReduced = true;
	return true;
}

void DFSM::clearTransitions() {
	_transition.resize(_numberOfStates);
	for (num_states_t s = 0; s < _numberOfStates; s++) {
		_transition[s].resize(_numberOfInputs);
		for (num_inputs_t i = 0; i < _numberOfInputs; i++) {
			_transition[s][i] = NULL_STATE;
		}
	}
}

void DFSM::clearStateOutputs() {
	_outputState.resize(_numberOfStates);
	for (num_states_t s = 0; s < _numberOfStates; s++) {
		_outputState[s] = DEFAULT_OUTPUT;
	}
}

void DFSM::clearTransitionOutputs() {
	_outputTransition.resize(_numberOfStates);
	for (num_states_t s = 0; s < _numberOfStates; s++) {
		_outputTransition[s].resize(_numberOfInputs);
		for (num_inputs_t i = 0; i < _numberOfInputs; i++) {
			_outputTransition[s][i] = DEFAULT_OUTPUT;
		}
	}
}

void DFSM::create(num_states_t numberOfStates, num_inputs_t numberOfInputs, num_outputs_t numberOfOutputs) {
	if (numberOfOutputs > (numberOfStates * (1 + numberOfInputs))) {
		cerr << typeNames[_type] << "::create - the number of outputs reduced to maximum of " 
			<< (numberOfStates * (1 + numberOfInputs)) << endl;
		numberOfOutputs = (numberOfStates * (1 + numberOfInputs));
	}
	if (numberOfInputs == 0) {
		cerr << typeNames[_type] << "::create - the number of inputs needs to be greater than 0 (set to 1)" << endl;
		numberOfInputs = 1;
	}
	if (numberOfOutputs == 0) {
		cerr << typeNames[_type] << "::create - the number of outputs needs to be greater than 0 (set to 1)" << endl;
		numberOfOutputs = 1;
	}

	_numberOfStates = numberOfStates;
	_numberOfInputs = numberOfInputs;
	_numberOfOutputs = numberOfOutputs;
	_type = TYPE_DFSM;
	_isReduced = false;
	_usedStateIDs.resize(_numberOfStates, true);

	clearTransitions();
	clearStateOutputs();
	clearTransitionOutputs();	
}

void DFSM::generateTransitions() {
	num_states_t state, actState, nextState, stopState, coherentStates;
	vector<num_states_t> endCounter(_numberOfStates, 0);
	num_inputs_t input;
	stack<num_states_t> lifo;
	
	// random transition for each state
	_transition.resize(_numberOfStates);
	for (actState = 0; actState < _numberOfStates; actState++) {
		_transition[actState].resize(_numberOfInputs);
		for (input = 0; input < _numberOfInputs; input++) {
			_transition[actState][input] = state_t(rand() % _numberOfStates);// int -> state_t
		}
	}

	// automaton is coherent and 
	// every state has at least one transition into
	lifo.push(0); // first state
	endCounter[0] = 1;
	state = 1;
	coherentStates = 0;
	do {
		while (!lifo.empty()) {
			actState = lifo.top();
			lifo.pop();
			coherentStates++;
			for (input = 0; input < _numberOfInputs; input++) {
				nextState = num_states_t(_transition[actState][input]);// state_t -> num_states_t
				if (endCounter[nextState] == 0) lifo.push(nextState);
				if (actState != nextState) endCounter[nextState]++;
			}
		}
		if (coherentStates == _numberOfStates) break;
		input = 0;
		stopState = actState = num_states_t(rand() % _numberOfStates);
		while ((endCounter[actState] == 0) || (endCounter[num_states_t(_transition[actState][input])] <= 1)
			|| (!isReacheableWithoutEdge(this, actState, input))) {
			input++;
			if ((input == _numberOfInputs) || (endCounter[actState] == 0)) {
				actState++;
				actState %= _numberOfStates;
				if (stopState == actState) {// there are only self loops on all reachable states
					do {
						if (endCounter[actState] >= 1) {
							for (input = 0; input < _numberOfInputs; input++) {
								if (num_states_t(_transition[actState][input]) == actState) {
									break;
								}
							}
							if (input < _numberOfInputs) {
								break;
							}
						}
						actState++;
						actState %= _numberOfStates;
					} while (stopState != actState);
					endCounter[actState]++;
					break;
				}
				input = 0;
			}
		}
		// find a separate state
		while (endCounter[state] > 0) state++;
		// remove transition
		endCounter[num_states_t(_transition[actState][input])]--;
		// connect the separate state
		_transition[actState][input] = state_t(state);
		lifo.push(state);
		endCounter[state++]++;
	} while ((coherentStates + 1) != _numberOfStates);
}

void DFSM::generateStateOutputs(num_outputs_t nOutputs) {
	num_states_t state;
	num_outputs_t output;
	vector<num_outputs_t> outputs(nOutputs);

	// random output on each transition
	_outputState.resize(_numberOfStates);
	for (state = 0; state < _numberOfStates; state++) {
		output = num_outputs_t(rand() % nOutputs);
		_outputState[state] = output_t(output);
		outputs[output]++;		
	}
	// each output is assigned at least once
	state = 0;
	for (output = 0; output < nOutputs; output++) {
		if (outputs[output] == 0) {
			while (outputs[num_outputs_t(_outputState[state])] <= 1) {
				state++;
			}
			outputs[num_outputs_t(_outputState[state])]--;
			_outputState[state] = output_t(output);
		}
	}
}

void DFSM::generateTransitionOutputs(num_outputs_t nOutputs, num_outputs_t firstOutput) {
	num_states_t state;
	num_inputs_t input;
	num_outputs_t output;
	vector<num_outputs_t> outputs(nOutputs);

	// random output on each transition
	_outputTransition.resize(_numberOfStates);
	for (state = 0; state < _numberOfStates; state++) {
		_outputTransition[state].resize(_numberOfInputs);
		for (input = 0; input < _numberOfInputs; input++) {
			output = num_outputs_t(rand() % nOutputs);
			_outputTransition[state][input] = output_t(firstOutput + output);
			outputs[output]++;
		}
	}
	// each output is assigned at least once
	state = 0;
	input = 0;
	for (output = 0; output < nOutputs; output++) {
		if (outputs[output] == 0) {
			while (outputs[num_outputs_t(_outputTransition[state][input] - firstOutput)] <= 1) {
				if (++input == _numberOfInputs) {
					state++;
					input = 0;
				}
			}
			outputs[num_outputs_t(_outputTransition[state][input] - firstOutput)]--;
			_outputTransition[state][input] = output_t(firstOutput + output);
		}
	}
}

void DFSM::generate(num_states_t numberOfStates, num_inputs_t numberOfInputs, num_outputs_t numberOfOutputs) {
	if (numberOfInputs == 0) {
		cerr << typeNames[_type] << "::generate - the number of inputs needs to be greater than 0 (set to 1)" << endl;
		numberOfInputs = 1;
	}
	if (numberOfOutputs == 0) {
		cerr << typeNames[_type] << "::generate - the number of outputs needs to be greater than 0 (set to 1)" << endl;
		numberOfOutputs = 1;
	}
	if (numberOfStates == 0) {
		cerr << typeNames[_type] << "::generate - the number of states needs to be greater than 0 (set to 1)" << endl;
		numberOfStates = 1;
	}
	if (numberOfOutputs > (numberOfStates * (1 + numberOfInputs))) {
		cerr << typeNames[_type] << "::generate - the number of outputs reduced to maximum of "
			<< (numberOfStates * (1 + numberOfInputs)) << endl;
		numberOfOutputs = (numberOfStates * (1 + numberOfInputs));
	}
	_numberOfStates = numberOfStates;
	_numberOfInputs = numberOfInputs;
	_numberOfOutputs = numberOfOutputs;
	_type = TYPE_DFSM;
	_isReduced = false;
	_usedStateIDs.resize(_numberOfStates, true);

	srand(time(NULL));

	generateTransitions();

	num_outputs_t stateOutputs, transitionOutputs, firstTransitionOutput;
	if (_numberOfOutputs < _numberOfStates) {
		stateOutputs = transitionOutputs = _numberOfOutputs;
		firstTransitionOutput = 0;
	}
	else {
		stateOutputs = _numberOfOutputs / (1 + _numberOfInputs);
		if (stateOutputs < 2) stateOutputs = 1;
		transitionOutputs = _numberOfOutputs - stateOutputs;
		if (transitionOutputs < 2) transitionOutputs = 1;
		firstTransitionOutput = _numberOfOutputs - transitionOutputs;
	}
	generateStateOutputs(stateOutputs);
	generateTransitionOutputs(transitionOutputs, firstTransitionOutput);
}

bool DFSM::loadStateOutputs(ifstream& file) {
	num_states_t tmpState;
	output_t output;
	_outputState.resize(_numberOfStates);
	for (num_states_t state = 0; state < _numberOfStates; state++) {
		file >> tmpState;
		if (tmpState != state) {
			cerr << typeNames[_type] << "::loadStateOutputs - bad state output line " << state << endl;
			return false;
		}
		file >> output;
		if ((num_outputs_t(output) >= _numberOfOutputs) && (output != DEFAULT_OUTPUT)) {
			cerr << typeNames[_type] << "::loadStateOutputs - bad state output line " << state 
				<< ", output " << output << endl;
			return false;
		}
		_outputState[tmpState] = output;
	}
	return true;
}

bool DFSM::loadTransitionOutputs(ifstream& file) {
	num_states_t tmpState;
	output_t output;
	_outputTransition.resize(_numberOfStates);
	for (num_states_t state = 0; state < _numberOfStates; state++) {
		file >> tmpState;
		if (tmpState != state) {
			cerr << typeNames[_type] << "::loadTransitionOutputs - bad transition output line " << state << endl;
			return false;
		}
		_outputTransition[state].resize(_numberOfInputs);
		for (num_inputs_t input = 0; input < _numberOfInputs; input++) {
			file >> output;
			if ((num_outputs_t(output) >= _numberOfOutputs) && (output != DEFAULT_OUTPUT)) {
				cerr << typeNames[_type] << "::loadTransitionOutputs - bad transition output line " << state 
					<< ", output " << output << endl;
				return false;
			}
			_outputTransition[state][input] = output;
		}
	}
	return true;
}

bool DFSM::loadTransitions(ifstream& file) {
	num_states_t tmpState;
	_transition.resize(_numberOfStates);
	for (num_states_t state = 0; state < _numberOfStates; state++) {
		file >> tmpState;
		if (tmpState != state) {
			cerr << typeNames[_type] << "::loadTransitions - bad transition line " << state << endl;
			return false;
		}
		_transition[state].resize(_numberOfInputs);
		for (num_inputs_t input = 0; input < _numberOfInputs; input++) {
			file >> tmpState;
			if ((tmpState >= _numberOfStates) && (state_t(tmpState) != NULL_STATE)) {
				cerr << typeNames[_type] << "::loadTransitions - bad transition line " << state 
					<< ", input " << input << endl;
				return false;
			}
			_transition[state][input] = state_t(tmpState);
			
		}
	}
	_usedStateIDs.resize(_numberOfStates, false);
	_usedStateIDs[0] = true;
	queue<num_states_t> fifo;
	fifo.push(0);
	while (!fifo.empty()) {
		tmpState = fifo.front();
		fifo.pop();
		for (num_inputs_t input = 0; input < _numberOfInputs; input++) {
			state_t& state = _transition[tmpState][input];
			if (_usedStateIDs[state] && (state_t(tmpState) != NULL_STATE)) {
				_usedStateIDs[tmpState] = true;
			}
		}
	}
	return true;
}

bool DFSM::load(string fileName) {
	ifstream file(fileName.c_str());
	if (!file.is_open()) {
		cerr << typeNames[_type] << "::load - unable to open file" << endl;
		return false;
	}
	file >> _type >> _isReduced >> _numberOfStates >> _numberOfInputs >> _numberOfOutputs;
	if (_type != TYPE_DFSM) {
		cerr << typeNames[_type] << "::load - bad type of FSM" << endl;
		file.close();
		return false;
	}
	if (_numberOfInputs == 0) {
		cerr << typeNames[_type] << "::load - the number of inputs needs to be greater than 0" << endl;
		file.close();
		return false;
	}
	if (_numberOfOutputs == 0) {
		cerr << typeNames[_type] << "::load - the number of outputs needs to be greater than 0" << endl;
		file.close();
		return false;
	}
	if (_numberOfStates == 0) {
		cerr << typeNames[_type] << "::load - the number of states needs to be greater than 0" << endl;
		file.close();
		return false;
	}
	if (_numberOfOutputs > (_numberOfStates * (1 + _numberOfInputs))) {
		cerr << typeNames[_type] << "::load - the number of outputs cannot be greater than the maximum value of "
			<< (_numberOfStates * (1 + _numberOfInputs)) << endl;
		file.close();
		return false;
	}
	if (!loadStateOutputs(file) || !loadTransitionOutputs(file) || !loadTransitions(file)) {
		file.close();
		return false;
	}
	file.close();
	return true;
}

string DFSM::getFilename() {
	string filename(typeNames[_type]);
	filename += (_isReduced ? "_R" : "_U");
	filename += Utils::toString(_numberOfStates);
	return filename;
}

void DFSM::saveInfo(ofstream& file) {
	file << _type << ' ' << _isReduced << endl;
	file << _numberOfStates << ' ' << _numberOfInputs << ' ' << _numberOfOutputs << endl;
}

void DFSM::saveStateOutputs(ofstream& file) {
	for (num_states_t state = 0; state < _numberOfStates; state++) {
		file << state << ' ' << _outputState[state] << endl;
	}
}

void DFSM::saveTransitionOutputs(ofstream& file) {
	for (num_states_t state = 0; state < _numberOfStates; state++) {
		file << state;
		for (num_inputs_t input = 0; input < _numberOfInputs; input++) {
			file << '\t' << _outputTransition[state][input];
		}
		file << endl;
	}
}

void DFSM::saveTransitions(ofstream& file) {
	for (num_states_t state = 0; state < _numberOfStates; state++) {
		file << state;
		for (num_inputs_t input = 0; input < _numberOfInputs; input++) {
			file << '\t' << num_states_t(_transition[state][input]);
		}
		file << endl;
	}
}

string DFSM::save(string path) {
	string filename = getFilename();
	filename = Utils::getUniqueName(filename, "fsm", path);
	ofstream file(filename.c_str());
	if (!file.is_open()) {
		cerr << typeNames[_type] << "::save - unable to open file" << endl;
		return "";
	}
	saveInfo(file);
	saveStateOutputs(file);
	saveTransitionOutputs(file);
	saveTransitions(file);
	file.close();
	return filename;
}

void DFSM::writeDotStart(ofstream& file) {
	file << "digraph { rankdir=LR;" << endl;
}

void DFSM::writeDotStates(ofstream& file, bool withOutputs) {
	for (num_states_t state = 0; state < _numberOfStates; state++) {
		file << state << " [label=\"" << state_t(state);
		if (withOutputs) file << "\n" << _outputState[state];
		file << "\"];" << endl;
	}
}

void DFSM::writeDotTransitions(ofstream& file, bool withOutputs) {
	for (num_states_t state = 0; state < _numberOfStates; state++) {
		for (num_inputs_t input = 0; input < _numberOfInputs; input++) {
			file << state << " -> " << num_states_t(_transition[state][input])
				<< " [label=\"" << input_t(input);
			if (withOutputs) file << " / " << _outputTransition[state][input];
			file << "\"];" << endl;
		}
	}
}

void DFSM::writeDotEnd(ofstream& file) {
	file << "}" << endl;
}

string DFSM::writeDOTfile(string path) {
	string filename("DOT");
	filename += getFilename();
	filename = Utils::getUniqueName(filename, "fsm", path);
	ofstream file(filename.c_str());
	if (!file.is_open()) {
		cerr << typeNames[_type] << "::writeDOTfile - unable to open file" << endl;
		return "";
	}
	writeDotStart(file);
	writeDotStates(file, true);
	writeDotTransitions(file, true);
	writeDotEnd(file);
	file.close();
	return filename;
}

state_t DFSM::addState(output_t stateOutput) {
	if ((num_outputs_t(stateOutput) >= _numberOfOutputs) && (stateOutput != DEFAULT_OUTPUT)) {
		cerr << typeNames[_type] << "::addState - bad output (increase the number of outputs first)" << endl;
		return NULL_STATE;
	}
	_isReduced = false;
	if (_usedStateIDs.size() == _numberOfStates) {
		_usedStateIDs.push_back(true);
		if (!_outputState.empty()) _outputState.push_back(stateOutput);
		if (!_outputTransition.empty()) {
			vector< output_t > newStateTransitionOutputs(_numberOfInputs, DEFAULT_OUTPUT);
			_outputTransition.push_back(newStateTransitionOutputs);
		}
		vector< state_t > newStateTransitions(_numberOfInputs, NULL_STATE);
		_transition.push_back(newStateTransitions);
		_numberOfStates++;
		return state_t(_numberOfStates - 1);
	}
	num_states_t newState = 0;
	while (_usedStateIDs[newState]) newState++;
	_usedStateIDs[newState] = true;
	return state_t(newState);
}

bool DFSM::setOutput(state_t state, output_t output, input_t input) {
	if ((num_states_t(state) >= _usedStateIDs.size()) || (!_usedStateIDs[num_states_t(state)])) {
		cerr << typeNames[_type] << "::setOutput - bad state" << endl;
		return false;
	}
	if ((num_outputs_t(output) >= _numberOfOutputs) && (output != DEFAULT_OUTPUT)) {
		cerr << typeNames[_type] << "::setOutput - bad output (increase the number of outputs first)" << endl;
		return false;
	}
	if (input == STOUT_INPUT) {
		_outputState[num_states_t(state)] = output;
		return true;
	}
	if (num_inputs_t(input) >= _numberOfInputs) {
		cerr << typeNames[_type] << "::setOutput - bad input" << endl;
		return false;
	}
	if (_transition[num_states_t(state)][num_inputs_t(input)] == NULL_STATE) {
		cerr << typeNames[_type] << "::setOutput - there is no such transition" << endl;
		return false;
	}
	_outputTransition[num_states_t(state)][num_inputs_t(input)] = output;
	_isReduced = false;
	return true; 
}

bool DFSM::setTransition(state_t from, input_t input, state_t to, output_t output) {
	if (input == STOUT_INPUT) {
		cerr << typeNames[_type] << "::setTransition - use setOutput to set a state output instead" << endl;
		return false;
	}
	if ((num_states_t(from) >= _usedStateIDs.size()) || (!_usedStateIDs[num_states_t(from)])) {
		cerr << typeNames[_type] << "::setTransition - bad state From" << endl;
		return false;
	}
	if (num_inputs_t(input) >= _numberOfInputs) {
		cerr << typeNames[_type] << "::setTransition - bad input" << endl;
		return false;
	}
	if ((num_states_t(to) >= _usedStateIDs.size()) || (!_usedStateIDs[num_states_t(to)])) {
		cerr << typeNames[_type] << "::setTransition - bad state To" << endl;
		return false;
	}
	if ((num_outputs_t(output) >= _numberOfOutputs) && (output != DEFAULT_OUTPUT)) {
		cerr << typeNames[_type] << "::setTransition - bad output (increase the number of outputs first)" << endl;
		return false;
	}
	_transition[num_states_t(from)][num_inputs_t(input)] = to;
	_outputTransition[num_states_t(from)][num_inputs_t(input)] = output;
	_isReduced = false;
	return true;
}

bool DFSM::removeState(state_t state) {
	if ((num_states_t(state) >= _usedStateIDs.size()) || (!_usedStateIDs[num_states_t(state)])) {
		cerr << typeNames[_type] << "::removeState - bad state" << endl;
		return false;
	}
	if (num_states_t(state) == 0) {
		cerr << typeNames[_type] << "::removeState - the initial state cannot be removed" << endl;
		return false;
	}
	if (!_outputState.empty()) _outputState[num_states_t(state)] = DEFAULT_OUTPUT;
	for (num_states_t s = 0; s < _usedStateIDs.size(); s++) {
		if (_usedStateIDs[s]) {
			for (num_inputs_t i = 0; i < _numberOfInputs; i++) {
				if ((_transition[s][i] == state) || (s == num_states_t(state))) {
					_transition[s][i] = NULL_STATE;
					if (!_outputTransition.empty()) _outputTransition[s][i] = DEFAULT_OUTPUT;
				}
			}
		}
	}
	_usedStateIDs[num_states_t(state)] = false;
	_numberOfStates--;
	_isReduced = false;
	return true;
}

bool DFSM::removeTransition(state_t from, input_t input, state_t to, output_t output) {
	if ((num_states_t(from) >= _usedStateIDs.size()) || (!_usedStateIDs[num_states_t(from)])) {
		cerr << typeNames[_type] << "::removeTransition - bad state From" << endl;
		return false;
	}
	if ((num_inputs_t(input) >= _numberOfInputs) || (input == STOUT_INPUT)) {
		cerr << typeNames[_type] << "::removeTransition - bad input" << endl;
		return false;
	}
	if (_transition[num_states_t(from)][num_inputs_t(input)] == NULL_STATE) {
		cerr << typeNames[_type] << "::removeTransition - there is no such transition" << endl;
		return false;
	}
	if ((to != NULL_STATE) && (_transition[num_states_t(from)][num_inputs_t(input)] != to)) {
		cerr << typeNames[_type] << "::removeTransition - state To has no effect here" << endl;
	}
	if (output != DEFAULT_OUTPUT) {
		cerr << typeNames[_type] << "::removeTransition - the output has no effect here" << endl;
	}
	_transition[num_states_t(from)][num_inputs_t(input)] = NULL_STATE;
	if (!_outputTransition.empty()) _outputTransition[num_states_t(from)][num_inputs_t(input)] = DEFAULT_OUTPUT;
	_isReduced = false;
	return true;
}

void DFSM::incNumberOfInputs(num_inputs_t byNum) {
	_numberOfInputs += byNum;
	for (num_states_t state = 0; state < _numberOfStates; state++) {
		for (num_inputs_t i = 0; i < byNum; i++) {
			_transition[state].push_back(NULL_STATE);
			if (!_outputTransition.empty()) _outputTransition[state].push_back(DEFAULT_OUTPUT);
		}
	}
}

void DFSM::incNumberOfOutputs(num_outputs_t byNum) {
	_numberOfOutputs += byNum;
}
