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
#include "../PrefixSet.h"

namespace FSMlearning {
	struct ot_node_t {
		output_t incomingOutput;
		state_t state;
		output_t stateOutput;
		sequence_in_t accessSeq;
		map<input_t, shared_ptr<ot_node_t>> succ;

		input_t distInput;

		ot_node_t(output_t incomingOutput, sequence_in_t accessSeq) :
			incomingOutput(incomingOutput), state(NULL_STATE),
			stateOutput(DEFAULT_OUTPUT), accessSeq(accessSeq) {
		}
	};
	
	static void printTree(const shared_ptr<ot_node_t>& node, string prefix) {
		printf("%d/%s %s\n", 
			node->state, FSMmodel::getOutSequenceAsString(sequence_out_t({ node->stateOutput })).c_str(),
			FSMmodel::getInSequenceAsString(sequence_in_t({ node->accessSeq })).c_str());
		for (auto& p : node->succ) {
			printf("%s -%d/%s-> ", prefix.c_str(), p.first,
				FSMmodel::getOutSequenceAsString(sequence_out_t({ p.second->incomingOutput })).c_str());
			printTree(p.second, prefix + "  ");
		}
	}

	static bool areDistinguished(const shared_ptr<ot_node_t>& n1, const shared_ptr<ot_node_t>& n2) {
		if (n1->stateOutput != n2->stateOutput) return true;
		for (auto& p : n1->succ) {
			auto it = n2->succ.find(p.first);
			if (it != n2->succ.end()) {
				if ((p.second->incomingOutput != it->second->incomingOutput)
					|| (areDistinguished(p.second, it->second))) {
					n1->distInput = p.first;
					return true;
				}
			}
		}
		return false;
	}

	static sequence_in_t getDistinguishingSeq(shared_ptr<ot_node_t> n1, shared_ptr<ot_node_t> n2, bool isStateOutput) {
		sequence_in_t distSeq;
		do {
			distSeq.emplace_back(n1->distInput);
			if (isStateOutput) distSeq.emplace_back(STOUT_INPUT);
			n2 = n2->succ.at(n1->distInput);
			n1 = n1->succ.at(n1->distInput);
		} while ((n1->incomingOutput == n2->incomingOutput) && (n1->stateOutput == n2->stateOutput));
		return distSeq;
	}

	static void addNodes(shared_ptr<ot_node_t> node, sequence_in_t& seq,
			const unique_ptr<DFSM>& conjecture, const unique_ptr<Teacher>& teacher) {
		sequence_in_t accessSeq(node->accessSeq);
		sequence_out_t outputSeq;
		bool isRL = teacher->isProvidedOnlyMQ();// TeacherRL gives only the last output
		if (!isRL) outputSeq = teacher->resetAndOutputQueryOnSuffix(accessSeq, seq);
		else teacher->resetAndOutputQuery(accessSeq);
		auto outIt = outputSeq.begin();
		for (auto& input : seq) {
			if (input == STOUT_INPUT) {
				node->stateOutput = isRL ? teacher->outputQuery(input) : *outIt;
			}
			else {
				accessSeq.push_back(input);
				node->succ.emplace(input, make_shared<ot_node_t>(isRL ? teacher->outputQuery(input) : *outIt, accessSeq));
				node = node->succ.at(input);
			}
			if (!isRL) ++outIt;
		}
	}

	static void extendNode(const shared_ptr<ot_node_t>& node, sequence_in_t& seq, 
		const unique_ptr<DFSM>& conjecture, const unique_ptr<Teacher>& teacher) {

		auto it = node->succ.find(seq.front());
		if (it == node->succ.end()) {
			addNodes(node, seq, conjecture, teacher);
		}
		else if (seq.size() > 1 + conjecture->isOutputState()) {// is the end of sequence?
			seq.pop_front();
			if (conjecture->isOutputState()) seq.pop_front(); // STOUT_INPUT
			extendNode(it->second, seq, conjecture, teacher);
		}
	}

	static void extendNode(const shared_ptr<ot_node_t>& node, const sequence_set_t& D, 
		const unique_ptr<DFSM>& conjecture, const unique_ptr<Teacher>& teacher) {
		for (auto seq : D) {
			extendNode(node, seq, conjecture, teacher);
		}
	}

