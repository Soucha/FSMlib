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

#include "DFSM.h"

// helps to generate connected state diagram
static bool isReacheableWithoutEdge(DFSM* fsm, state_t start, input_t input) {
	vector<bool> isReachable(fsm->getNumberOfStates(), false);
	queue<state_t> fifo;
	state_t state, nextState, end = fsm->getNextState(start, input);
	isReachable[0] = true;
	fifo.push(0);
	while (!fifo.empty()) {
		state = fifo.front();
		fifo.pop();
		for (input_t i = 0; i < fsm->getNumberOfInputs(); i++) {
			nextState = fsm->getNextState(state, i);
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
	if ((state >= _usedStateIDs.size()) || (!_usedStateIDs[state])) {
		ERROR_MESSAGE("%s::getNextState - bad state id (%d)", machineTypeNames[_type], state);
		return WRONG_STATE;
	}
	if (input == STOUT_INPUT) {
		return state;
	}
	if (input >= _numberOfInputs) {
		ERROR_MESSAGE("%s::getNextState - bad input (%d)", machineTypeNames[_type], input);
		return WRONG_STATE;
	}
	return _transition[state][input];
}

state_t DFSM::getEndPathState(state_t state, sequence_in_t path) {
	for (sequence_in_t::iterator inputIt = path.begin(); inputIt != path.end(); inputIt++) {
		state = this->getNextState(state, *inputIt);
		if (state == NULL_STATE) return WRONG_STATE;
		if (state == WRONG_STATE) return state;
	}
	return state;
}

output_t DFSM::getOutput(state_t state, input_t input) {
	if ((state >= _usedStateIDs.size()) || (!_usedStateIDs[state])) {
		ERROR_MESSAGE("%s::getOutput - bad state id (%d)", machineTypeNames[_type], state);
		return WRONG_OUTPUT;
	}
	if (input == STOUT_INPUT) {
		return _outputState[state];
	}
	if (input >= _numberOfInputs) {
		ERROR_MESSAGE("%s::getOutput - bad input (%d)", machineTypeNames[_type], input);
		return WRONG_OUTPUT;
	}
	state_t& nextState = _transition[state][input];
	if ((nextState == NULL_STATE) || (nextState >= _usedStateIDs.size()) || (!_usedStateIDs[nextState])) {
		ERROR_MESSAGE("%s::getOutput - there is no such transition (%d, %d)", machineTypeNames[_type], state, input);
		return WRONG_OUTPUT;
	}
	return _outputTransition[state][input];
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
	queue<state_t> fifo;
	isReachable[0] = true;
	fifo.push(0);
	while (!fifo.empty()) {
		auto state = fifo.front();
		fifo.pop();
		for (input_t input = 0; input < _numberOfInputs; input++) {
			state_t& nextState = _transition[state][input];
			if ((nextState != NULL_STATE) && (!isReachable[nextState])) {
				isReachable[nextState] = true;
				fifo.push(nextState);
			}
		}
	}
	for (state_t s = 0; s < _usedStateIDs.size(); s++) {
		if (_usedStateIDs[s] && !isReachable[s]) {
			for (input_t i = 0; i < _numberOfInputs; i++) {
				_transition[s][i] = NULL_STATE;
				if (_isOutputTransition) _outputTransition[s][i] = DEFAULT_OUTPUT;
			}
			if (_isOutputState) _outputState[s] = DEFAULT_OUTPUT;
			_usedStateIDs[s] = false;
			_numberOfStates--;
		}
	}
	return true;
}

bool DFSM::distinguishByStateOutputs(queue< vector< state_t > >& blocks) {
	auto block = blocks.front();
	blocks.pop();
	vector< vector< state_t > > sameOutput(_numberOfOutputs + 1);
	for each (state_t state in block) {
		auto output = this->getOutput(state, STOUT_INPUT);
		if (output == WRONG_OUTPUT) return false;
		if (output == DEFAULT_OUTPUT) {
			sameOutput[_numberOfOutputs].push_back(state);
		}
		else {
			sameOutput[output].push_back(state);
		}
	}
	for (output_t output = 0; output < _numberOfOutputs + 1; output++) {
		if (!sameOutput[output].empty()) {
			vector< state_t > tmp(sameOutput[output]);
			blocks.push(tmp);
		}
	}
	return true;
}

bool DFSM::distinguishByTransitionOutputs(queue< vector< state_t > >& blocks) {
	state_t stop, counter;
	vector< vector< state_t > > sameOutput(_numberOfOutputs + 2);
	stop = blocks.size();
	for (input_t input = 0; input < _numberOfInputs; input++) {
		counter = 0;
		do {
			auto block = blocks.front();
			blocks.pop();
			for each (state_t state in block) {
				auto output = this->getOutput(state, input);
				if (output == WRONG_OUTPUT) {// there is no transition
					sameOutput[_numberOfOutputs + 1].push_back(state);
				} else if (output == DEFAULT_OUTPUT) {
					sameOutput[_numberOfOutputs].push_back(state);
				}
				else {
					sameOutput[output].push_back(state);
				}
			}
			for (output_t output = 0; output < _numberOfOutputs + 2; output++) {
				if (!sameOutput[output].empty()) {
					vector< state_t > tmp(sameOutput[output]);
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

bool DFSM::distinguishByTransitions(queue< vector< state_t > >& blocks) {
	vector< state_t > stateGroup(_usedStateIDs.size());
	vector< vector< state_t > > sameOutput(_numberOfStates + 1);
	set<state_t> outputGroups;
	state_t counter, stop, nextStateGroup, group;
	input_t input, inputStop;
	stop = blocks.size();
	for (counter = 0; counter < stop; counter++) {
		auto block = blocks.front();
		blocks.pop();
		for each (state_t state in block) {
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
		for each (state_t state in block) {
			auto nextState = this->getNextState(state, input);
			if (nextState == NULL_STATE) {
				nextStateGroup = _numberOfStates;
			}
			else {
				nextStateGroup = stateGroup[nextState];
			}
			sameOutput[nextStateGroup].push_back(state);
			outputGroups.insert(nextStateGroup);
		}
		for (set<state_t>::iterator it = outputGroups.begin(); it != outputGroups.end(); it++) {
			if (it != outputGroups.begin()) {
				for each (state_t state in sameOutput[*it]) {
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

void DFSM::mergeEquivalentStates(queue< vector< state_t > >& equivalentStates) {
	vector< state_t > stateEquiv(_usedStateIDs.size());
	for (state_t state = 0; state < stateEquiv.size(); state++) {
		stateEquiv[state] = state;
	}
	while (!equivalentStates.empty()) {
		auto block = equivalentStates.front();
		equivalentStates.pop();
		for (state_t state = 1; state < block.size(); state++) {
			stateEquiv[block[state]] = block[0];
			if (_isOutputState) _outputState[block[state]] = DEFAULT_OUTPUT;
		}
		_numberOfStates -= (block.size() - 1);
	}
	for (state_t s = 0; s < _usedStateIDs.size(); s++) {
		if (_usedStateIDs[s]) {
			for (input_t i = 0; i < _numberOfInputs; i++) {
				state_t& nextState = _transition[s][i];
				if (stateEquiv[s] != s) {
					_transition[s][i] = NULL_STATE;
					if (_isOutputTransition) _outputTransition[s][i] = DEFAULT_OUTPUT;
				} else if ((nextState != NULL_STATE) && 
					(stateEquiv[nextState] != nextState)) {
					_transition[s][i] = stateEquiv[nextState];
				}
			}
			if (stateEquiv[s] != s) {
				_usedStateIDs[s] = false;
			}
		}
	}
}

void DFSM::makeCompact() {
	if (_numberOfStates == _usedStateIDs.size()) return;

	vector< state_t > stateEquiv(_usedStateIDs.size());
	for (state_t state = 0; state < stateEquiv.size(); state++) {
		stateEquiv[state] = state;
	}
	state_t oldState = _usedStateIDs.size()-1, newState = 0;
	while (_usedStateIDs[newState]) newState++;
	while (!_usedStateIDs[oldState]) oldState--;
	while (newState < oldState) {
		// transfer state
		if (_isOutputState) _outputState[newState] = _outputState[oldState];
		for (input_t i = 0; i < _numberOfInputs; i++) {
			_transition[newState][i] = _transition[oldState][i];
			if (_isOutputTransition) _outputTransition[newState][i] = _outputTransition[oldState][i];
		}
		stateEquiv[oldState] = newState;
		_usedStateIDs[newState] = true;
		_usedStateIDs[oldState] = false;
		while (_usedStateIDs[newState]) newState++;
		while (!_usedStateIDs[oldState]) oldState--;
	}

	if (_isOutputState) _outputState.resize(_numberOfStates);
	if (_isOutputTransition) _outputTransition.resize(_numberOfStates);
	_transition.resize(_numberOfStates);
	_usedStateIDs.resize(_numberOfStates);
	input_t greatestInput = 0;
	output_t greatestOutput = 0;
	for (state_t s = 0; s < _numberOfStates; s++) {
		for (input_t i = 0; i < _numberOfInputs; i++) {
			state_t& nextState = _transition[s][i];
			if ((nextState != NULL_STATE) &&
				(stateEquiv[nextState] != nextState)) {
				_transition[s][i] = stateEquiv[nextState];
			}
			if ((nextState != NULL_STATE) && (i + 1 > greatestInput)) {
				greatestInput = i + 1;
			}
			if (_isOutputTransition && (_outputTransition[s][i] != DEFAULT_OUTPUT)
				&& (_outputTransition[s][i] + 1 > greatestOutput)) {
				greatestOutput = _outputTransition[s][i] + 1;
			}
		}
		if (_isOutputState && (_outputState[s] != DEFAULT_OUTPUT) 
			&& (_outputState[s] + 1 > greatestOutput)) {
			greatestOutput = _outputState[s] + 1;
		}
	}
	if (greatestInput < _numberOfInputs) {
		_numberOfInputs = greatestInput;
		for (state_t s = 0; s < _numberOfStates; s++) {
			_transition[s].resize(_numberOfInputs);
			if (_isOutputTransition) _outputTransition[s].resize(_numberOfInputs);
		}
	}
	_numberOfOutputs = greatestOutput;
}

bool DFSM::minimize() {
	if (!removeUnreachableStates()) return false;

	queue< vector< state_t > > blocks;
	vector< state_t > block = getStates();
	blocks.push(block);

	if (_isOutputState) {
		if (!distinguishByStateOutputs(blocks)) return false;
		if (blocks.size() == _numberOfStates) {
			_isReduced = true;
			return true;
		}
	}
	if (_isOutputTransition) {
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
	for (state_t s = 0; s < _numberOfStates; s++) {
		_transition[s].resize(_numberOfInputs);
		for (input_t i = 0; i < _numberOfInputs; i++) {
			_transition[s][i] = NULL_STATE;
		}
	}
}

void DFSM::clearStateOutputs() {
	_outputState.resize(_numberOfStates);
	for (state_t s = 0; s < _numberOfStates; s++) {
		_outputState[s] = DEFAULT_OUTPUT;
	}
}

void DFSM::clearTransitionOutputs() {
	_outputTransition.resize(_numberOfStates);
	for (state_t s = 0; s < _numberOfStates; s++) {
		_outputTransition[s].resize(_numberOfInputs);
		for (input_t i = 0; i < _numberOfInputs; i++) {
			_outputTransition[s][i] = DEFAULT_OUTPUT;
		}
	}
}

void DFSM::create(state_t numberOfStates, input_t numberOfInputs, output_t numberOfOutputs) {
	if (numberOfStates == 0) {
		ERROR_MESSAGE("%s::create - the number of states needs to be greater than 0 (set to 1)", machineTypeNames[_type]);
		numberOfStates = 1;
	}
	/*
	if (numberOfInputs == 0) {
		ERROR_MESSAGE("%s::create - the number of inputs needs to be greater than 0 (set to 1)", machineTypeNames[_type]);
		numberOfInputs = 1;
	}
	if (numberOfOutputs == 0) {
		ERROR_MESSAGE("%s::create - the number of outputs needs to be greater than 0 (set to 1)", machineTypeNames[_type]);
		numberOfOutputs = 1;
	}
	*/
	output_t maxOutputs = getMaxOutputs(numberOfStates, numberOfInputs);
	if (numberOfOutputs > maxOutputs) {
		ERROR_MESSAGE("%s::create - the number of outputs reduced to the maximum of %d", machineTypeNames[_type], maxOutputs); 
		numberOfOutputs = maxOutputs;
	}
	
	_numberOfStates = numberOfStates;
	_numberOfInputs = numberOfInputs;
	_numberOfOutputs = numberOfOutputs;
	//_type is set
	_isReduced = false;
	_usedStateIDs.clear();
	_usedStateIDs.resize(_numberOfStates, true);

	clearTransitions();
	if (_isOutputState) clearStateOutputs();
	if (_isOutputTransition) clearTransitionOutputs();
}

void DFSM::generateTransitions() {
	state_t state, actState, nextState, stopState, coherentStates;
	vector<state_t> endCounter(_numberOfStates, 0);
	input_t input;
	stack<state_t> lifo;
	
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
				nextState = _transition[actState][input];// state_t -> num_states_t
				if (endCounter[nextState] == 0) lifo.push(nextState);
				if (actState != nextState) endCounter[nextState]++;
			}
		}
		if (coherentStates == _numberOfStates) break;
		input = 0;
		stopState = actState = state_t(rand() % _numberOfStates);
		while ((endCounter[actState] == 0) || (endCounter[_transition[actState][input]] <= 1)
			|| (!isReacheableWithoutEdge(this, actState, input))) {
			input++;
			if ((input == _numberOfInputs) || (endCounter[actState] == 0)) {
				actState++;
				actState %= _numberOfStates;
				if (stopState == actState) {// there are only self loops on all reachable states
					do {
						if (endCounter[actState] >= 1) {
							for (input = 0; input < _numberOfInputs; input++) {
								if (_transition[actState][input] == actState) {
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
		endCounter[_transition[actState][input]]--;
		// connect the separate state
		_transition[actState][input] = state_t(state);
		lifo.push(state);
		endCounter[state++]++;
	} while ((coherentStates + 1) != _numberOfStates);
}

void DFSM::generateStateOutputs(output_t nOutputs) {
	state_t state;
	output_t output;
	vector<output_t> outputs(nOutputs);

	// random output on each transition
	_outputState.resize(_numberOfStates);
	for (state = 0; state < _numberOfStates; state++) {
		output = output_t(rand() % nOutputs);
		_outputState[state] = output;
		outputs[output]++;		
	}
	// each output is assigned at least once
	state = 0;
	for (output = 0; output < nOutputs; output++) {
		if (outputs[output] == 0) {
			while (outputs[_outputState[state]] <= 1) {
				state++;
			}
			outputs[_outputState[state]]--;
			_outputState[state] = output;
		}
	}
}

void DFSM::generateTransitionOutputs(output_t nOutputs, output_t firstOutput) {
	state_t state;
	input_t input;
	output_t output;
	vector<output_t> outputs(nOutputs);

	// random output on each transition
	_outputTransition.resize(_numberOfStates);
	for (state = 0; state < _numberOfStates; state++) {
		_outputTransition[state].resize(_numberOfInputs);
		for (input = 0; input < _numberOfInputs; input++) {
			output = output_t(rand() % nOutputs);
			_outputTransition[state][input] = firstOutput + output;
			outputs[output]++;
		}
	}
	// each output is assigned at least once
	state = 0;
	input = 0;
	for (output = 0; output < nOutputs; output++) {
		if (outputs[output] == 0) {
			while (outputs[_outputTransition[state][input] - firstOutput] <= 1) {
				if (++input == _numberOfInputs) {
					state++;
					input = 0;
				}
			}
			outputs[_outputTransition[state][input] - firstOutput]--;
			_outputTransition[state][input] = firstOutput + output;
		}
	}
}

void DFSM::generate(state_t numberOfStates, input_t numberOfInputs, output_t numberOfOutputs) {
	if (numberOfStates == 0) {
		ERROR_MESSAGE("%s::generate - the number of states needs to be greater than 0 (set to 1)", machineTypeNames[_type]);
		numberOfStates = 1;
	}
	if (numberOfInputs == 0) {
		ERROR_MESSAGE("%s::generate - the number of inputs needs to be greater than 0 (set to 1)", machineTypeNames[_type]);
		numberOfInputs = 1;
	}
	if (numberOfOutputs == 0) {
		ERROR_MESSAGE("%s::generate - the number of outputs needs to be greater than 0 (set to 1)", machineTypeNames[_type]);
		numberOfOutputs = 1;
	}
	output_t maxOutputs = this->getMaxOutputs(numberOfStates, numberOfInputs);
	if (numberOfOutputs > maxOutputs) {
		ERROR_MESSAGE("%s::generate - the number of outputs reduced to the maximum of %d", machineTypeNames[_type], maxOutputs);
		numberOfOutputs = maxOutputs;
	}

	_numberOfStates = numberOfStates;
	_numberOfInputs = numberOfInputs;
	_numberOfOutputs = numberOfOutputs;
	//_type is set
	_isReduced = false;
	_usedStateIDs.clear();
	_usedStateIDs.resize(_numberOfStates, true);

	//srand(time(NULL));

	generateTransitions();

	output_t stateOutputs, transitionOutputs, firstTransitionOutput;
	stateOutputs = transitionOutputs = _numberOfOutputs;
	firstTransitionOutput = 0;
	if ((_numberOfOutputs > _numberOfStates) && _isOutputState && _isOutputTransition) {
		stateOutputs = _numberOfOutputs / (1 + _numberOfInputs);
		if (stateOutputs < 2) stateOutputs = 1;
		transitionOutputs = _numberOfOutputs - stateOutputs;
		if (transitionOutputs < 2) transitionOutputs = 1;
		firstTransitionOutput = _numberOfOutputs - transitionOutputs;
	}
	if (_isOutputState) generateStateOutputs(stateOutputs);
	if (_isOutputTransition) generateTransitionOutputs(transitionOutputs, firstTransitionOutput);
}

bool DFSM::loadStateOutputs(ifstream& file) {
	state_t tmpState;
	output_t output;
	_outputState.resize(_usedStateIDs.size(), DEFAULT_OUTPUT);
	for (state_t state = 0; state < _numberOfStates; state++) {
		file >> tmpState;
		if ((tmpState >= _usedStateIDs.size()) || (_usedStateIDs[tmpState])) {
			ERROR_MESSAGE("%s::loadStateOutputs - bad state output line %d", machineTypeNames[_type], state);
			return false;
		}
		_usedStateIDs[tmpState] = true;
		file >> output;
		if ((output >= _numberOfOutputs) && (output != DEFAULT_OUTPUT)) {
			ERROR_MESSAGE("%s::loadStateOutputs - bad state output line %d, output %d", machineTypeNames[_type], state, output);
			return false;
		}
		_outputState[tmpState] = output;
	}
	return true;
}

bool DFSM::loadTransitionOutputs(ifstream& file) {
	state_t tmpState;
	output_t output;
	_outputTransition.resize(_usedStateIDs.size());
	for (state_t state = 0; state < _usedStateIDs.size(); state++) {
		_outputTransition[state].resize(_numberOfInputs); 
		_usedStateIDs[state] = false;
	}
	for (state_t state = 0; state < _numberOfStates; state++) {
		file >> tmpState;
		if ((tmpState >= _usedStateIDs.size()) || (_usedStateIDs[tmpState])) {
			ERROR_MESSAGE("%s::loadTransitionOutputs - bad transition output line %d", machineTypeNames[_type], state);
			return false;
		}
		_usedStateIDs[tmpState] = true;
		for (input_t input = 0; input < _numberOfInputs; input++) {
			file >> output;
			if ((output >= _numberOfOutputs) && (output != DEFAULT_OUTPUT)) {
				ERROR_MESSAGE("%s::loadTransitionOutputs - bad transition output line %d, input %d, output %d",
					machineTypeNames[_type], state, input, output);
				return false;
			}
			_outputTransition[tmpState][input] = output;
		}
	}
	return true;
}

bool DFSM::loadTransitions(ifstream& file) {
	state_t tmpState, nextState;
	_transition.resize(_usedStateIDs.size());
	for (state_t state = 0; state < _usedStateIDs.size(); state++) {
		_transition[state].resize(_numberOfInputs);
	}
	for (state_t state = 0; state < _numberOfStates; state++) {
		file >> tmpState;
		if ((tmpState >= _usedStateIDs.size()) || (!_usedStateIDs[tmpState])) {
			ERROR_MESSAGE("%s::loadTransitions - bad transition line %d", machineTypeNames[_type], state);
			return false;
		}
		for (input_t input = 0; input < _numberOfInputs; input++) {
			file >> nextState;
			if ((nextState != NULL_STATE) && ((nextState >= _usedStateIDs.size()) || (!_usedStateIDs[nextState]))) {
				ERROR_MESSAGE("%s::loadTransitions - bad transition line %d, input %d", machineTypeNames[_type], state, input);
				return false;
			}
			_transition[tmpState][input] = nextState;
		}
	}
	return true;
}

bool DFSM::load(string fileName) {
	ifstream file(fileName.c_str());
	if (!file.is_open()) {
		ERROR_MESSAGE("%s::load - unable to open file %s", machineTypeNames[_type], fileName.c_str());
		return false;
	}
	machine_type_t type;
	bool isReduced;
	state_t numberOfStates, maxState;
	input_t numberOfInputs;
	output_t numberOfOutputs;
	file >> type >> isReduced >> numberOfStates >> numberOfInputs >> numberOfOutputs >> maxState;
	if (_type != type) {
		ERROR_MESSAGE("%s::load - bad type of FSM (%d)", machineTypeNames[_type], type);
		file.close();
		return false;
	}
	if (numberOfStates < 0) {
		ERROR_MESSAGE("%s::load - the number of states cannot be negative", machineTypeNames[_type]);
		file.close();
		return false;
	}
	if (numberOfInputs < 0) {
		ERROR_MESSAGE("%s::load - the number of inputs cannot be negative", machineTypeNames[_type]);
		file.close();
		return false;
	}
	if (numberOfOutputs < 0) {
		ERROR_MESSAGE("%s::load - the number of outputs cannot be negative", machineTypeNames[_type]);
		file.close();
		return false;
	}
	if (maxState < numberOfStates) {
		ERROR_MESSAGE("%s::load - the number of states cannot be greater than the greatest state ID", machineTypeNames[_type]);
		file.close();
		return false;
	}
	output_t maxOutputs = getMaxOutputs(numberOfStates, numberOfInputs);
	if (numberOfOutputs > maxOutputs) {
		ERROR_MESSAGE("%s::load - the number of outputs should not be greater than the maximum value of %d. Consider minimization!",
			machineTypeNames[_type], maxOutputs);
	}
	_numberOfStates = numberOfStates;
	_numberOfInputs = numberOfInputs;
	_numberOfOutputs = numberOfOutputs;
	//_type is set
	_isReduced = isReduced;
	_usedStateIDs.clear();
	_usedStateIDs.resize(maxState, false);
	if ((_isOutputState && !loadStateOutputs(file)) || (_isOutputTransition && !loadTransitionOutputs(file))
		|| !loadTransitions(file)) {
		file.close();
		return false;
	}
	file.close();
	return true;
}

string DFSM::getFilename() {
	string filename(machineTypeNames[_type]);
	filename += (_isReduced ? "_R" : "_U");
	filename += FSMlib::Utils::toString(_numberOfStates);
	return filename;
}

void DFSM::saveInfo(ofstream& file) {
	file << _type << ' ' << _isReduced << endl;
	file << _numberOfStates << ' ' << _numberOfInputs << ' ' << _numberOfOutputs << endl;
	file << _usedStateIDs.size() << endl;
}

void DFSM::saveStateOutputs(ofstream& file) {
	for (state_t state = 0; state < _usedStateIDs.size(); state++) {
		if (_usedStateIDs[state]) {
			if (_outputState[state] == DEFAULT_OUTPUT)
				file << state << ' ' << -1 << endl;
			else
				file << state << ' ' << _outputState[state] << endl;
		}
	}
}

void DFSM::saveTransitionOutputs(ofstream& file) {
	for (state_t state = 0; state < _usedStateIDs.size(); state++) {
		if (_usedStateIDs[state]) {
			file << state;
			for (input_t input = 0; input < _numberOfInputs; input++) {
				if (_outputTransition[state][input] == DEFAULT_OUTPUT) {
					file << '\t' << -1;
				}
				else {
					file << '\t' << _outputTransition[state][input];
				}
			}
			file << endl;
		}
	}
}

void DFSM::saveTransitions(ofstream& file) {
	for (state_t state = 0; state < _usedStateIDs.size(); state++) {
		if (_usedStateIDs[state]) {
			file << state;
			for (input_t input = 0; input < _numberOfInputs; input++) {
				if (_transition[state][input] == NULL_STATE) {
					file << '\t' << -1;
				}
				else {
					file << '\t' << _transition[state][input];
				}
			}
			file << endl;
		}
	}
}

string DFSM::save(string path) {
	string filename = getFilename();
	filename = FSMlib::Utils::getUniqueName(filename, "fsm", path);
	ofstream file(filename.c_str());
	if (!file.is_open()) {
		ERROR_MESSAGE("%s::save - unable to open file %s", machineTypeNames[_type], filename.c_str());
		return "";
	}
	saveInfo(file);
	if (_isOutputState) saveStateOutputs(file);
	if (_isOutputTransition) saveTransitionOutputs(file);
	saveTransitions(file);
	file.close();
	return filename;
}

void DFSM::writeDotStart(ofstream& file) {
	file << "digraph { rankdir=LR;" << endl;
}

void DFSM::writeDotStates(ofstream& file) {
	for (state_t state = 0; state < _usedStateIDs.size(); state++) {
		if (_usedStateIDs[state]) {
			file << state << " [label=\"" << state;
			if (_isOutputState) 
				if (_outputState[state] == DEFAULT_OUTPUT) file << "\n" << DEFAULT_OUTPUT_SYMBOL;
				else file << "\n" << _outputState[state];
			file << "\"];" << endl;
		}
	}
}

void DFSM::writeDotTransitions(ofstream& file) {
	for (state_t state = 0; state < _usedStateIDs.size(); state++) {
		if (_usedStateIDs[state]) {
			for (input_t input = 0; input < _numberOfInputs; input++) {
				if (_transition[state][input] != NULL_STATE) {
					file << state << " -> " << _transition[state][input]
						<< " [label=\"" << input;
					if (_isOutputTransition)
						if (_outputTransition[state][input] == DEFAULT_OUTPUT) file << " / " << DEFAULT_OUTPUT_SYMBOL;
						else file << " / " << _outputTransition[state][input];
						file << "\"];" << endl;
				}
			}
		}
	}
}

void DFSM::writeDotEnd(ofstream& file) {
	file << "}" << endl;
}

string DFSM::writeDOTfile(string path) {
	string filename("DOT");
	filename += getFilename();
	filename = FSMlib::Utils::getUniqueName(filename, "fsm", path);
	ofstream file(filename.c_str());
	if (!file.is_open()) {
		ERROR_MESSAGE("%s::writeDOTfile - unable to open file %s", machineTypeNames[_type], filename.c_str());
		return "";
	}
	writeDotStart(file);
	writeDotStates(file);
	writeDotTransitions(file);
	writeDotEnd(file);
	file.close();
	return filename;
}

state_t DFSM::addState(output_t stateOutput) {
	if ((stateOutput >= _numberOfOutputs) && (stateOutput != DEFAULT_OUTPUT)) {
		ERROR_MESSAGE("%s::addState - bad output (%d) (increase the number of outputs first)", machineTypeNames[_type], stateOutput);
		return NULL_STATE;
	}
	if (!_isOutputState && (stateOutput != DEFAULT_OUTPUT)) {
		ERROR_MESSAGE("%s::addState - set output (%d) of a state is not permitted", machineTypeNames[_type], stateOutput);
		return NULL_STATE;
	}
	_isReduced = false;
	if (_usedStateIDs.size() == _numberOfStates) {
		_usedStateIDs.push_back(true);
		if (_isOutputState) _outputState.push_back(stateOutput);
		if (_isOutputTransition) {
			vector< output_t > newStateTransitionOutputs(_numberOfInputs, DEFAULT_OUTPUT);
			_outputTransition.push_back(newStateTransitionOutputs);
		}
		vector< state_t > newStateTransitions(_numberOfInputs, NULL_STATE);
		_transition.push_back(newStateTransitions);
		_numberOfStates++;
		return _numberOfStates - 1;
	}
	state_t newState = 0;
	while (_usedStateIDs[newState]) newState++;
	_usedStateIDs[newState] = true;
	if (_isOutputState) _outputState[newState] = stateOutput;
	_numberOfStates++;
	return newState;
}

bool DFSM::setOutput(state_t state, output_t output, input_t input) {
	if ((state >= _usedStateIDs.size()) || (!_usedStateIDs[state])) {
		ERROR_MESSAGE("%s::setOutput - bad state (%d)", machineTypeNames[_type], state);
		return false;
	}
	if ((output >= _numberOfOutputs) && (output != DEFAULT_OUTPUT)) {
		ERROR_MESSAGE("%s::setOutput - bad output (%d) (increase the number of outputs first)", machineTypeNames[_type], output);
		return false;
	}
	if (input == STOUT_INPUT) {
		_outputState[state] = output;
		return true;
	}
	if (input >= _numberOfInputs) {
		ERROR_MESSAGE("%s::setOutput - bad input (%d)", machineTypeNames[_type], input);
		return false;
	}
	if (_transition[state][input] == NULL_STATE) {
		ERROR_MESSAGE("%s::setOutput - there is no such transition (%d, %d)", machineTypeNames[_type], state, input);
		return false;
	}
	_outputTransition[state][input] = output;
	_isReduced = false;
	return true; 
}

bool DFSM::setTransition(state_t from, input_t input, state_t to, output_t output) {
	if (input == STOUT_INPUT) {
		ERROR_MESSAGE("%s::setTransition - use setOutput() to set a state output instead", machineTypeNames[_type]);
		return false;
	}
	if ((from >= _usedStateIDs.size()) || (!_usedStateIDs[from])) {
		ERROR_MESSAGE("%s::setTransition - bad state From (%d)", machineTypeNames[_type], from);
		return false;
	}
	if (input >= _numberOfInputs) {
		ERROR_MESSAGE("%s::setTransition - bad input (%d)", machineTypeNames[_type], input);
		return false;
	}
	if ((to >= _usedStateIDs.size()) || (!_usedStateIDs[to])) {
		ERROR_MESSAGE("%s::setTransition - bad state To (%d)", machineTypeNames[_type], to);
		return false;
	}
	if ((output >= _numberOfOutputs) && (output != DEFAULT_OUTPUT)) {
		ERROR_MESSAGE("%s::setTransition - bad output (%d) (increase the number of outputs first)", machineTypeNames[_type], output);
		return false;
	}
	_transition[from][input] = to;
	_outputTransition[from][input] = output;
	_isReduced = false;
	return true;
}

bool DFSM::removeState(state_t state) {
	if ((state >= _usedStateIDs.size()) || (!_usedStateIDs[state])) {
		ERROR_MESSAGE("%s::removeState - bad state (%d)", machineTypeNames[_type], state);
		return false;
	}
	if (state == 0) {
		ERROR_MESSAGE("%s::removeState - the initial state cannot be removed", machineTypeNames[_type]);
		return false;
	}
	if (_isOutputState) _outputState[state] = DEFAULT_OUTPUT;
	for (state_t s = 0; s < _usedStateIDs.size(); s++) {
		if (_usedStateIDs[s]) {
			for (input_t i = 0; i < _numberOfInputs; i++) {
				if ((_transition[s][i] == state) || (s == state)) {
					_transition[s][i] = NULL_STATE;
					if (_isOutputTransition) _outputTransition[s][i] = DEFAULT_OUTPUT;
				}
			}
		}
	}
	_usedStateIDs[state] = false;
	_numberOfStates--;
	_isReduced = false;
	return true;
}

bool DFSM::removeTransition(state_t from, input_t input, state_t to, output_t output) {
	if ((from >= _usedStateIDs.size()) || (!_usedStateIDs[from])) {
		ERROR_MESSAGE("%s::removeTransition - bad state From (%d)", machineTypeNames[_type], from);
		return false;
	}
	if ((input >= _numberOfInputs) || (input == STOUT_INPUT)) {
		ERROR_MESSAGE("%s::removeTransition - bad input (%d)", machineTypeNames[_type], input);
		return false;
	}
	if (_transition[from][input] == NULL_STATE) {
		ERROR_MESSAGE("%s::removeTransition - there is no such transition (%d, %d)", machineTypeNames[_type], from, input);
		return false;
	}
	if ((to != NULL_STATE) && (_transition[from][input] != to)) {
		ERROR_MESSAGE("%s::removeTransition - state To (%d) has no effect here", machineTypeNames[_type], to);
	}
	if (output != DEFAULT_OUTPUT) {
		ERROR_MESSAGE("%s::removeTransition - the output (%d) has no effect here", machineTypeNames[_type], output);
	}
	_transition[from][input] = NULL_STATE;
	if (_isOutputTransition) _outputTransition[from][input] = DEFAULT_OUTPUT;
	_isReduced = false;
	return true;
}

void DFSM::incNumberOfInputs(input_t byNum) {
	_numberOfInputs += byNum;
	for (state_t state = 0; state < _numberOfStates; state++) {
		for (input_t i = 0; i < byNum; i++) {
			_transition[state].push_back(NULL_STATE);
			if (_isOutputTransition) _outputTransition[state].push_back(DEFAULT_OUTPUT);
		}
	}
}

void DFSM::incNumberOfOutputs(output_t byNum) {
	_numberOfOutputs += byNum;
}

output_t DFSM::getMaxOutputs(state_t numberOfStates, input_t numberOfInputs) {
	return (numberOfStates * (1 + numberOfInputs));
}

vector<state_t> DFSM::getStates() {
	vector<state_t> states;
	for (state_t state = 0; state < _usedStateIDs.size(); state++) {
		if (_usedStateIDs[state]) {
			states.push_back(state);
		}
	}
	return states;
}
