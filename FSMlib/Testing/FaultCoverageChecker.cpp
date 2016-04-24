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

#include "FaultCoverageChecker.h"

namespace FSMtesting {
	namespace FaultCoverageChecker {
		struct dynamic_bitset {
			vector<bool> data;
			size_t counter;
			size_t npos = size_t(-1);

			void resize(size_t N, bool val) {
				data.resize(N, val);
				counter = val ? N : 0;
			}

			void flip() {
				data.flip();
				counter = data.size() - counter;
			}

			void reset() {
				data.assign(data.size(), false);
				counter = 0;
			}

			void set(size_t position, bool val = true) {
				if (data[position] != val) {
					data[position] = val;
					if (val) counter++;
					else counter--;
				}
			}

			const bool operator[](size_t position) const {
				return data[position];
			}

			dynamic_bitset& operator&=(const dynamic_bitset& rhs) {
				for (size_t i = 0; i != data.size(); ++i) {
					set(i, data[i] & rhs.data[i]);
				}
				return *this;
			}

			size_t count() {
				return counter;
			}

			const bool none() {
				return (counter == 0);
			}

			size_t find_first() {
				if (counter == 0) return npos;
				for (size_t i = 0; i < data.size(); i++) {
					if (data[i]) return i;
				}
				return npos;
			}

			size_t find_next(size_t position) {
				if (counter == 0) return npos;
				for (size_t i = position + 1; i < data.size(); i++) {
					if (data[i]) return i;
				}
				return npos;
			}

			bool intersects(dynamic_bitset & other) {
				size_t N = (data.size() < other.data.size()) ? data.size() : other.data.size();
				for (size_t i = 0; i < N; i++) {
					if (data[i] && other.data[i]) return true;
				}
				return false;
			}
		};

		static input_t P = STOUT_INPUT;
		static state_t MaxStates;

		struct StateVariable {
			state_t varIdx, tmp;
			state_t instance = NULL_STATE, state;
			set<state_t> different;
			dynamic_bitset domain;
			state_t prev;
			input_t prevInput;
			bool isRefState = false;

			output_t stateOutput;
			vector<output_t> transitionOutput;
			vector<StateVariable*> next;

			StateVariable(input_t input, state_t state, output_t output) :
				prevInput(input), state(state), stateOutput(output) {
				next.resize(P);
				transitionOutput.resize(P);
				domain.resize(MaxStates, true);
			}

			StateVariable(const StateVariable& sv) :
				varIdx(sv.varIdx), instance(sv.instance), state(sv.state), prev(sv.prev),
				prevInput(sv.prevInput), isRefState(sv.isRefState), stateOutput(sv.stateOutput),
				transitionOutput(sv.transitionOutput) {
				next.resize(P);
				domain = sv.domain;
			}
		};

		struct LogItemFC {
			state_t from, to;
			input_t input;
			output_t output;
			int level;

			LogItemFC(state_t from, input_t input, state_t to, output_t output, int level) :
				from(from), input(input), output(output), to(to), level(level) {
			}
		};

		static DFSM * partial;
		static vector<StateVariable*> var;
		static vector<vector<state_t>> refStates;
		static stack<state_t> instantiated;
		static vector<DFSM*> indistinguishable;
		static stack<LogItemFC*> log;
		static int level = -1;
		
		static void cleanLevel(state_t rootIdx) {
			while (var.size() > rootIdx) {
				if (var.back() != NULL) {
					delete var.back();
				}
				var.pop_back();
			}
			refStates[level].clear();
			refStates.pop_back();
			while (!log.empty() && (log.top()->level == level)) {
				auto & li = log.top();
				if (li->to == NULL_STATE) {
					partial->removeTransition(li->from, li->input);
				} else {
					partial->setOutput(li->from, li->output, li->input);
				}
				delete li;
				log.pop();
			}
			while (!instantiated.empty()) {
				instantiated.pop();
			}
		}

		static void cleanup() {
			if (level >= 0) {
				cleanLevel(0);
				refStates.clear();
				delete partial;
				level--;
			}
			indistinguishable.clear();
		}