	static bool buildQuotient(const shared_ptr<ot_node_t>& root, FSMlib::PrefixSet& pset, vector<shared_ptr<ot_node_t>>& stateNodes,
			const unique_ptr<DFSM>& conjecture, const unique_ptr<Teacher>& teacher) {
		
		auto D = pset.getMaximalSequences();
		queue<shared_ptr<ot_node_t>> openNodes;
		openNodes.emplace(root);
		while (!openNodes.empty()) {
			auto node = move(openNodes.front());
			openNodes.pop();

			extendNode(node, D, conjecture, teacher);
			if ((node->state != NULL_STATE) && (node == stateNodes[node->state])) {
				for (input_t input = 0; input < conjecture->getNumberOfInputs(); input++) {
					openNodes.emplace(node->succ.at(input));
				}
				continue;
			}
			node->state = NULL_STATE;
			for (auto& sn : stateNodes) {
				if (!areDistinguished(node, sn)) {
					node->state = sn->state;
					break;
				}
			}
			if (node->state == NULL_STATE) {// new state
				node->state = state_t(stateNodes.size());
				stateNodes.emplace_back(node);
				for (input_t input = 0; input < conjecture->getNumberOfInputs(); input++) {
					openNodes.emplace(node->succ.at(input));
				}
			}
		}
		//printTree(root, "");
		conjecture->create(state_t(stateNodes.size()), teacher->getNumberOfInputs(), teacher->getNumberOfOutputs());
		for (auto& sn : stateNodes) {
			if (conjecture->isOutputState()) conjecture->setOutput(sn->state, sn->stateOutput);
			for (auto& p : sn->succ) {
				conjecture->setTransition(sn->state, p.first, p.second->state, 
					conjecture->isOutputTransition() ? p.second->incomingOutput : DEFAULT_OUTPUT);
			}
		}
		bool consistent = true;
		for (auto& sn : stateNodes) {
			for (auto& p : sn->succ) {
				if (p.second != stateNodes[p.second->state]) {
					// label
					openNodes.emplace(p.second);
					while (!openNodes.empty()) {
						auto node = move(openNodes.front());
						openNodes.pop();

						for (auto& succ : node->succ) {
							auto nextState = conjecture->getNextState(node->state, succ.first);
							if (areDistinguished(succ.second, stateNodes[nextState])) {// inconsistency
								pset.insert(getDistinguishingSeq(succ.second, stateNodes[nextState], conjecture->isOutputState()));
								consistent = false;
								break;
							}
							succ.second->state = nextState;
							openNodes.emplace(succ.second);
						}
						if (!consistent) break;
					}
					if (!consistent) break;
				}
			}
			if (!consistent) break;
		}
		return consistent;
	}

	unique_ptr<DFSM> QuotientAlgorithm(const unique_ptr<Teacher>& teacher,
		function<bool(const unique_ptr<DFSM>& conjecture)> provideTentativeModel) {
		if (!teacher->isBlackBoxResettable()) {
			ERROR_MESSAGE("FSMlearning::ObservationPackAlgorithm - the Black Box needs to be resettable");
			return nullptr;
		}

		/// counterexample
		sequence_in_t ce;
		/// Observation Tree root
		auto ot = make_shared<ot_node_t>(DEFAULT_OUTPUT, sequence_in_t());
		vector<shared_ptr<ot_node_t>> stateNodes;

		/// distinguishing sequences
		FSMlib::PrefixSet pset;
		
		// numberOfOutputs can produce error message
		auto conjecture = FSMmodel::createFSM(teacher->getBlackBoxModelType(), 1, teacher->getNumberOfInputs(), teacher->getNumberOfOutputs());

		if (conjecture->isOutputState()) {
			ot->stateOutput = teacher->resetAndOutputQuery(STOUT_INPUT);
		}
		for (input_t input = 0; input < conjecture->getNumberOfInputs(); input++) {
			sequence_in_t seq({ input });
			if (conjecture->isOutputState()) seq.emplace_back(STOUT_INPUT);
			pset.insert(move(seq));
		}

		bool unlearned = true;
		while (unlearned) {
			do {
				if (provideTentativeModel) {
					unlearned = provideTentativeModel(conjecture);
				}
			} while (unlearned && !buildQuotient(ot, pset, stateNodes, conjecture, teacher));
			if (!unlearned) break;
			if (provideTentativeModel) {
				unlearned = provideTentativeModel(conjecture);
				if (!unlearned) break;
			}
			ce = teacher->equivalenceQuery(conjecture);
			if (ce.empty()) unlearned = false;
			else {// process CE
				if (conjecture->getNumberOfInputs() != teacher->getNumberOfInputs()) {
					for (input_t input = conjecture->getNumberOfInputs(); input < teacher->getNumberOfInputs(); input++) {
						sequence_in_t seq({ input });
						if (conjecture->isOutputState()) seq.emplace_back(STOUT_INPUT);
						pset.insert(move(seq));
					}
					conjecture->incNumberOfInputs(teacher->getNumberOfInputs() - conjecture->getNumberOfInputs());
				}
				if (conjecture->isOutputState()) {// check CE for interleaving with STOUT_INPUT
					sequence_in_t newCE;
					for (auto& input : ce) {
						if (input == STOUT_INPUT) continue;
						newCE.emplace_back(input);
						newCE.emplace_back(STOUT_INPUT);
					}
					ce.swap(newCE);
				}
				extendNode(ot, sequence_in_t(ce), conjecture, teacher);
				auto node = ot;
				while (!ce.empty()) {
					if (ce.front() != STOUT_INPUT) {
						if ((node != stateNodes[node->state]) && areDistinguished(node, stateNodes[node->state])) {// inconsistency
							pset.insert(getDistinguishingSeq(node, stateNodes[node->state], conjecture->isOutputState()));
							break;
						}
						node->succ.at(ce.front())->state = conjecture->getNextState(node->state, ce.front());
						node = node->succ.at(ce.front());
					}
					ce.pop_front();
				}
			}
		}
		return conjecture;
	}
}
