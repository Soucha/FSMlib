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

#include "FSMtesting.h"

using namespace FSMsequence;

namespace FSMtesting {
	struct TestNodeH {
		output_t incomingOutput;
		output_t stateOutput;
		state_t state;
		input_t distinguishingInput = STOUT_INPUT;
		map<input_t, shared_ptr<TestNodeH>> next;

		TestNodeH(state_t state, output_t stateOutput, output_t inOutput) :
			incomingOutput(inOutput), stateOutput(stateOutput), state(state) {
		}
	};

	static void printTStree(const shared_ptr<TestNodeH>& node, string prefix = "") {
		printf("%s%d/%d <- %d (in%d)\n", prefix.c_str(), node->state, node->stateOutput, node->incomingOutput, node->distinguishingInput);
		for (auto it : node->next) {
			printf("%s - %d ->\n", prefix.c_str(), it.first);
			printTStree(it.second, prefix + "\t");
		}
	}

	static void createBasicTree(const unique_ptr<DFSM>& fsm, vector<shared_ptr<TestNodeH>>& coreNodes,
			vector<shared_ptr<TestNodeH>>& extNodes, int extraStates) {
		output_t outputState, outputTransition;
		vector<bool> covered(fsm->getNumberOfStates(), false);
		// root
		outputState = (fsm->isOutputState()) ? fsm->getOutput(0, STOUT_INPUT) : DEFAULT_OUTPUT;
		coreNodes.emplace_back(make_shared<TestNodeH>(0, outputState, DEFAULT_OUTPUT));
		covered[0] = true;
		for (state_t idx = 0; idx != coreNodes.size(); idx++) {
			for (input_t input = 0; input < fsm->getNumberOfInputs(); input++) {
				auto state = fsm->getNextState(coreNodes[idx]->state, input);
				if (state == NULL_STATE) {
					coreNodes[idx]->next[input] = make_shared<TestNodeH>(state, WRONG_OUTPUT, WRONG_OUTPUT);
					continue;
				}
				outputState = (fsm->isOutputState()) ? fsm->getOutput(state, STOUT_INPUT) : DEFAULT_OUTPUT;
				outputTransition = (fsm->isOutputTransition()) ? fsm->getOutput(coreNodes[idx]->state, input) : DEFAULT_OUTPUT;
				auto node = make_shared<TestNodeH>(state, outputState, outputTransition);
				coreNodes[idx]->next[input] = node;
				if (covered[state]) {
					extNodes.emplace_back(move(node));
				}
				else {
					coreNodes.emplace_back(move(node));
					covered[state] = true;
				}
			}
		}
		// extNodes now contains TC-SC, coreNodes = SC
		if (extraStates > 0) {
			stack<shared_ptr<TestNodeH>> lifo;
			for (const auto& n : extNodes) {
				n->distinguishingInput = extraStates; // distinguishingInput is used here as a counter
				lifo.emplace(n);
				while (!lifo.empty()) {
					auto actNode = move(lifo.top());
					lifo.pop();
					for (input_t input = 0; input < fsm->getNumberOfInputs(); input++) {
						auto state = fsm->getNextState(actNode->state, input);
						if (state == NULL_STATE) {
							actNode->next[input] = make_shared<TestNodeH>(state, WRONG_OUTPUT, WRONG_OUTPUT);
							continue;
						}
						outputState = (fsm->isOutputState()) ? fsm->getOutput(state, STOUT_INPUT) : DEFAULT_OUTPUT;
						outputTransition = (fsm->isOutputTransition()) ? fsm->getOutput(actNode->state, input) : DEFAULT_OUTPUT;
						auto node = make_shared<TestNodeH>(state, outputState, outputTransition);
						if (actNode->distinguishingInput > 1) {
							node->distinguishingInput = actNode->distinguishingInput - 1;
							lifo.emplace(node);
						}
						actNode->next[input] = move(node);
					}
				}
			}
		}
	}

	static seq_len_t getEstimate(const shared_ptr<TestNodeH>& n1, const shared_ptr<TestNodeH>& n2, const input_t& input,
			const vector<LinkCell>& sepSeq) {
		auto idx = getStatePairIdx(n1->state, n2->state);
		auto nextIdx = sepSeq[idx].next[input];
		if (nextIdx == NULL_STATE) return seq_len_t(-1);
		if (nextIdx == idx) return 1;
		return 2 * sepSeq[nextIdx].minLen + 1;
	}

