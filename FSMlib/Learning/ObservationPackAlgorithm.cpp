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

#include "FSMlearning.h"

namespace FSMlearning {
	struct dt_node_t {
		sequence_in_t sequence;// access or distinguishing if state == NULL_STATE
		state_t state;
		map<sequence_out_t, shared_ptr<dt_node_t>> succ;
		weak_ptr<dt_node_t> parent;
		size_t level;
	};

	struct componentOT {
		sequence_in_t S;
		vector<sequence_in_t> E;
		map<sequence_in_t, vector<sequence_out_t>> T;
	};

	static void checkNumberOfOutputs(const unique_ptr<Teacher>& teacher, const unique_ptr<DFSM>& conjecture) {
		if (conjecture->getNumberOfOutputs() != teacher->getNumberOfOutputs()) {
			conjecture->incNumberOfOutputs(teacher->getNumberOfOutputs() - conjecture->getNumberOfOutputs());
		}
	}

	static shared_ptr<dt_node_t> createNode(const shared_ptr<dt_node_t>& parent, sequence_in_t s, sequence_out_t output) {
		auto leaf = make_shared<dt_node_t>();
		//leaf->level = parent->level + 1;
		leaf->parent = parent;
		leaf->sequence = move(s);
		leaf->state = NULL_STATE;
		parent->succ.emplace(move(output), leaf);
		return leaf;
	}

	static void addState(shared_ptr<dt_node_t>& node, vector<shared_ptr<dt_node_t>>& stateNodes,
		const unique_ptr<DFSM>& conjecture, const unique_ptr<Teacher>& teacher) {
		node->state = conjecture->addState();
		stateNodes.emplace_back(node);
		if (conjecture->isOutputState()) {
			teacher->resetAndOutputQuery(node->sequence);
			auto output = teacher->outputQuery(STOUT_INPUT);
			checkNumberOfOutputs(teacher, conjecture);
			conjecture->setOutput(node->state, output);
		}
	}

	static shared_ptr<dt_node_t> sift(const shared_ptr<dt_node_t>& dt, const sequence_in_t& s, const unique_ptr<Teacher>& teacher) {
		auto currentNode = dt;
		while (currentNode->state == NULL_STATE) {
			teacher->resetAndOutputQuery(s);
			auto output = teacher->outputQuery(currentNode->sequence);
			auto it = currentNode->succ.find(output);
			if (it == currentNode->succ.end()) {
				return createNode(currentNode, s, move(output));
			}
			currentNode = it->second;
		}
		return currentNode;
	}

	static void addNewTransitions(state_t startState, shared_ptr<dt_node_t>& dt, vector<shared_ptr<dt_node_t>>& stateNodes,
		const unique_ptr<DFSM>& conjecture, const unique_ptr<Teacher>& teacher) {
		for (state_t state = startState; state < stateNodes.size(); state++) {
			for (input_t i = 0; i < conjecture->getNumberOfInputs(); i++) {
				sequence_in_t prefix(stateNodes[state]->sequence);
				prefix.emplace_back(i);
				auto dtNode = sift(dt, prefix, teacher);
				if (dtNode->state == NULL_STATE) {// new state
					addState(dtNode, stateNodes, conjecture, teacher);
				}
				conjecture->setTransition(state, i, dtNode->state);
				if (conjecture->isOutputTransition()) {
					auto output = teacher->resetAndOutputQuery(prefix);
					checkNumberOfOutputs(teacher, conjecture);
					conjecture->setOutput(state, output.back(), i);
				}
			}
		}
	}