		static void initVar(DFSM* fsm, sequence_set_t& TS) {
			StateVariable* root, *actStateVar;
			output_t outputState;
			state_t nextState;
			auto states = fsm->getStates();
			outputState = (fsm->isOutputState()) ? fsm->getOutput(0, STOUT_INPUT) : DEFAULT_OUTPUT;
			root = new StateVariable(STOUT_INPUT, 0, outputState);
			root->prev = 0;
			
			for (sequence_in_t seq : TS) {
				actStateVar = root;
				for (input_t input : seq) {
					if (input == STOUT_INPUT) continue;
					if (actStateVar->next[input] != NULL) {
						actStateVar = actStateVar->next[input];
						continue;
					}
					nextState = fsm->getNextState(states[actStateVar->state], input);
					outputState = (fsm->isOutputState()) ? fsm->getOutput(nextState, STOUT_INPUT) : DEFAULT_OUTPUT;
					nextState = FSMsequence::getIdx(states, nextState);
					actStateVar->transitionOutput[input] = (fsm->isOutputTransition()) ?
						fsm->getOutput(states[actStateVar->state], input) : DEFAULT_OUTPUT;
					actStateVar->next[input] = new StateVariable(input, nextState, outputState);
					actStateVar = actStateVar->next[input];		
				}
			}
			// set varIdx and prev variables
			queue<StateVariable*> fifo;
			fifo.push(root);
			while (!fifo.empty()) {
				actStateVar = fifo.front();
				fifo.pop();

				actStateVar->varIdx = state_t(var.size());
				var.push_back(actStateVar);
				for (input_t input = 0; input < P; input++) {
					if (actStateVar->next[input] != NULL) {
						actStateVar->next[input]->prev = actStateVar->varIdx;
						fifo.push(actStateVar->next[input]);
					}
				}
			}
		}

		static bool areNodesDifferent(StateVariable* first, StateVariable* second, bool cyclic = false) {
			if ((cyclic) && (first->instance != NULL_STATE)) {
				return (!first->domain.intersects(second->domain));
			}
			if (first->stateOutput != second->stateOutput) return true;
			//if ((first->stateOutput != second->stateOutput) || (!first->domain.intersects(second->domain))) return true;
			for (input_t i = 0; i < P; i++) {
				if ((first->next[i] != NULL) && (second->next[i] != NULL) &&
						((first->transitionOutput[i] != second->transitionOutput[i]) ||
						areNodesDifferent(first->next[i], second->next[i], cyclic)))
					return true;
			}
			return false;
		}

		static bool isUniqueNode(StateVariable* node) {
			bool unique = true;
			for (auto idx : refStates[level]) {
				if (areNodesDifferent(node, var[idx])) {
					node->domain.set(var[idx]->instance, false);
				}
				else {
					unique = false;
				}
			}
			return unique;
		}

		static void initRefStates(StateVariable* root, sequence_vec_t hint) {
			if (hint.empty()) {
				root->domain.flip();
				root->domain.set(0, true);
				root->instance = 0;
				root->isRefState = true;
				refStates[level].push_back(root->varIdx);
			}
			else {
				for (auto seq : hint) {
					StateVariable* sv;
					auto node = root;
					for (auto i : seq) {
						if (i != STOUT_INPUT)
							node = node->next[i];
					}
					sv = node;
					if (isUniqueNode(sv)) {
						sv->domain.reset();
						sv->domain.set(refStates[level].size(), true);
						sv->instance = state_t(refStates[level].size());
						sv->isRefState = true;
						refStates[level].push_back(sv->varIdx);
					}
				}
			}
		}

		static bool instantiate(StateVariable* node) {
			state_t idx = node->varIdx;
			if (var[idx]->domain.none()) return false;
			var[idx]->instance = state_t(var[idx]->domain.find_first());
			if (var[idx]->instance >= refStates[level].size()) {
				refStates[level].push_back(idx);
				var[idx]->isRefState = true;
			}
			instantiated.push(idx);
			for (auto j : var[idx]->different) {
				if (var[j] != NULL) {
					if (var[j]->instance == NULL_STATE) {
						var[j]->domain.set(var[idx]->instance, false);
						if (var[j]->domain.count() == 1) {
							if (!instantiate(var[j])) return false;
						}
					}
					else if (var[j]->instance == var[idx]->instance) {
						return false;
					}
				}
			}
			return true;
		}

