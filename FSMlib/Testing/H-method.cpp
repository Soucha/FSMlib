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
		map<input_t, TestNodeH*> next;

		TestNodeH(state_t state, output_t stateOutput, output_t inOutput) :
			incomingOutput(inOutput), stateOutput(stateOutput), state(state) {
		}

		~TestNodeH() {
			for (auto n : next) {
				delete n.second;
			}
		}
	};

	static vector<TestNodeH*> coreNodes, extNodes; // stores SC, TC-SC respectively
	static DFSM * specification;
	static vector<LinkCell> sepSeq;
	static vector<state_t> states;

	static void cleanup() {
		delete coreNodes[0];
		coreNodes.clear();
		extNodes.clear();
		sepSeq.clear();
	}

	static void printTStree(TestNodeH* node, string prefix = "") {
		printf("%s%d/%d <- %d (in%d)\n", prefix.c_str(), node->state, node->stateOutput, node->incomingOutput, node->distinguishingInput);
		for (auto it : node->next) {
			printf("%s - %d ->\n", prefix.c_str(), it.first);
			printTStree(it.second, prefix + "\t");
		}
	}

	static state_t getPairIdx(state_t s1, state_t s2) {
		return (s1 < s2) ?
			(s1 * specification->getNumberOfStates() + s2 - 1 - (s1 * (s1 + 3)) / 2) :
			(s2 * specification->getNumberOfStates() + s1 - 1 - (s2 * (s2 + 3)) / 2);
	}

	static void createBasicTree(DFSM * fsm, int extraStates) {
		output_t outputState, outputTransition;
		state_t state;
		vector<bool> covered(fsm->getNumberOfStates(), false);
		
		coreNodes.clear();
		extNodes.clear();

		// root
		outputState = (fsm->isOutputState()) ? fsm->getOutput(0, STOUT_INPUT) : DEFAULT_OUTPUT;
		TestNodeH* node = new TestNodeH(0, outputState, DEFAULT_OUTPUT);
		coreNodes.push_back(node);
		covered[0] = true;
		for (state_t idx = 0; idx != coreNodes.size(); idx++) {
			for (input_t input = 0; input < fsm->getNumberOfInputs(); input++) {
				state = fsm->getNextState(states[coreNodes[idx]->state], input);
				if (state == NULL_STATE) continue;
				outputState = (fsm->isOutputState()) ? fsm->getOutput(state, STOUT_INPUT) : DEFAULT_OUTPUT;
				outputTransition = (fsm->isOutputTransition()) ? fsm->getOutput(states[coreNodes[idx]->state], input) : DEFAULT_OUTPUT;
				state = getIdx(states, state);
				node = new TestNodeH(state, outputState, outputTransition);
				coreNodes[idx]->next[input] = node;
				if (covered[state]) {
					extNodes.push_back(node);
				}
				else {
					coreNodes.push_back(node);
					covered[state] = true;
				}
			}
		}
		// extNodes now contains TC-SC, coreNodes = SC
		if (extraStates > 0) {
			stack<TestNodeH*> lifo;
			for (auto n : extNodes) {
				n->distinguishingInput = extraStates; // distinguishingInput is used here as a counter
				lifo.push(n);
				while (!lifo.empty()) {
					auto actNode = lifo.top();
					lifo.pop();
					for (input_t input = 0; input < fsm->getNumberOfInputs(); input++) {
						state = fsm->getNextState(states[actNode->state], input);
						if (state == NULL_STATE) continue;
						outputState = (fsm->isOutputState()) ? fsm->getOutput(state, STOUT_INPUT) : DEFAULT_OUTPUT;
						outputTransition = (fsm->isOutputTransition()) ? fsm->getOutput(states[actNode->state], input) : DEFAULT_OUTPUT;
						state = getIdx(states, state);
						node = new TestNodeH(state, outputState, outputTransition);
						actNode->next[input] = node;
						if (actNode->distinguishingInput > 1) {
							node->distinguishingInput = actNode->distinguishingInput - 1;
							lifo.push(node);
						}
					}
				}
			}
		}
	}

	static int getEstimate(TestNodeH* n1, TestNodeH* n2, input_t input) {
		state_t idx = getPairIdx(n1->state, n2->state);
		state_t nextIdx = sepSeq[idx].next[input];
		if (nextIdx == NULL_STATE) return -1;
		if (nextIdx == idx) return 1;
		return 2 * sepSeq[nextIdx].minLen + 1;
	}

	static int getMinLenToDistinguish(TestNodeH* n1, TestNodeH* n2) {
		map<input_t, TestNodeH*>::iterator sIt1, sIt2;
		int minVal = specification->getNumberOfStates();
		input_t input = STOUT_INPUT;
		for (input_t i = 0; i < specification->getNumberOfInputs(); i++) {
			if ((sIt1 = n1->next.find(i)) != n1->next.end()) {
				if ((sIt2 = n2->next.find(i)) != n2->next.end()) {
					if ((sIt1->second->incomingOutput != sIt2->second->incomingOutput) ||
						(sIt1->second->stateOutput != sIt2->second->stateOutput)) return 0; //distinguished
					if (sIt1->second->state == sIt2->second->state) continue;
					int est = getMinLenToDistinguish(sIt1->second, sIt2->second);
					if (est == 0) return 0; //distinguished
					if (minVal >= est) {
						minVal = est;
						input = i;
					}
				}
				else {
					int est = getEstimate(n1, n2, i);
					if ((minVal > est) && (est > 0)) {
						minVal = est;
						input = i;
					}
				}
			}
			else if ((sIt2 = n2->next.find(i)) != n2->next.end()) {
				int est = getEstimate(n2, n1, i);
				if ((minVal > est) && (est > 0)) {
					minVal = est;
					input = i;
				}
			}
			else {
				int est = getEstimate(n2, n1, i);
				if ((minVal > est + 1) && (est > 0)) {
					minVal = est + 1;
					input = i;
				}
			}
		}
		n1->distinguishingInput = input;
		return minVal;
	}

	static void appendSeparatingSequence(TestNodeH* n1, TestNodeH* n2) {
		state_t idx = getPairIdx(n1->state, n2->state), nextIdx;
		input_t input = STOUT_INPUT;
		for (input_t i = 0; i < specification->getNumberOfInputs(); i++) {
			nextIdx = sepSeq[idx].next[i];
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
			state_t state = specification->getNextState(states[n1->state], input);
			outputState1 = (specification->isOutputState()) ? specification->getOutput(state, STOUT_INPUT) : DEFAULT_OUTPUT;
			outputTransition1 = (specification->isOutputTransition()) ? 
				specification->getOutput(states[n1->state], input) : DEFAULT_OUTPUT;
			state = getIdx(states, state);
			TestNodeH* node = new TestNodeH(state, outputState1, outputTransition1);
			n1->next[input] = node;
		}
		else {
			outputState1 = it->second->stateOutput;
			outputTransition1 = it->second->incomingOutput;
		}
		it = n2->next.find(input);
		if (it == n2->next.end()) {
			state_t state = specification->getNextState(states[n2->state], input);
			outputState2 = (specification->isOutputState()) ? specification->getOutput(state, STOUT_INPUT) : DEFAULT_OUTPUT;
			outputTransition2 = (specification->isOutputTransition()) ?
				specification->getOutput(states[n2->state], input) : DEFAULT_OUTPUT;
			state = getIdx(states, state);
			TestNodeH* node = new TestNodeH(state, outputState2, outputTransition2);
			n2->next[input] = node;
		}
		else {
			outputState2 = it->second->stateOutput;
			outputTransition2 = it->second->incomingOutput;
		}
		if ((outputState1 == outputState2) && (outputTransition1 == outputTransition2)) {
			appendSeparatingSequence(n1->next[input], n2->next[input]);
			//  } else {
			//        printTStree(coreNodes[0]);
		}
	}

	static void addSeparatingSequence(TestNodeH* n1, TestNodeH* n2) {
		auto fIt = n1->next.find(n1->distinguishingInput);
		if (fIt == n1->next.end()) {
			appendSeparatingSequence(n2, n1);
			return;
		}
		auto sIt = n2->next.find(n1->distinguishingInput);
		if (sIt == n2->next.end()) {
			appendSeparatingSequence(n1, n2);
			return;
		}
		addSeparatingSequence(fIt->second, sIt->second);
	}

	static void distinguish(TestNodeH* n1, TestNodeH* n2) {
		if ((n1->state == n2->state) || (n1->stateOutput != n2->stateOutput)
			|| (getMinLenToDistinguish(n1, n2) == 0)) return;
		addSeparatingSequence(n1, n2);
	}

	static void distinguish(vector<TestNodeH*>& nodes, TestNodeH* currNode, int depth, bool extend = false) {
		if (depth > 0) {
			if (extend) nodes.push_back(currNode);
			for (auto pNext : currNode->next) {
				distinguish(nodes, pNext.second, depth - 1, extend);
			}
			if (extend) nodes.pop_back();
		}
		for (auto n : nodes) {
			distinguish(n, currNode);
		}
	}

	static void getSequences(TestNodeH* node, sequence_set_t& TS) {
		stack< pair<TestNodeH*, sequence_in_t> > lifo;
		sequence_in_t seq;
		if (specification->isOutputState()) seq.push_back(STOUT_INPUT);
		lifo.push(make_pair(node, seq));
		while (!lifo.empty()) {
			auto p = lifo.top();
			lifo.pop();
			if (p.first->next.empty()) {
				TS.insert(p.second);
			}
			else {
				for (auto pNext : p.first->next) {
					seq = p.second;
					seq.push_back(pNext.first);
					if (specification->isOutputState()) seq.push_back(STOUT_INPUT);
					lifo.push(make_pair(pNext.second, seq));
				}
			}
		}
	}

	void H_method(DFSM* fsm, sequence_set_t & TS, int extraStates) {
		TS.clear();
		if (extraStates < 0) {
			return;
		}
		specification = fsm;
		states = fsm->getStates();

		sepSeq = getSeparatingSequences(fsm);

		// 1. step
		createBasicTree(fsm, extraStates);
		//printTStree(coreNodes[0]);

		// 2. step
		for (auto n1it = coreNodes.begin(); n1it != coreNodes.end(); n1it++) {
			for (auto n2it = n1it + 1; n2it != coreNodes.end(); n2it++) {
				distinguish(*n1it, *n2it);
			}
		}

		// 3. step
		for (auto n1it = extNodes.begin(); n1it != extNodes.end(); n1it++) {
			distinguish(coreNodes, *n1it, extraStates);
		}

		// 4. step
		if (extraStates > 0) {
			vector<TestNodeH*> tmp;
			for (auto n1it = extNodes.begin(); n1it != extNodes.end(); n1it++) {
				distinguish(tmp, *n1it, extraStates, true);
			}
		}

		// obtain TS
		//printTStree(coreNodes[0]);
		getSequences(coreNodes[0], TS);

		cleanup();
	}
}