	static seq_len_t getMinLenToDistinguish(const shared_ptr<TestNodeH>& n1, const shared_ptr<TestNodeH>& n2,
			const vector<LinkCell>& sepSeq, const state_t& N, const input_t& P) {
		seq_len_t minVal = N;
		input_t input = STOUT_INPUT;
		for (input_t i = 0; i < P; i++) {
			auto sIt1 = n1->next.find(i);
			auto sIt2 = n2->next.find(i);
			if (sIt1 != n1->next.end()) {
				if (sIt2 != n2->next.end()) {
					if ((sIt1->second->incomingOutput != sIt2->second->incomingOutput) ||
						(sIt1->second->stateOutput != sIt2->second->stateOutput)) return 0; //distinguished
					if (sIt1->second->state == sIt2->second->state) continue;
					auto est = getMinLenToDistinguish(sIt1->second, sIt2->second, sepSeq, N, P);
					if (est == 0) return 0; //distinguished
					if (minVal >= est) {
						minVal = est;
						input = i;
					}
				}
				else {
					auto est = getEstimate(n1, n2, i, sepSeq);
					if (minVal > est) {
						minVal = est;
						input = i;
					}
				}
			}
			else {
				if (sIt2 != n2->next.end()) {
					auto est = getEstimate(n2, n1, i, sepSeq);
					if (minVal > est) {
						minVal = est;
						input = i;
					}
				}
				else {
					auto est = getEstimate(n2, n1, i, sepSeq);
					if (minVal - 1 > est) {
						minVal = est + 1;
						input = i;
					}
				}
			}
		}
		n1->distinguishingInput = input;
		return minVal;
	}

	static void appendSeparatingSequence(const shared_ptr<TestNodeH>& n1, const shared_ptr<TestNodeH>& n2,
			const unique_ptr<DFSM>& fsm, const vector<LinkCell>& sepSeq) {
		auto idx = getStatePairIdx(n1->state, n2->state);
		input_t input = STOUT_INPUT;
		for (input_t i = 0; i < fsm->getNumberOfInputs(); i++) {
			auto nextIdx = sepSeq[idx].next[i];
			if (nextIdx == NULL_STATE) continue;
			if ((nextIdx == idx) || (sepSeq[nextIdx].minLen == sepSeq[idx].minLen - 1)) {
				if (n1->next.find(i) != n1->next.end()) {
					input = i;
					break;
				}
				if (input == STOUT_INPUT) {
					input = i;
				}
			}
		}
		output_t outputState1, outputState2, outputTransition1, outputTransition2;
		auto it = n1->next.find(input);
		if (it == n1->next.end()) {
			state_t state = fsm->getNextState(n1->state, input);
			if (state == NULL_STATE) {
				outputState1 = outputTransition1 = WRONG_OUTPUT;
			}
			else {
				outputState1 = (fsm->isOutputState()) ? fsm->getOutput(state, STOUT_INPUT) : DEFAULT_OUTPUT;
				outputTransition1 = (fsm->isOutputTransition()) ?
					fsm->getOutput(n1->state, input) : DEFAULT_OUTPUT;
			}
			n1->next[input] = make_shared<TestNodeH>(state, outputState1, outputTransition1);
		}
		else {
			outputState1 = it->second->stateOutput;
			outputTransition1 = it->second->incomingOutput;
		}
		it = n2->next.find(input);
		if (it == n2->next.end()) {
			state_t state = fsm->getNextState(n2->state, input);
			if (state == NULL_STATE) {
				outputState1 = outputTransition1 = WRONG_OUTPUT;
			}
			else {
				outputState2 = (fsm->isOutputState()) ? fsm->getOutput(state, STOUT_INPUT) : DEFAULT_OUTPUT;
				outputTransition2 = (fsm->isOutputTransition()) ?
					fsm->getOutput(n2->state, input) : DEFAULT_OUTPUT;
			}
			n2->next[input] = make_shared<TestNodeH>(state, outputState2, outputTransition2);
		}
		else {
			outputState2 = it->second->stateOutput;
			outputTransition2 = it->second->incomingOutput;
		}
		if ((outputState1 == outputState2) && (outputTransition1 == outputTransition2)) {
			appendSeparatingSequence(n1->next[input], n2->next[input], fsm, sepSeq);
			//  } else {
			//        printTStree(coreNodes[0]);
		}
	}