		static bool reduceDomains() {
			for (state_t i = 0; i < var.size(); i++) {
				if (var[i]->instance != NULL_STATE) continue; // a ref state
				bool unique = isUniqueNode(var[i]);
				if ((unique) || (var[i]->domain.count() == 1)) {
					if (unique) {
						// assign unused state to the var
						var[i]->domain.reset();
						var[i]->domain.set(refStates[level].size(), true);
						var[i]->instance = state_t(refStates[level].size());
						var[i]->isRefState = true;
						refStates[level].push_back(i);
					}
					else {
						var[i]->instance = state_t(var[i]->domain.find_first());
						instantiated.push(i);
					}
					for (state_t j = 0; j < i; j++) {
						if (var[j]->instance == NULL_STATE && // not a reference state
							areNodesDifferent(var[i], var[j])) {
							var[j]->domain.set(var[i]->instance, false);

							if (var[j]->domain.count() == 1) {
								if (!instantiate(var[j])) return false;
							}
						}
					}
				}
				else {
					for (state_t j = 0; j < i; j++) {
						if (!var[j]->isRefState && // not a reference state
							areNodesDifferent(var[i], var[j])) {
							if (var[j]->instance != NULL_STATE) {
								var[i]->domain.set(var[j]->instance, false);
							}
							else {
								var[i]->different.insert(j);
								var[j]->different.insert(i);
							}
						}
					}
					if (var[i]->domain.count() == 1) {
						if (!instantiate(var[i])) return false;
					}
				}
			}
			return true;
		}

		static bool mergeNodes(StateVariable* fromNode, StateVariable* toNode) {
			toNode->domain &= fromNode->domain;
			if (toNode->domain.none() || ((toNode->instance != fromNode->instance) &&
				(toNode->instance != NULL_STATE) && (fromNode->instance != NULL_STATE)))
				return false;
			if (!toNode->isRefState)
				toNode->different.insert(fromNode->different.begin(), fromNode->different.end());
			if ((toNode->instance == NULL_STATE) && (toNode->domain.count() == 1)) {
				if (!instantiate(toNode)) return false;
			}
			else if (fromNode->instance == NULL_STATE) {
				if (toNode->domain.count() == 1) {
					fromNode->domain = toNode->domain;
					if (!instantiate(fromNode)) return false;
				}
			} //else in instantiated

			for (input_t i = 0; i < P; i++) {
				if (fromNode->next[i] != NULL) {
					if (toNode->next[i] != NULL) {
						if (fromNode->next[i] == toNode->next[i]) {
							toNode->next[i]->prev = toNode->varIdx;
							continue;
						}
						if (fromNode->next[i]->isRefState) {
							fromNode->next[i]->prev = toNode->varIdx;
							if (toNode->next[i]->instance == NULL_STATE) {
								toNode->next[i]->domain = fromNode->next[i]->domain;
								if (!instantiate(toNode->next[i])) return false;
							}
							else if (toNode->next[i]->isRefState ||
								(toNode->next[i]->instance != fromNode->next[i]->instance)) {
								return false;
							} // else in instantiated
							continue;
						}
						if (!mergeNodes(fromNode->next[i], toNode->next[i])) return false;
					}
					else {
						fromNode->next[i]->prev = toNode->varIdx;
						toNode->next[i] = fromNode->next[i];
						toNode->transitionOutput[i] = fromNode->transitionOutput[i];
					}
				}
			}
			var[fromNode->varIdx] = NULL;
			delete fromNode;

			return true;
		}

		static bool processInstantiated() {
			bool consistent = true;
			while (!instantiated.empty()) {
				state_t i = instantiated.top();
				instantiated.pop();
				if (!consistent || (var[i] == NULL) || (var[i]->isRefState)) continue;
				
				StateVariable* & sv = var[refStates[level][var[i]->instance]];
				var[var[i]->prev]->next[var[i]->prevInput] = sv;
				consistent = mergeNodes(var[i], sv);
			}
			return consistent;
		}

		static state_t isSolved(state_t rootIdx) {
			state_t minDom = MaxStates;
			state_t minIdx = NULL_STATE;
			for (state_t i = rootIdx; i < var.size(); i++) {
				if (var[i] != NULL) {
					auto & node = var[i];
					if (node->domain.count() != 1) {
						for (state_t state = state_t(node->domain.find_first());
							((state != state_t(node->domain.npos)) && (state < state_t(refStates[level].size())));
							state = state_t(node->domain.find_next(state))) {
							if (areNodesDifferent(node, var[refStates[level][state]], true)) {
								node->domain.set(state, false);
								if (node->domain.count() == 1) {
									if (!instantiate(node))
										return WRONG_STATE;
								}
								else if (node->domain.none()) {
									return WRONG_STATE;
								}
							}
						}
						if (minDom > node->domain.count()) {
							minDom = state_t(node->domain.count());
							minIdx = node->varIdx;
						}
					}
				}
			}
			return minIdx;
		}

