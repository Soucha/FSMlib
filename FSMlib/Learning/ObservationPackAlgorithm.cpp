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
	struct componentOT {
		sequence_in_t S;
		vector<sequence_in_t> E;
		map<sequence_in_t, vector<sequence_out_t>> T;

		componentOT(sequence_in_t S, vector<sequence_in_t>&& E, vector<sequence_out_t>&& refRow) :
			S(S), E(move(E)) {
			T.emplace(move(S), move(refRow));
		}
	};

	static void checkNumberOfOutputs(const unique_ptr<Teacher>& teacher, const unique_ptr<DFSM>& conjecture) {
		if (conjecture->getNumberOfOutputs() != teacher->getNumberOfOutputs()) {
			conjecture->incNumberOfOutputs(teacher->getNumberOfOutputs() - conjecture->getNumberOfOutputs());
		}
	}

	static shared_ptr<dt_node_t> createNode(const shared_ptr<dt_node_t>& parent, sequence_in_t s, sequence_out_t output, size_t sepSeqIdx = 0) {
		auto leaf = make_shared<dt_node_t>();
		leaf->level = sepSeqIdx;
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
			auto output = teacher->resetAndOutputQueryOnSuffix(node->sequence, STOUT_INPUT);
#if DUMP_OQ 
			printf("%d T(%s, S) = %d addState\n", teacher->getOutputQueryCount(), FSMmodel::getInSequenceAsString(node->sequence).c_str(),
			output);
#endif // DUMP_OQ
			checkNumberOfOutputs(teacher, conjecture);
			conjecture->setOutput(node->state, output);
		}
	}

	static shared_ptr<dt_node_t> sift(const shared_ptr<dt_node_t>& dt, const sequence_in_t& s, const unique_ptr<Teacher>& teacher) {
		auto currentNode = dt;
		while (currentNode->state == NULL_STATE) {
			auto output = teacher->resetAndOutputQueryOnSuffix(s, currentNode->sequence);
#if DUMP_OQ 
			printf("%d T(%s, %s) = %s sift\n", teacher->getOutputQueryCount(), FSMmodel::getInSequenceAsString(s).c_str(),
			FSMmodel::getInSequenceAsString(currentNode->sequence).c_str(), FSMmodel::getOutSequenceAsString(output).c_str());
#endif // DUMP_OQ
			auto it = currentNode->succ.find(output);
			if (it == currentNode->succ.end()) {
				return createNode(currentNode, s, move(output));
			}
			currentNode = it->second;
		}
		return currentNode;
	}

	static bool isRowClosed(vector<sequence_out_t>& row, const sequence_in_t& prefix, const componentOT& ot,
		size_t startColumn, size_t& sepSeqIdx, const unique_ptr<Teacher>& teacher) {
		bool undististinguished = true;
		auto& refRow = ot.T.at(ot.S);
		for (auto e = startColumn; e < ot.E.size(); e++) {
			row.emplace_back(teacher->resetAndOutputQueryOnSuffix(prefix, ot.E[e]));
#if DUMP_OQ 
			printf("%d T(%s, %s) = %s isRowClosed\n", teacher->getOutputQueryCount(), FSMmodel::getInSequenceAsString(prefix).c_str(),
			FSMmodel::getInSequenceAsString(ot.E[e]).c_str(), FSMmodel::getOutSequenceAsString(row.back()).c_str());
#endif // DUMP_OQ
			if (undististinguished && (row.back() != refRow[e])) {
				sepSeqIdx = e;
				undististinguished = false;
			}
		}
		return undististinguished;
	}

	static void split(shared_ptr<dt_node_t>& dtNode, vector<shared_ptr<dt_node_t>>& stateNodes, vector<componentOT>& components,
		vector<sequence_in_t> E, size_t sepSeqIdx, sequence_out_t& outputToMoved,
		sequence_in_t prefix, vector<sequence_out_t>& row, size_t sepSeqOfNewComponent,
		const unique_ptr<DFSM>& conjecture, const unique_ptr<Teacher>& teacher) {
		auto leaf = createNode(dtNode, move(dtNode->sequence), outputToMoved, dtNode->level);
		leaf->state = dtNode->state;
		stateNodes[dtNode->state] = leaf;
		dtNode->sequence = E[sepSeqIdx];
		dtNode->state = NULL_STATE;
		dtNode->level = sepSeqIdx;

		leaf = createNode(dtNode, prefix, row[sepSeqIdx], sepSeqOfNewComponent);
		addState(leaf, stateNodes, conjecture, teacher);
		components.emplace_back(move(prefix), move(E), move(row));

		dtNode = leaf;
	}

	static void separateRow(vector<sequence_out_t>& row, sequence_in_t accessSeq, shared_ptr<dt_node_t> currentNode,
		const state_t& state, const size_t& sepSeqIdx, vector<shared_ptr<dt_node_t>>& stateNodes,
		vector<componentOT>& components, vector<componentOT>& newComponents,
		const unique_ptr<DFSM>& conjecture, const unique_ptr<Teacher>& teacher) {
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
			auto E = components[state].E;
			newComponents.emplace_back(accessSeq, move(E), move(row));
		}
		else {
			auto& otherOT = (currentNode->state < components.size()) ?
				components[currentNode->state] : newComponents[currentNode->state - components.size()];
			auto& refOtherRow = otherOT.T.at(otherOT.S);
			auto sepIdx = otherOT.E.size();
			if (currentNode->state == state) {
				sepIdx = sepSeqIdx;
			}
			else {
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
			else {// new state/component/dt_node
				split(currentNode, stateNodes, newComponents, otherOT.E, sepIdx, refOtherRow[sepIdx],
					accessSeq, row, sepSeqIdx, conjecture, teacher);
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
	}

	static void extendStateByInput(state_t state, input_t input, shared_ptr<dt_node_t>& dt, vector<shared_ptr<dt_node_t>>& stateNodes,
		vector<componentOT>& components, vector<componentOT>& newComponents, const OP_CEprocessing& processCE,
		const unique_ptr<DFSM>& conjecture, const unique_ptr<Teacher>& teacher) {
		sequence_in_t prefix(stateNodes[state]->sequence);
		prefix.emplace_back(input);
		auto dtNode = sift(dt, prefix, teacher);// what state is the next state?
		if (dtNode->state == NULL_STATE) {// new state
			addState(dtNode, stateNodes, conjecture, teacher);
			vector<sequence_in_t> E;
			vector<sequence_out_t> row;
			if (processCE == OneLocally) {
				for (input_t i = 0; i < conjecture->getNumberOfInputs(); i++) {
					sequence_in_t seq({ i });
					row.emplace_back(teacher->resetAndOutputQueryOnSuffix(prefix, seq));
#if DUMP_OQ 
					printf("%d T(%s, %s) = %s extendStateByInput\n", teacher->getOutputQueryCount(), FSMmodel::getInSequenceAsString(prefix).c_str(),
						FSMmodel::getInSequenceAsString(seq).c_str(), FSMmodel::getOutSequenceAsString(row.back()).c_str());
#endif // DUMP_OQ
					E.emplace_back(move(seq));
				}
				if (conjecture->isOutputState()) {
					row.emplace_back(sequence_out_t({ conjecture->getOutput(dtNode->state, STOUT_INPUT) }));
					E.emplace_back(sequence_in_t({ STOUT_INPUT }));
				}
			}
			else {// AllGlobally, OneGlobally - global set of suffixes
				E = components.front().E;
				for (auto& seq : E) {
					row.emplace_back(teacher->resetAndOutputQueryOnSuffix(prefix, seq));
#if DUMP_OQ 
					printf("%d T(%s, %s) = %s extendStateByInput\n", teacher->getOutputQueryCount(), FSMmodel::getInSequenceAsString(prefix).c_str(),
					FSMmodel::getInSequenceAsString(seq).c_str(), FSMmodel::getOutSequenceAsString(row.back()).c_str());
#endif // DUMP_OQ
				}
			}
			newComponents.emplace_back(move(prefix), move(E), move(row));
		}
		else {
			auto& ot = (dtNode->state < components.size()) ?
				components[dtNode->state] : newComponents[dtNode->state - components.size()];
			if (!ot.T.count(prefix)) {
				vector<sequence_out_t> row;
				size_t sepSeqIdx;
				if (isRowClosed(row, prefix, ot, 0, sepSeqIdx, teacher)) {
					ot.T.emplace(move(prefix), move(row));
				}
				else {// new component
					split(dtNode, stateNodes, newComponents, ot.E, sepSeqIdx, ot.T.at(ot.S)[sepSeqIdx],
						move(prefix), row, sepSeqIdx, conjecture, teacher);
				}
			}
		}
		conjecture->setTransition(state, input, dtNode->state);
		if (conjecture->isOutputTransition()) {
			output_t output(DEFAULT_OUTPUT);
			if (components[state].E[input].front() == input) {
				output = components[state].T[components[state].S][input].front();
			}
			else {
				for (size_t idx = 0; idx < components[state].E.size(); idx++) {
					if (components[state].E[idx].front() == input) {
						output = components[state].T[components[state].S][idx].front();
						break;
					}
				}
			}
			checkNumberOfOutputs(teacher, conjecture);
			conjecture->setOutput(state, output, input);
		}
	}

	unique_ptr<DFSM> ObservationPackAlgorithm(const unique_ptr<Teacher>& teacher, OP_CEprocessing processCounterexample,
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

		vector<sequence_in_t> E;
		vector<sequence_out_t> row;
		// add all transitions
		for (input_t i = 0; i < conjecture->getNumberOfInputs(); i++) {
			sequence_in_t seq({ i });
			row.emplace_back(teacher->resetAndOutputQuery(seq));
#if DUMP_OQ 
			printf("%d T(eps, %s) = %s OPalg\n", teacher->getOutputQueryCount(), FSMmodel::getInSequenceAsString(seq).c_str(),
			FSMmodel::getOutSequenceAsString(row.back()).c_str());
#endif // DUMP_OQ
			E.emplace_back(move(seq));
			conjecture->setTransition(0, i, 0, DEFAULT_OUTPUT);
			if (conjecture->isOutputTransition()) {
				checkNumberOfOutputs(teacher, conjecture);
				conjecture->setOutput(0, row.back().back(), i);
			}
		}
		if (conjecture->isOutputState()) {
			dt->sequence.emplace_back(STOUT_INPUT);
			auto output = teacher->resetAndOutputQuery(STOUT_INPUT);
#if DUMP_OQ 
			printf("%d T(eps, S) = %d OPalg\n", teacher->getOutputQueryCount(), output);
#endif // DUMP_OQ
			checkNumberOfOutputs(teacher, conjecture);
			conjecture->setOutput(0, output);
			row.emplace_back(sequence_out_t({ output }));
			E.emplace_back(sequence_in_t({ STOUT_INPUT }));
			auto leaf = createNode(dt, sequence_in_t(), sequence_out_t({ output }));// the initial state -> empty access sequence
			leaf->state = 0;
			stateNodes.emplace_back(leaf);
			dt->state = NULL_STATE;
		}
		else {
			stateNodes.emplace_back(dt);
			dt->state = 0;
		}
		newComponents.emplace_back(sequence_in_t(), move(E), move(row));
		bool unlearned = true;
		if (provideTentativeModel) {
			unlearned = provideTentativeModel(conjecture);
		}
		input_t deltaInputs = 0;
		while (unlearned) {
			// complete components
			for (state_t state = 0; state < components.size(); state++) {
				auto& ot = components[state];
				if (ot.T.begin()->second.size() != ot.E.size()) {// needs to be filled
					// reference row
					auto& refRow = ot.T[ot.S];
					for (auto e = refRow.size(); e < ot.E.size(); e++) {
						refRow.emplace_back(teacher->resetAndOutputQueryOnSuffix(ot.S, ot.E[e]));
#if DUMP_OQ 
						printf("%d T(%s, %s) = %s completeRefRow\n", teacher->getOutputQueryCount(), FSMmodel::getInSequenceAsString(ot.S).c_str(),
							FSMmodel::getInSequenceAsString(ot.E[e]).c_str(), FSMmodel::getOutSequenceAsString(refRow.back()).c_str());
#endif // DUMP_OQ
					}
					auto refNode = stateNodes[state];
					bool rowErased = false;// the first row can be erased so the iterator can't be incremented
					for (auto itRow = ot.T.begin(); itRow != ot.T.end(); (rowErased) ? itRow : ++itRow) {
						rowErased = false;
						if (itRow->first == ot.S) continue;
						size_t sepSeqIdx;
						if (!isRowClosed(itRow->second, itRow->first, ot, itRow->second.size(), sepSeqIdx, teacher)) {
							separateRow(itRow->second, itRow->first, refNode, state, sepSeqIdx,
								stateNodes, components, newComponents, conjecture, teacher);
							// erase row from ot
							itRow = ot.T.erase(itRow);
							rowErased = true;
							if (provideTentativeModel) {
								unlearned = provideTentativeModel(conjecture);
								if (!unlearned) break;
							}
						}
					}
					if (!unlearned) break;
				}
			}
			if (!unlearned) break;
			if (deltaInputs != 0) {// inputs were extended
				for (state_t state = 0; state < components.size(); state++) {
					for (input_t input = conjecture->getNumberOfInputs() - deltaInputs; input < conjecture->getNumberOfInputs(); input++) {
						extendStateByInput(state, input, dt, stateNodes, components, newComponents, processCounterexample, conjecture, teacher);
						if (provideTentativeModel) {
							unlearned = provideTentativeModel(conjecture);
							if (!unlearned) break;
						}
					}
					if (!unlearned) break;
				}
				deltaInputs = 0;
				if (!unlearned) break;
			}
			// process new components
			state_t numberOfOldComponents = state_t(components.size());
			for (state_t state = 0; state < newComponents.size(); state++) {
				components.emplace_back(move(newComponents[state]));
			}
			newComponents.clear();
			for (state_t state = numberOfOldComponents; state < components.size(); state++) {
				for (input_t input = 0; input < conjecture->getNumberOfInputs(); input++) {
					extendStateByInput(state, input, dt, stateNodes, components, components, processCounterexample, conjecture, teacher);
					if (provideTentativeModel) {
						unlearned = provideTentativeModel(conjecture);
						if (!unlearned) break;
					}
				}
				if (!unlearned) break;
			}
			if (!unlearned) break;
			// EQ
			ce = teacher->equivalenceQuery(conjecture);
#if DUMP_OQ 
			printf("%d EQ => %s\n", teacher->getEquivalenceQueryCount(), FSMmodel::getInSequenceAsString(ce).c_str());
#endif // DUMP_OQ
			if (ce.empty()) unlearned = false;
			else {// process CE
				if (conjecture->getNumberOfInputs() != teacher->getNumberOfInputs()) {
					deltaInputs = teacher->getNumberOfInputs() - conjecture->getNumberOfInputs();
					for (auto& ot : components) {
						for (input_t input = conjecture->getNumberOfInputs(); input < teacher->getNumberOfInputs(); input++) {
							ot.E.emplace_back(sequence_in_t({ input }));
						}
					}
					conjecture->incNumberOfInputs(deltaInputs);
				}
				if (processCounterexample == AllGlobally) {
					auto& E = components.front().E;
					vector<sequence_in_t> newE;
					sequence_in_t suffix(ce);
					while (find(E.begin(), E.end(), suffix) == E.end()) {
						newE.emplace_back(suffix);
						suffix.pop_front();
					}
					for (auto& ot : components) {
						ot.E.insert(ot.E.end(), newE.begin(), newE.end());
					}
				}
				else {
					//split
					sequence_in_t prefix, suffix(ce);
					auto bbOutput = teacher->resetAndOutputQuery(ce);
#if DUMP_OQ 
					printf("%d T(eps, %s) = %s splitCE\n", teacher->getOutputQueryCount(), 
					FSMmodel::getInSequenceAsString(ce).c_str(), FSMmodel::getOutSequenceAsString(bbOutput).c_str());
#endif // DUMP_OQ
					state_t state = 0;
					for (const auto& input : ce) {
						if (input != STOUT_INPUT) {
							if (prefix != stateNodes[state]->sequence) {
								auto output = teacher->resetAndOutputQueryOnSuffix(stateNodes[state]->sequence, suffix);
#if DUMP_OQ 
								printf("%d T(%s, %s) = %s splitCE\n", teacher->getOutputQueryCount(), FSMmodel::getInSequenceAsString(stateNodes[state]->sequence).c_str(),
								FSMmodel::getInSequenceAsString(suffix).c_str(), FSMmodel::getOutSequenceAsString(output).c_str());
#endif // DUMP_OQ
								if (bbOutput != output) {
									break;
								}
							}
							prefix.push_back(input);
							state = conjecture->getNextState(state, input);
						}
						if (bbOutput.size() > 1) bbOutput.pop_front();
						suffix.pop_front();
					}
					if (processCounterexample == OneGlobally) {
						for (auto& ot : components) {
							ot.E.emplace_back(suffix);
						}
					}
					else if (processCounterexample == OneLocally) {
						components[state].E.emplace_back(move(suffix));
					}
				}
			}
		}
		return conjecture;
	}
}