	static void extendInputs(input_t byNumInputs, shared_ptr<dt_node_t>& dt, vector<shared_ptr<dt_node_t>>& stateNodes,
		const unique_ptr<DFSM>& conjecture, const unique_ptr<Teacher>& teacher) {
		bool newState = false;
		auto numStates = state_t(stateNodes.size());
		for (state_t state = 0; state < numStates; state++) {
			for (input_t i = conjecture->getNumberOfInputs(); i < conjecture->getNumberOfInputs() + byNumInputs; i++) {
				sequence_in_t prefix(stateNodes[state]->sequence);
				prefix.emplace_back(i);
				auto dtNode = sift(dt, prefix, teacher);
				if (dtNode->state == NULL_STATE) {// new state
					newState = true;
					addState(dtNode, stateNodes, conjecture, teacher);
				}
				conjecture->setTransition(state, i, dtNode->state);
				if (conjecture->isOutputTransition()) {
					auto output = teacher->resetAndOutputQuery(prefix);
					conjecture->setOutput(state, output.back(), i);
				}
			}
		}
		if (newState) {
			addNewTransitions(numStates, dt, stateNodes, conjecture, teacher);
		}
	}

	static void updateConjecture(state_t splittedState, shared_ptr<dt_node_t>& dt, vector<shared_ptr<dt_node_t>>& stateNodes,
		const unique_ptr<DFSM>& conjecture, const unique_ptr<Teacher>& teacher) {
		auto newState = stateNodes.back()->state;
		auto distNode = stateNodes[splittedState]->parent.lock();
		// fix all transitions that lead to the new state instead of to the splitted state
		for (state_t state = 0; state < newState; state++) {
			for (input_t i = 0; i < conjecture->getNumberOfInputs(); i++) {
				if (conjecture->getNextState(state, i) == splittedState) {
					sequence_in_t prefix(stateNodes[state]->sequence);
					prefix.emplace_back(i);
					//auto dtNode = sift(dt, prefix, teacher);// similar to what follows but with more effort
					teacher->resetAndOutputQuery(prefix);
					auto output = teacher->outputQuery(distNode->sequence);
					auto it = distNode->succ.find(output);
					if (it == distNode->succ.end()) {// new state
						auto leaf = createNode(distNode, move(prefix), move(output));
						addState(leaf, stateNodes, conjecture, teacher);
						conjecture->setTransition(state, i, leaf->state,
							conjecture->isOutputTransition() ? conjecture->getOutput(state, i) : DEFAULT_OUTPUT);
					}
					else if (it->second->state != splittedState) {// == newState
						conjecture->setTransition(state, i, it->second->state,
							conjecture->isOutputTransition() ? conjecture->getOutput(state, i) : DEFAULT_OUTPUT);
					}
				}
			}
		}
		//addNewTransitions(newState, dt, stateNodes, conjecture, teacher);
	}

	static void updateTree(const sequence_in_t& ce, shared_ptr<dt_node_t>& dt, vector<shared_ptr<dt_node_t>>& stateNodes,
		const unique_ptr<DFSM>& conjecture, const unique_ptr<Teacher>& teacher) {
		state_t currState = 0;
		sequence_in_t prefix;
		for (const auto& input : ce) {
			if (input == STOUT_INPUT) continue;
			auto nextState = conjecture->getNextState(currState, input);
			prefix.emplace_back(input);
			if (conjecture->isOutputTransition()) {
				// nextState== NULL_STATE ?
				auto output = teacher->resetAndOutputQuery(prefix);
				if (output.back() != conjecture->getOutput(currState, input)) {
					auto refNode = stateNodes[currState];
					auto leaf = createNode(refNode, move(refNode->sequence), sequence_out_t({ conjecture->getOutput(currState, input) }));
					leaf->state = currState;
					stateNodes[currState] = leaf;

					prefix.pop_back();
					leaf = createNode(refNode, move(prefix), sequence_out_t({ output.back() }));
					addState(leaf, stateNodes, conjecture, teacher);

					refNode->sequence.clear();
					refNode->sequence.emplace_back(input);
					refNode->state = NULL_STATE;

					updateConjecture(currState, dt, stateNodes, conjecture, teacher);
					break;
				}
			}
			auto dtNode = sift(dt, prefix, teacher);
			if (dtNode->state == NULL_STATE) {// new leaf
				addState(dtNode, stateNodes, conjecture, teacher);
				addNewTransitions(dtNode->state, dt, stateNodes, conjecture, teacher);
				break;
			}
			else if (nextState != dtNode->state) {
				// find distinguishing sequence
				auto n1 = stateNodes[nextState];
				auto n2 = stateNodes[dtNode->state];
				while (n1->level > n2->level) n1 = n1->parent.lock();
				while (n1->level < n2->level) n2 = n2->parent.lock();
				while (n1 != n2) {
					n1 = n1->parent.lock();
					n2 = n2->parent.lock();
				}
				auto distSequence = n1->sequence;
				distSequence.push_front(input);

				auto refNode = stateNodes[currState];
				teacher->resetAndOutputQuery(refNode->sequence);// access sequence
				auto leaf = createNode(refNode, move(refNode->sequence), teacher->outputQuery(distSequence));
				leaf->state = currState;
				stateNodes[currState] = leaf;

				prefix.pop_back();
				teacher->resetAndOutputQuery(prefix);
				leaf = createNode(refNode, move(prefix), teacher->outputQuery(distSequence));
				addState(leaf, stateNodes, conjecture, teacher);

				refNode->sequence = move(distSequence);
				refNode->state = NULL_STATE;

				updateConjecture(currState, dt, stateNodes, conjecture, teacher);
				break;
			}
			currState = nextState;
		}
	}