		static bool setTransitions(StateVariable* node) {
			output_t outNS, outTranNS, outputState, outputTransition;
			StateVariable* nodeNS;
			for (input_t input = 0; input < P; input++) {
				if (node->next[input] == NULL) continue;
				nodeNS = node->next[input];
				outTranNS = node->transitionOutput[input];
				outNS = nodeNS->stateOutput;
				state_t & insNS = nodeNS->instance;
				state_t nextState = partial->getNextState(node->instance, input);
				if (nextState != NULL_STATE) {
					outputState = (partial->isOutputState()) ? partial->getOutput(nextState, STOUT_INPUT) : DEFAULT_OUTPUT;
					outputTransition = (partial->isOutputTransition()) ? partial->getOutput(node->instance, input) : DEFAULT_OUTPUT;
					if (((outputState != DEFAULT_OUTPUT) && (outNS != outputState)) ||
						((outputTransition != DEFAULT_OUTPUT) && (outTranNS != outputTransition)) ||
						((insNS != NULL_STATE) && (insNS != nextState))) {
						return false;
					}
					if (insNS == NULL_STATE) {
						nodeNS->domain.reset();
						nodeNS->domain.set(nextState, true);
						instantiate(nodeNS);
						if (!setTransitions(nodeNS)) return false;
					}
					if (outputTransition != outTranNS) {// set transition output
						partial->setTransition(node->instance, input, insNS, outTranNS);
						log.push(new LogItemFC(node->instance, input, insNS, DEFAULT_OUTPUT, level));
					}
					if (outputState != outNS) {// set state output
						partial->setOutput(insNS, outNS);
						log.push(new LogItemFC(insNS, STOUT_INPUT, insNS, DEFAULT_OUTPUT, level));
					}
				}
				else if (insNS != NULL_STATE) {
					outputState = (partial->isOutputState()) ? partial->getOutput(insNS, STOUT_INPUT) : DEFAULT_OUTPUT;
					//outputTransition = (partial->isOutputTransition()) ? partial->getOutput(node->instance, input) : DEFAULT_OUTPUT;
					if ((outputState != DEFAULT_OUTPUT) && (outNS != outputState)) {
						//((outputTransition != DEFAULT_OUTPUT) && (outTranNS != outputTransition))) {
						return false;
					}
					printf("what output on: %d - %d / ? -> ? (%d)\n", node->instance, input, outTranNS);
					// set transition and possibly even output
					partial->setTransition(node->instance, input, insNS, outTranNS);
					log.push(new LogItemFC(node->instance, input, NULL_STATE, DEFAULT_OUTPUT, level));
					if (outputState != outNS) {// set state output
						partial->setOutput(insNS, outNS);
						log.push(new LogItemFC(insNS, STOUT_INPUT, insNS, DEFAULT_OUTPUT, level));
					}
				}
				else if (partial->isOutputTransition()) {
					printf("setOutput: %d - %d / %d -> ?\n", node->instance, input, outTranNS);
					/*
					outputTransition = partial->getOutput(node->instance, input);
					if (outTranNS != outputTransition) {// always: var[i].output != DEFAULT_OUTPUT
						if (outputTransition != DEFAULT_OUTPUT) {
							return false;
						}
						// set output
						partial->setOutput(node->instance, outTranNS, input);
						log.push(new LogItemFC(node->instance, input, NULL_STATE, DEFAULT_OUTPUT, level));
					}
					*/
				}
			}
			return true;
		}

		static bool isConsistent() {
			for (auto idx : refStates[level]) {
				if (!setTransitions(var[idx])) {
					return false;
				}
			}
			return true;
		}

		static void checkSolution() {
			DFSM* accurate = (DFSM*)FSMmodel::duplicateFSM(partial);
			accurate->minimize();
			for (auto f : indistinguishable) {
				if (FSMmodel::areIsomorphic(f, accurate)) {
					delete accurate;
					return;
				}
			}

			indistinguishable.push_back(accurate);
		}

