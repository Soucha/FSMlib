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

			size_t size() const {
				return data.size();
			}

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
			vector<shared_ptr<StateVariable>> next;

			StateVariable(input_t input, state_t state, output_t output, input_t P, state_t MaxStates) :
				prevInput(input), state(state), stateOutput(output) {
				next.resize(P);
				transitionOutput.resize(P);
				domain.resize(MaxStates, true);
			}

			StateVariable(const StateVariable& sv) :
				varIdx(sv.varIdx), instance(sv.instance), state(sv.state), prev(sv.prev),
				prevInput(sv.prevInput), isRefState(sv.isRefState), stateOutput(sv.stateOutput),
				transitionOutput(sv.transitionOutput) {
				next.resize(sv.next.size());
				domain = sv.domain;
			}
		};

		struct LogItemFC {
			state_t from, to;
			input_t input;
			output_t output;
			
			LogItemFC(state_t from, input_t input, state_t to, output_t output) :
				from(from), input(input), output(output), to(to) {
			}
		};

		static vector<shared_ptr<StateVariable>> initVar(const unique_ptr<DFSM>& fsm, const sequence_set_t& TS, int extraStates) {
			output_t outputState = (fsm->isOutputState()) ? fsm->getOutput(0, STOUT_INPUT) : DEFAULT_OUTPUT;
			input_t P = fsm->getNumberOfInputs();
			state_t MaxStates = fsm->getNumberOfStates() + extraStates;
			auto root = make_shared<StateVariable>(STOUT_INPUT, 0, outputState, P, MaxStates);
			root->prev = 0;
			
			for (auto& seq : TS) {
				auto actStateVar = root;
				for (auto& input : seq) {
					if (input == STOUT_INPUT) continue;
					if (actStateVar->next[input]) {
						actStateVar = actStateVar->next[input];
						continue;
					}
					auto nextState = fsm->getNextState(actStateVar->state, input);
					outputState = (fsm->isOutputState()) ? fsm->getOutput(nextState, STOUT_INPUT) : DEFAULT_OUTPUT;
					actStateVar->transitionOutput[input] = (fsm->isOutputTransition()) ?
						fsm->getOutput(actStateVar->state, input) : DEFAULT_OUTPUT;
					actStateVar->next[input] = make_shared<StateVariable>(input, nextState, outputState, P, MaxStates);
					actStateVar = actStateVar->next[input];		
				}
			}
			// set varIdx and prev variables
			queue<shared_ptr<StateVariable>> fifo;
			vector<shared_ptr<StateVariable>> var;
			fifo.emplace(move(root));
			while (!fifo.empty()) {
				auto & actStateVar = fifo.front();
				actStateVar->varIdx = state_t(var.size());
				var.emplace_back(actStateVar);
				for (input_t input = 0; input < fsm->getNumberOfInputs(); input++) {
					if (actStateVar->next[input]) {
						actStateVar->next[input]->prev = actStateVar->varIdx;
						fifo.emplace(actStateVar->next[input]);
					}
				}
				fifo.pop();
			}
			return var;
		}

		static bool areNodesDifferent(const shared_ptr<StateVariable>& first, const shared_ptr<StateVariable>& second, bool cyclic = false) {
			if ((cyclic) && (first->instance != NULL_STATE)) {
				return (!first->domain.intersects(second->domain));
			}
			if (first->stateOutput != second->stateOutput) return true;
			//if ((first->stateOutput != second->stateOutput) || (!first->domain.intersects(second->domain))) return true;
			for (input_t i = 0; i < first->next.size(); i++) {
				if ((first->next[i]) && (second->next[i]) &&
						((first->transitionOutput[i] != second->transitionOutput[i]) ||
						areNodesDifferent(first->next[i], second->next[i], cyclic)))
					return true;
			}
			return false;
		}

		static bool isUniqueNode(shared_ptr<StateVariable>& node, vector<shared_ptr<StateVariable>>& var, vector<state_t>& refStates) {
			bool unique = true;
			for (auto idx : refStates) {
				if (areNodesDifferent(node, var[idx])) {
					node->domain.set(var[idx]->instance, false);
				}
				else {
					unique = false;
				}
			}
			return unique;
		}

		static vector<state_t> initRefStates(vector<shared_ptr<StateVariable>>& var, const sequence_vec_t& hint) {
			vector<state_t> refStates;
			if (hint.empty()) {
				auto & root = var[0];
				root->domain.flip();
				root->domain.set(0, true);
				root->instance = 0;
				root->isRefState = true;
				refStates.emplace_back(root->varIdx);
			}
			else {
				for (auto& seq : hint) {
					auto node = var[0];
					for (auto& i : seq) {
						if (i != STOUT_INPUT)
							node = node->next[i];
					}
					auto sv = node;
					if (isUniqueNode(sv, var, refStates)) {
						sv->domain.reset();
						sv->domain.set(refStates.size(), true);
						sv->instance = state_t(refStates.size());
						sv->isRefState = true;
						refStates.emplace_back(sv->varIdx);
					}
				}
			}
			return refStates;
		}

		static bool instantiate(shared_ptr<StateVariable>& node, vector<shared_ptr<StateVariable>>& var, 
				vector<state_t>& refStates, stack<state_t>& instantiated) {
			state_t& idx = node->varIdx;
			if (var[idx]->domain.none()) return false;
			var[idx]->instance = state_t(var[idx]->domain.find_first());
			if (var[idx]->instance >= refStates.size()) {
				refStates.emplace_back(idx);
				var[idx]->isRefState = true;
			}
			instantiated.emplace(idx);
			for (auto j : var[idx]->different) {
				if (var[j]) {
					if (var[j]->instance == NULL_STATE) {
						var[j]->domain.set(var[idx]->instance, false);
						if (var[j]->domain.count() == 1) {
							if (!instantiate(var[j], var, refStates, instantiated)) return false;
						}
					}
					else if (var[j]->instance == var[idx]->instance) {
						return false;
					}
				}
			}
			return true;
		}

		static bool reduceDomains(vector<shared_ptr<StateVariable>>& var, vector<state_t>& refStates, stack<state_t>& instantiated) {
			for (state_t i = 0; i < var.size(); i++) {
				if (var[i]->instance != NULL_STATE) continue; // a ref state
				bool unique = isUniqueNode(var[i], var, refStates);
				if ((unique) || (var[i]->domain.count() == 1)) {
					if (unique) {
						// assign unused state to the var
						var[i]->domain.reset();
						var[i]->domain.set(refStates.size(), true);
						var[i]->instance = state_t(refStates.size());
						var[i]->isRefState = true;
						refStates.emplace_back(i);
					}
					else {
						var[i]->instance = state_t(var[i]->domain.find_first());
						instantiated.emplace(i);
					}
					for (state_t j = 0; j < i; j++) {
						if ((var[j]->instance == NULL_STATE) && // not a reference state
								areNodesDifferent(var[i], var[j])) {
							var[j]->domain.set(var[i]->instance, false);

							if (var[j]->domain.count() == 1) {
								if (!instantiate(var[j], var, refStates, instantiated)) return false;
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
						if (!instantiate(var[i], var, refStates, instantiated)) return false;
					}
				}
			}
			return true;
		}

		static bool mergeNodes(shared_ptr<StateVariable>& fromNode, shared_ptr<StateVariable>& toNode,
				vector<shared_ptr<StateVariable>>& var, vector<state_t>& refStates, stack<state_t>& instantiated) {
			toNode->domain &= fromNode->domain;
			if (toNode->domain.none() || ((toNode->instance != fromNode->instance) &&
				(toNode->instance != NULL_STATE) && (fromNode->instance != NULL_STATE)))
				return false;
			if (!toNode->isRefState)
				toNode->different.insert(fromNode->different.begin(), fromNode->different.end());
			if ((toNode->instance == NULL_STATE) && (toNode->domain.count() == 1)) {
				if (!instantiate(toNode, var, refStates, instantiated)) return false;
			}
			else if (fromNode->instance == NULL_STATE) {
				if (toNode->domain.count() == 1) {
					fromNode->domain = toNode->domain;
					if (!instantiate(fromNode, var, refStates, instantiated)) return false;
				}
			} //else in instantiated

			for (input_t i = 0; i < fromNode->next.size(); i++) {
				if (fromNode->next[i]) {
					if (toNode->next[i]) {
						if (fromNode->next[i] == toNode->next[i]) {
							toNode->next[i]->prev = toNode->varIdx;
							continue;
						}
						if (fromNode->next[i]->isRefState) {
							fromNode->next[i]->prev = toNode->varIdx;
							if (toNode->next[i]->instance == NULL_STATE) {
								toNode->next[i]->domain = fromNode->next[i]->domain;
								if (!instantiate(toNode->next[i], var, refStates, instantiated)) return false;
							}
							else if (toNode->next[i]->isRefState ||
								(toNode->next[i]->instance != fromNode->next[i]->instance)) {
								return false;
							} // else in instantiated
							continue;
						}
						if (!mergeNodes(fromNode->next[i], toNode->next[i], var, refStates, instantiated)) return false;
					}
					else {
						fromNode->next[i]->prev = toNode->varIdx;
						toNode->next[i].swap(fromNode->next[i]);
						toNode->transitionOutput[i] = fromNode->transitionOutput[i];
					}
				}
			}
			var[fromNode->varIdx].reset();
			fromNode.reset();
			return true;
		}

		static bool processInstantiated(vector<shared_ptr<StateVariable>>& var, vector<state_t>& refStates, stack<state_t>& instantiated) {
			bool consistent = true;
			while (!instantiated.empty()) {
				auto i = instantiated.top();
				instantiated.pop();
				if (!consistent || (!var[i]) || (var[i]->isRefState)) continue;
				
				auto & sv = var[refStates[var[i]->instance]];
				var[var[i]->prev]->next[var[i]->prevInput] = sv;
				consistent = mergeNodes(var[i], sv, var, refStates, instantiated);
			}
			return consistent;
		}

		static state_t isSolved(state_t rootIdx, vector<shared_ptr<StateVariable>>& var,
				vector<state_t>& refStates, stack<state_t>& instantiated) {
			state_t minDom = state_t(var[refStates[0]]->domain.size() + 1);
			state_t minIdx = NULL_STATE;
			for (state_t i = rootIdx; i < var.size(); i++) {
				if (var[i]) {
					auto & node = var[i];
					if (node->domain.count() != 1) {
						for (state_t state = state_t(node->domain.find_first());
							((state != state_t(node->domain.npos)) && (state < state_t(refStates.size())));
							state = state_t(node->domain.find_next(state))) {
							if (areNodesDifferent(node, var[refStates[state]], true)) {
								node->domain.set(state, false);
								if (node->domain.count() == 1) {
									if (!instantiate(node, var, refStates, instantiated))
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

		static bool setTransitions(shared_ptr<StateVariable>& node, const unique_ptr<DFSM>& partial, stack<unique_ptr<LogItemFC>>& log, 
				vector<shared_ptr<StateVariable>>& var, vector<state_t>& refStates, stack<state_t>& instantiated) {
			for (input_t input = 0; input < node->next.size(); input++) {
				if (!node->next[input]) continue;
				auto & nodeNS = node->next[input];
				auto & outTranNS = node->transitionOutput[input];
				auto outNS = nodeNS->stateOutput;
				state_t & insNS = nodeNS->instance;
				state_t nextState = partial->getNextState(node->instance, input);
				if (nextState != NULL_STATE) {
					auto outputState = (partial->isOutputState()) ? partial->getOutput(nextState, STOUT_INPUT) : DEFAULT_OUTPUT;
					auto outputTransition = (partial->isOutputTransition()) ? partial->getOutput(node->instance, input) : DEFAULT_OUTPUT;
					if (((outputState != DEFAULT_OUTPUT) && (outNS != outputState)) ||
						((outputTransition != DEFAULT_OUTPUT) && (outTranNS != outputTransition)) ||
						((insNS != NULL_STATE) && (insNS != nextState))) {
						return false;
					}
					if (insNS == NULL_STATE) {
						nodeNS->domain.reset();
						nodeNS->domain.set(nextState, true);
						instantiate(nodeNS, var, refStates, instantiated);
						if (!setTransitions(nodeNS, partial, log, var, refStates, instantiated)) return false;
					}
					if (outputTransition != outTranNS) {// set transition output
						partial->setTransition(node->instance, input, insNS, outTranNS);
						log.emplace(make_unique<LogItemFC>(node->instance, input, insNS, DEFAULT_OUTPUT));
					}
					if (outputState != outNS) {// set state output
						partial->setOutput(insNS, outNS);
						log.emplace(make_unique<LogItemFC>(insNS, STOUT_INPUT, insNS, DEFAULT_OUTPUT));
					}
				}
				else if (insNS != NULL_STATE) {
					auto outputState = (partial->isOutputState()) ? partial->getOutput(insNS, STOUT_INPUT) : DEFAULT_OUTPUT;
					//outputTransition = (partial->isOutputTransition()) ? partial->getOutput(node->instance, input) : DEFAULT_OUTPUT;
					if ((outputState != DEFAULT_OUTPUT) && (outNS != outputState)) {
						//((outputTransition != DEFAULT_OUTPUT) && (outTranNS != outputTransition))) {
						return false;
					}
					printf("what output on: %d - %d / ? -> ? (%d)\n", node->instance, input, outTranNS);
					// set transition and possibly even output
					partial->setTransition(node->instance, input, insNS, outTranNS);
					log.emplace(make_unique<LogItemFC>(node->instance, input, NULL_STATE, DEFAULT_OUTPUT));
					if (outputState != outNS) {// set state output
						partial->setOutput(insNS, outNS);
						log.emplace(make_unique<LogItemFC>(insNS, STOUT_INPUT, insNS, DEFAULT_OUTPUT));
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
						log.emplace(make_unique<LogItemFC>(node->instance, input, NULL_STATE, DEFAULT_OUTPUT, level));
					}
					*/
				}
			}
			return true;
		}

		static bool isConsistent(const unique_ptr<DFSM>& partial, stack<unique_ptr<LogItemFC>>& log,
				vector<shared_ptr<StateVariable>>& var, vector<state_t>& refStates, stack<state_t>& instantiated) {
			for (auto idx : refStates) {
				if (!setTransitions(var[idx], partial, log, var, refStates, instantiated)) {
					return false;
				}
			}
			return true;
		}

		static void checkSolution(vector<unique_ptr<DFSM>>& indistinguishable, const unique_ptr<DFSM>& partial) {
			auto accurate = FSMmodel::duplicateFSM(partial);
			accurate->minimize();
			for (auto &f : indistinguishable) {
				if (FSMmodel::areIsomorphic(f, accurate)) {
					return;
				}
			}
			indistinguishable.emplace_back(move(accurate));
		}

		static void copyVar(state_t rootIdx, state_t newRootIdx, vector<shared_ptr<StateVariable>>& var) {
			for (state_t i = rootIdx; i < newRootIdx; i++) {
				if (var[i]) {
					var[i]->tmp = state_t(var.size());
					var.emplace_back(make_shared<StateVariable>(*(var[i])));
				}
			}
			for (state_t i = newRootIdx; i < var.size(); i++) {
				for (auto dn : var[var[i]->varIdx]->different) {
					if (var[dn]) {
						var[i]->different.insert(var[dn]->tmp);
					}
				}
				for (input_t input = 0; input < var[newRootIdx]->next.size(); input++) {
					if (var[var[i]->varIdx]->next[input]) {
						var[i]->next[input] = var[var[var[i]->varIdx]->next[input]->tmp];
					}
				}
				var[i]->varIdx = i;
				var[i]->prev = var[var[i]->prev]->tmp;
			}
		}

		static void cleanLevel(const unique_ptr<DFSM>& partial, stack<unique_ptr<LogItemFC>>& log) {
			while (!log.empty()) {
				auto & li = log.top();
				if (li->to == NULL_STATE) {
					partial->removeTransition(li->from, li->input);
				} else {
					partial->setOutput(li->from, li->output, li->input);
				}
				log.pop();
			}
		}

		static void cleanLevel(state_t rootIdx, vector<shared_ptr<StateVariable>>& var, stack<state_t>& instantiated) {
			var.resize(rootIdx);
			while (!instantiated.empty()) {
				instantiated.pop();
			}
		}

		static void search(state_t idx, state_t rootIdx, vector<unique_ptr<DFSM>>& indistinguishable, const unique_ptr<DFSM>& partial,
				vector<shared_ptr<StateVariable>>& var, vector<state_t>& refStates, stack<state_t>& instantiated) {
			//printf("search: %d %d %d\n", level, rootIdx, idx);
			bool fail;
			state_t newRootIdx = state_t(var.size()), newIdx;
			for (state_t state = state_t(var[idx]->domain.find_first());
					((state != state_t(var[idx]->domain.npos)) && (state <= state_t(refStates.size()))); // it can be new ref state
					state = state_t(var[idx]->domain.find_next(state))) {
				fail = false;
				copyVar(rootIdx, newRootIdx, var);
				vector<state_t> newRefStates;
				for (auto i : refStates) {
					newRefStates.emplace_back(var[i]->tmp);
				}
				var[var[idx]->tmp]->domain.reset();
				var[var[idx]->tmp]->domain.set(state, true);
				if (!instantiate(var[var[idx]->tmp], var, newRefStates, instantiated)) {
					cleanLevel(newRootIdx, var, instantiated);
					continue;
				}
				stack<unique_ptr<LogItemFC>> log;
				do {
					while (!instantiated.empty()) {
						if (!processInstantiated(var, newRefStates, instantiated) ||
							((newIdx = isSolved(newRootIdx, var, newRefStates, instantiated)) == WRONG_STATE)) {
							fail = true;
							break;
						}
					}
					if (fail || !isConsistent(partial, log, var, newRefStates, instantiated)) {
						fail = true;
						break;
					}
				} while (!instantiated.empty());
				if (!fail) {
					if (newIdx == NULL_STATE) {
						checkSolution(indistinguishable, partial);
					}
					else {
						search(newIdx, newRootIdx, indistinguishable, partial, var, newRefStates, instantiated);
					}
				}
				cleanLevel(newRootIdx, var, instantiated);
				cleanLevel(partial, log);
			}
		}
		
		vector<unique_ptr<DFSM>> getFSMs(const unique_ptr<DFSM>& fsm, const sequence_in_t & CS, int extraStates) {
			sequence_vec_t hint;
			return getFSMs(fsm, CS, hint, extraStates);
		}

		vector<unique_ptr<DFSM>> getFSMs(const unique_ptr<DFSM>& fsm, const sequence_in_t & CS, const sequence_vec_t& hint, int extraStates) {
			sequence_set_t TS;
			TS.insert(CS);
			return getFSMs(fsm, TS, hint, extraStates);
		}

		vector<unique_ptr<DFSM>> getFSMs(const unique_ptr<DFSM>& fsm, const sequence_set_t & TS, int extraStates) {
			sequence_vec_t hint;
			return getFSMs(fsm, TS, hint, extraStates);
		}
		
		vector<unique_ptr<DFSM>> getFSMs(const unique_ptr<DFSM>& fsm, const sequence_set_t & TS, const sequence_vec_t& hint, int extraStates) {
			RETURN_IF_UNREDUCED(fsm, "FaultCoverageChecker::getFSMs", vector<unique_ptr<DFSM>>());
			if (TS.empty() || (TS.begin())->empty()) return vector<unique_ptr<DFSM>>();

			auto partial = FSMmodel::createFSM(fsm->getType(), fsm->getNumberOfStates() + extraStates,
				fsm->getNumberOfInputs(), fsm->getNumberOfOutputs());
			if (!partial) return vector<unique_ptr<DFSM>>();

			auto var = initVar(fsm, TS, extraStates);
			if (fsm->isOutputState()) {
				partial->setOutput(0, var[0]->stateOutput);
			}

			auto refStates = initRefStates(var, hint);
			state_t idx;
			stack<state_t> instantiated;
			if (!reduceDomains(var, refStates, instantiated) || 
				!processInstantiated(var, refStates, instantiated) || 
				((idx = isSolved(0, var, refStates, instantiated)) == WRONG_STATE)) {
				ERROR_MESSAGE("FCC: Unable to reconstruct a FSM!\n");
				return vector<unique_ptr<DFSM>>();
			}
			while (!instantiated.empty()) {
				if (!processInstantiated(var, refStates, instantiated) ||
					((idx = isSolved(0, var, refStates, instantiated)) == WRONG_STATE)) {
					ERROR_MESSAGE("FCC: Unable to reconstruct a FSM!\n");
					return vector<unique_ptr<DFSM>>();
				}
			}
			stack<unique_ptr<LogItemFC>> log;
			if (!isConsistent(partial, log, var, refStates, instantiated)) {
				ERROR_MESSAGE("FCC: Unable to reconstruct a FSM!\n");
				return vector<unique_ptr<DFSM>>();
			}
			vector<unique_ptr<DFSM>> indistinguishable;
			if (idx == NULL_STATE) {
				checkSolution(indistinguishable, partial);
			}
			else {
				search(idx, 0, indistinguishable, partial, var, refStates, instantiated);
			}
			return indistinguishable;
		}
	}
}