	unique_ptr<DFSM> ObservationPackAlgorithm(const unique_ptr<Teacher>& teacher,
		function<bool(const unique_ptr<DFSM>& conjecture)> provideTentativeModel) {
		if (!teacher->isBlackBoxResettable()) {
			ERROR_MESSAGE("FSMlearning::ObservationPackAlgorithm - the Black Box needs to be resettable");
			return nullptr;
		}
		/// counterexample
		sequence_in_t ce;
		/// Discrimination Tree root
		auto dt = make_shared<dt_node_t>();
		dt->level = 0;
		vector<shared_ptr<dt_node_t>> stateNodes;
		vector<componentOT> components, newComponents;
		// numberOfOutputs can produce error message
		auto conjecture = FSMmodel::createFSM(teacher->getBlackBoxModelType(), 1, teacher->getNumberOfInputs(), teacher->getNumberOfOutputs());
		
		componentOT ot;
		//ot.S = sequence_in_t();
		vector<sequence_out_t> row;
		// add all transitions
		for (input_t i = 0; i < conjecture->getNumberOfInputs(); i++) {
			row.emplace_back(sequence_out_t({ teacher->resetAndOutputQuery(i) }));
			ot.E.emplace_back(sequence_in_t({ i }));
			conjecture->setTransition(0, i, 0, DEFAULT_OUTPUT);
			if (conjecture->isOutputTransition()) {
				checkNumberOfOutputs(teacher, conjecture);
				conjecture->setOutput(0, row.back().back(), i);
			}
		}
		
		if (conjecture->isOutputState()) {
			dt->sequence.emplace_back(STOUT_INPUT);
			auto output = teacher->resetAndOutputQuery(STOUT_INPUT);
			checkNumberOfOutputs(teacher, conjecture);
			conjecture->setOutput(0, output);
			row.emplace_back(sequence_out_t({ output }));
			ot.E.emplace_back(sequence_in_t({ STOUT_INPUT }));
			auto leaf = createNode(dt, sequence_in_t(), sequence_out_t({ output }));// the initial state -> empty access sequence
			leaf->state = 0;
			stateNodes.emplace_back(leaf);
			dt->state = NULL_STATE;
		}
		else {
			stateNodes.emplace_back(dt);
			dt->state = 0;
		}
		ot.T.emplace(sequence_in_t(), move(row));
		newComponents.emplace_back(move(ot));
		bool unlearned = true;
		if (provideTentativeModel) {
			unlearned = provideTentativeModel(conjecture);
		}
		while (unlearned) {
			// complete components
			for (state_t state = 0; state < components.size(); state++) {
				auto& ot = components[state];
				if (ot.T.begin()->second.size() != ot.E.size()) {// needs to be filled
					// reference row
					auto& refRow = ot.T[ot.S];
					for (auto e = refRow.size(); e < ot.E.size(); e++) {
						teacher->resetAndOutputQuery(ot.S);
						refRow.emplace_back(teacher->outputQuery(ot.E[e]));
					}
					auto refNode = stateNodes[state];
					for (auto itRow = ot.T.begin(); itRow != ot.T.end(); itRow++) {
						if (itRow->first == ot.S) continue;
						bool undististinguished = true;
						size_t sepSeqIdx;
						auto& row = itRow->second;
						for (auto e = row.size(); e < ot.E.size(); e++) {
							teacher->resetAndOutputQuery(itRow->first);
							row.emplace_back(teacher->outputQuery(ot.E[e]));
							if (undististinguished && (row.back() != refRow[e])) {
								sepSeqIdx = e;
								undististinguished = false;
							}
						}
						if (!undististinguished) {// new state
							sequence_in_t accessSeq(itRow->first);
							auto currentNode = refNode;
							while (currentNode->state == NULL_STATE) {
								auto it = currentNode->succ.find(row[currentNode->level]);
								if (it == currentNode->succ.end()) {
									currentNode = createNode(currentNode, accessSeq, row[currentNode->level]);
									break;
								}
								currentNode = it->second;
							}
							if (currentNode->state == NULL_STATE) {// new state
								addState(currentNode, stateNodes, conjecture, teacher);
								currentNode->level = sepSeqIdx;
								componentOT newOT;
								newOT.S = accessSeq;
								newOT.E = ot.E;
								newOT.T.emplace(accessSeq, move(row));
								newComponents.emplace_back(move(newOT));
							}
							else {
								auto& otherOT = (currentNode->state < components.size()) ? 
									components[currentNode->state] : newComponents[currentNode->state - components.size()];
								auto& refOtherRow = otherOT.T[otherOT.S];
								auto sepIdx = otherOT.E.size();
								if (currentNode->state == state) {
									sepIdx = sepSeqIdx;
								} else {
									for (auto e = min(currentNode->level, sepSeqIdx); e < otherOT.E.size(); e++) {
										if (row[e] != refOtherRow[e]) {
											sepIdx = e;
											break;
										}
									}
								}
								if (sepIdx == otherOT.E.size()) {// not distinguished
									otherOT.T.emplace(accessSeq, row);
								}
								else {
									// update DT
									auto leaf = createNode(currentNode, move(currentNode->sequence), refOtherRow[sepIdx]);
									leaf->state = currentNode->state;
									leaf->level = currentNode->level;
									stateNodes[currentNode->state] = leaf;
									currentNode->sequence = otherOT.E[sepIdx];
									currentNode->state = NULL_STATE;
									currentNode->level = sepIdx;

									leaf = createNode(currentNode, accessSeq, row[sepIdx]);
									addState(leaf, stateNodes, conjecture, teacher);
									leaf->level = sepSeqIdx;

									componentOT newOT;
									newOT.S = accessSeq;
									newOT.E = otherOT.E;
									newOT.T.emplace(accessSeq, move(row));
									newComponents.emplace_back(move(newOT));

									currentNode = leaf;
								}
							}
							// updateConjecture
							input_t input = accessSeq.back();
							accessSeq.pop_back();
							for (auto sn : stateNodes) {
								if (sn->sequence == accessSeq) {
									auto output = conjecture->isOutputTransition() ? conjecture->getOutput(sn->state, input) : DEFAULT_OUTPUT;
									conjecture->setTransition(sn->state, input, currentNode->state, output);
									break;
								}
							}
							// erase row from ot
							itRow = ot.T.erase(itRow);
							itRow--;
						}
					}
				}
			}
			if (provideTentativeModel) {
				unlearned = provideTentativeModel(conjecture);
				if (!unlearned) continue;
			}
			// process new components
			state_t numberOfOldComponents = state_t(components.size());
			for (state_t state = 0; state < newComponents.size(); state++) {
				components.emplace_back(move(newComponents[state]));
			}
			newComponents.clear();
			for (state_t state = numberOfOldComponents; state < components.size(); state++) {
				for (input_t input = 0; input < conjecture->getNumberOfInputs(); input++) {
					sequence_in_t prefix(stateNodes[state]->sequence);
					prefix.emplace_back(input);
					auto dtNode = sift(dt, prefix, teacher);// what state is the next state?
					if (dtNode->state == NULL_STATE) {// new state
						addState(dtNode, stateNodes, conjecture, teacher);
						
						componentOT ot;
						ot.S = prefix;
						vector<sequence_out_t> row;
						for (input_t i = 0; i < conjecture->getNumberOfInputs(); i++) {
							teacher->resetAndOutputQuery(prefix);
							row.emplace_back(sequence_out_t({ teacher->outputQuery(i) }));
							ot.E.emplace_back(sequence_in_t({ i }));
						}
						if (conjecture->isOutputState()) {
							row.emplace_back(sequence_out_t({ conjecture->getOutput(dtNode->state, STOUT_INPUT) }));
							ot.E.emplace_back(sequence_in_t({ STOUT_INPUT }));
						}
						ot.T.emplace(move(prefix), move(row));
						components.emplace_back(move(ot));
					}
					else {
						auto& ot = components[dtNode->state];
						if (!ot.T.count(prefix)) {
							vector<sequence_out_t> row;
							bool undististinguished = true;
							size_t sepSeqIdx;
							auto& refRow = ot.T[ot.S];
							for (size_t e = 0; e < ot.E.size(); e++) {
								teacher->resetAndOutputQuery(prefix);
								row.emplace_back(teacher->outputQuery(ot.E[e]));
								if (undististinguished && (row.back() != refRow[e])) {
									sepSeqIdx = e;
									undististinguished = false;
								}
							}
							if (undististinguished) {
								ot.T.emplace(move(prefix), move(row));
							}
							else {// new component
								auto leaf = createNode(dtNode, move(dtNode->sequence), refRow[sepSeqIdx]);
								leaf->state = dtNode->state;
								stateNodes[dtNode->state] = leaf;
								dtNode->sequence = ot.E[sepSeqIdx];
								dtNode->state = NULL_STATE;

								leaf = createNode(dtNode, prefix, row[sepSeqIdx]);
								addState(leaf, stateNodes, conjecture, teacher);

								componentOT newOT;
								newOT.S = prefix;
								newOT.E = ot.E;
								newOT.T.emplace(move(prefix), move(row));
								components.emplace_back(move(newOT));

								dtNode = leaf;
							}
						}
					}
					conjecture->setTransition(state, input, dtNode->state);
					if (conjecture->isOutputTransition()) {
						auto& output = components[state].T[components[state].S][input];
						checkNumberOfOutputs(teacher, conjecture);
						conjecture->setOutput(state, output.back(), input);
					}
					if (provideTentativeModel) {
						unlearned = provideTentativeModel(conjecture);
					}
				}
			}
			ce = teacher->equivalenceQuery(conjecture);
			if (ce.empty()) unlearned = false;
			else {
				if (conjecture->getNumberOfInputs() != teacher->getNumberOfInputs()) {
					auto deltaInputs = teacher->getNumberOfInputs() - conjecture->getNumberOfInputs();
					//extendInputs(deltaInputs, dt, stateNodes, conjecture, teacher);
					conjecture->incNumberOfInputs(deltaInputs);
				}
				
				for (auto& ot : components) {
					sequence_in_t suffix(ce);
					while (suffix.size() > 1) {
						ot.E.emplace_back(suffix);
						suffix.pop_front();
					}
				}
				// process CE
				//updateTree(ce, dt, stateNodes, conjecture, teacher);

				if (provideTentativeModel) {
					unlearned = provideTentativeModel(conjecture);
				}
			}
		}
		return conjecture;
	}
}