		static void copyVar(state_t rootIdx, state_t newRootIdx) {
			for (state_t i = rootIdx; i < newRootIdx; i++) {
				if (var[i] != NULL) {
					var[i]->tmp = state_t(var.size());
					var.push_back(new StateVariable(*(var[i])));
				}
			}
			for (state_t i = newRootIdx; i < var.size(); i++) {
				for (auto dn : var[var[i]->varIdx]->different) {
					if (var[dn] != NULL) {
						var[i]->different.insert(var[dn]->tmp);
					}
				}
				for (input_t input = 0; input < P; input++) {
					if (var[var[i]->varIdx]->next[input] != NULL) {
						var[i]->next[input] = var[var[var[i]->varIdx]->next[input]->tmp];
					}
				}
				var[i]->varIdx = i;
				var[i]->prev = var[var[i]->prev]->tmp;
			}
			vector<state_t> rs;
			refStates.push_back(rs);
			for (auto i : refStates[level - 1]) {
				refStates[level].push_back(var[i]->tmp);
			}
		}

		static void search(state_t idx, state_t rootIdx) {
			level++;
			//printf("search: %d %d %d\n", level, rootIdx, idx);
			bool fail;
			state_t newRootIdx = state_t(var.size()), newIdx;
			for (state_t state = state_t(var[idx]->domain.find_first());
					((state != state_t(var[idx]->domain.npos)) && (state <= state_t(refStates[level - 1].size()))); // it can be new ref state
					state = state_t(var[idx]->domain.find_next(state))) {
				fail = false;
				copyVar(rootIdx, newRootIdx);
				var[var[idx]->tmp]->domain.reset();
				var[var[idx]->tmp]->domain.set(state, true);
				if (!instantiate(var[var[idx]->tmp])) {
					cleanLevel(newRootIdx);
					continue;
				}
				do {
					while (!instantiated.empty()) {
						if (!processInstantiated() || ((newIdx = isSolved(newRootIdx)) == -2)) {
							fail = true;
							break;
						}
					}
					if (fail || !isConsistent()) {
						fail = true;
						break;
					}
				} while (!instantiated.empty());
				if (!fail) {
					if (newIdx == -1) {
						checkSolution();
					}
					else {
						search(newIdx, newRootIdx);
					}
				}
				cleanLevel(newRootIdx);
			}
			level--;
		}

		void getFSMs(DFSM* fsm, sequence_in_t & CS, vector<DFSM*>& indistinguishable, int extraStates) {
			sequence_vec_t hint;
			getFSMs(fsm, CS, indistinguishable, hint, extraStates);
		}

		void getFSMs(DFSM* fsm, sequence_in_t & CS, vector<DFSM*>& indistinguishable, sequence_vec_t hint, int extraStates) {
			sequence_set_t TS;
			TS.insert(CS);
			getFSMs(fsm, TS, indistinguishable, hint, extraStates);
		}

		void getFSMs(DFSM* fsm, sequence_set_t & TS, vector<DFSM*>& indistinguishable, int extraStates) {
			sequence_vec_t hint;
			getFSMs(fsm, TS, indistinguishable, hint, extraStates);
		}

		void getFSMs(DFSM* fsm, sequence_set_t & TS, vector<DFSM*>& indistinguishableFSMs, sequence_vec_t hint, int extraStates) {
			indistinguishableFSMs.clear();
			if (TS.empty() || (TS.begin())->empty()) return;

			partial = (DFSM*)FSMmodel::createFSM(fsm->getType(), fsm->getNumberOfStates() + extraStates,
				fsm->getNumberOfInputs(), fsm->getNumberOfOutputs());
			if (partial == NULL) return;
			
			P = fsm->getNumberOfInputs();
			MaxStates = fsm->getNumberOfStates() + extraStates;

			level = 0;
			vector<state_t> rs;
			refStates.push_back(rs);
			initVar(fsm, TS);
			if (fsm->isOutputState()) {
				partial->setOutput(0, var[0]->stateOutput);
			}

			initRefStates(var[0], hint);
			state_t idx;
			if (!reduceDomains() || !processInstantiated() || ((idx = isSolved(0)) == WRONG_STATE)) {
				ERROR_MESSAGE("FCC: Unable to reconstruct a FSM!\n");
				cleanup();
				return;
			}
			while (!instantiated.empty()) {
				if (!processInstantiated() || ((idx = isSolved(0)) == WRONG_STATE)) {
					ERROR_MESSAGE("FCC: Unable to reconstruct a FSM!\n");
					cleanup();
					return;
				}
			}

			if (!isConsistent()) {
				ERROR_MESSAGE("FCC: Unable to reconstruct a FSM!\n");
				cleanup();
				return;
			}
			if (idx == NULL_STATE) {
				checkSolution();
			}
			else {
				search(idx, 0);
			}
			
			indistinguishableFSMs.assign(indistinguishable.begin(), indistinguishable.end());
			cleanup();
		}
	}
}