	static void addSeparatingSequence(const shared_ptr<TestNodeH>& n1, const shared_ptr<TestNodeH>& n2,
			const unique_ptr<DFSM>& fsm, const vector<LinkCell>& sepSeq) {
		auto fIt = n1->next.find(n1->distinguishingInput);
		if (fIt == n1->next.end()) {
			appendSeparatingSequence(n2, n1, fsm, sepSeq);
			return;
		}
		auto sIt = n2->next.find(n1->distinguishingInput);
		if (sIt == n2->next.end()) {
			appendSeparatingSequence(n1, n2, fsm, sepSeq);
			return;
		}
		addSeparatingSequence(fIt->second, sIt->second, fsm, sepSeq);
	}

	static void distinguish(const shared_ptr<TestNodeH>& n1, const shared_ptr<TestNodeH>& n2,
		const unique_ptr<DFSM>& fsm, const vector<LinkCell>& sepSeq) {
		if ((n1->state == n2->state) || (n1->stateOutput != n2->stateOutput)
			|| (getMinLenToDistinguish(n1, n2, sepSeq, fsm->getNumberOfStates(), fsm->getNumberOfInputs()) == 0)) return;
		addSeparatingSequence(n1, n2, fsm, sepSeq);
	}

	static void distinguish(vector<shared_ptr<TestNodeH>>& nodes, const shared_ptr<TestNodeH>& currNode, 
		const unique_ptr<DFSM>& fsm, const vector<LinkCell>& sepSeq , int depth, bool extend = false) {
		for (const auto& n : nodes) {
			distinguish(n, currNode, fsm, sepSeq);
		}
		if (depth > 0) {
			if (extend) nodes.emplace_back(currNode);
			for (const auto& pNext : currNode->next) {
				distinguish(nodes, pNext.second, fsm, sepSeq, depth - 1, extend);
			}
			if (extend) nodes.pop_back();
		}
		
	}

	static sequence_set_t getSequences(const shared_ptr<TestNodeH>& node, const unique_ptr<DFSM>& fsm) {
		sequence_set_t TS;
		stack<pair<shared_ptr<TestNodeH>, sequence_in_t>> lifo;
		sequence_in_t seq;
		if (fsm->isOutputState()) seq.push_back(STOUT_INPUT);
		lifo.emplace(node, move(seq));
		while (!lifo.empty()) {
			auto p = move(lifo.top());
			lifo.pop();
			if (p.first->next.empty()) {
				TS.emplace(move(p.second));
			}
			else {
				for (const auto& pNext : p.first->next) {
					sequence_in_t seq(p.second);
					seq.push_back(pNext.first);
					if (fsm->isOutputState()) seq.push_back(STOUT_INPUT);
					lifo.emplace(pNext.second, move(seq));
				}
			}
		}
		return TS;
	}

	sequence_set_t H_method(const unique_ptr<DFSM>& fsm, int extraStates) {
		RETURN_IF_UNREDUCED(fsm, "FSMtesting::H_method", sequence_set_t());
		if (extraStates < 0) {
			return sequence_set_t();
		}
		auto sepSeq = getSeparatingSequences(fsm);
		vector<shared_ptr<TestNodeH>> coreNodes, extNodes; // stores SC, TC-SC respectively

		// 1. step
		createBasicTree(fsm, coreNodes, extNodes, extraStates);
		//printTStree(coreNodes[0]);

		// 2. step
		for (auto n1it = coreNodes.begin(); n1it != coreNodes.end(); n1it++) {
			for (auto n2it = n1it + 1; n2it != coreNodes.end(); n2it++) {
				distinguish(*n1it, *n2it, fsm, sepSeq);
			}
		}

		// 3. step
		for (const auto& node : extNodes) {
			distinguish(coreNodes, node, fsm, sepSeq, extraStates);
		}

		// 4. step
		if (extraStates > 0) {
			vector<shared_ptr<TestNodeH>> tmp;
			for (const auto& node : extNodes) {
				distinguish(tmp, node, fsm, sepSeq, extraStates, true);
			}
		}

		// obtain TS
		//printTStree(coreNodes[0]);
		return getSequences(coreNodes[0], fsm);
	}
}
