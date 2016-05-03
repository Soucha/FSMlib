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
	struct TestNodeSPY {
		output_t incomingOutput;
		output_t stateOutput;
		state_t state;
		bool isConfirmed;
		map<input_t, TestNodeSPY*> next;

		TestNodeSPY(state_t state, output_t stateOutput, output_t inOutput) :
			incomingOutput(inOutput), stateOutput(stateOutput), state(state), isConfirmed(false) {
		}

		~TestNodeSPY() {
			for (auto n : next) {
				delete n.second;
			}
		}
	};

	static vector<TestNodeSPY*> coreNodes; // stores SC
	static vector<pair<state_t, input_t>> uncoveredTransitions;
	static vector<vector<bool>> coveredTransitions;
	static vector<vector<TestNodeSPY*>> confirmedNodes;
	static DFSM* specification;
	static vector<state_t> states;

	static void cleanup() {
		delete coreNodes[0];
		coreNodes.clear();
		uncoveredTransitions.clear();
		coveredTransitions.clear();
		confirmedNodes.clear();
	}

	static void printTStree(TestNodeSPY* node, string prefix = "") {
		printf("%s%d/%d <- %d (conf%d)\n", prefix.c_str(), node->state, node->stateOutput, node->incomingOutput, int(node->isConfirmed));
		for (auto it : node->next) {
			printf("%s - %d ->\n", prefix.c_str(), it.first);
			printTStree(it.second, prefix + "\t");
		}
	}

	static void createBasicTree(const unique_ptr<DFSM>& fsm, vector<sequence_set_t>& H) {
		output_t outputState, outputTransition;
		state_t state;
		vector<bool> covered(fsm->getNumberOfStates(), false);

		coreNodes.clear();
		uncoveredTransitions.clear();

		// root
		outputState = (fsm->isOutputState()) ? fsm->getOutput(0, STOUT_INPUT) : DEFAULT_OUTPUT;
		TestNodeSPY* node = new TestNodeSPY(0, outputState, DEFAULT_OUTPUT);
		coreNodes.push_back(node);
		covered[0] = true;
		for (state_t idx = 0; idx != coreNodes.size(); idx++) {
			coreNodes[idx]->isConfirmed = true;
			confirmedNodes[coreNodes[idx]->state].push_back(coreNodes[idx]);
			for (input_t input = 0; input < fsm->getNumberOfInputs(); input++) {
				state = fsm->getNextState(states[coreNodes[idx]->state], input);
				if (state == NULL_STATE) continue;
				if (covered[state]) {
					uncoveredTransitions.push_back(make_pair(coreNodes[idx]->state, input));
				}
				else {
					coveredTransitions[coreNodes[idx]->state][input] = true;
					outputState = (fsm->isOutputState()) ? fsm->getOutput(state, STOUT_INPUT) : DEFAULT_OUTPUT;
					outputTransition = (fsm->isOutputTransition()) ? fsm->getOutput(states[coreNodes[idx]->state], input) : DEFAULT_OUTPUT;
					state = getIdx(states, state);
					node = new TestNodeSPY(state, outputState, outputTransition);
					coreNodes[idx]->next[input] = node;
					coreNodes.push_back(node);
					covered[state] = true;
				}
			}
		}
		for (auto cn : coreNodes) {
			for (auto seq : H[cn->state]) {
				node = cn;
				for (auto input : seq) {
					auto nIt = node->next.find(input);
					if (nIt == node->next.end()) {
						state = fsm->getNextState(states[node->state], input);
						outputState = (fsm->isOutputState()) ? fsm->getOutput(state, STOUT_INPUT) : DEFAULT_OUTPUT;
						outputTransition = (fsm->isOutputTransition()) ? fsm->getOutput(states[node->state], input) : DEFAULT_OUTPUT;
						state = getIdx(states, state);
						TestNodeSPY* nextNode = new TestNodeSPY(state, outputState, outputTransition);
						node->next[input] = nextNode;
						node = nextNode;
					}
					else {
						node = nIt->second;
					}
				}
			}
		}
	}

	static void appendSequence(TestNodeSPY* node, const sequence_in_t& seq) {
		for (auto input : seq) {
			state_t state = specification->getNextState(states[node->state], input);
			output_t outputState = (specification->isOutputState()) ? specification->getOutput(state, STOUT_INPUT) : DEFAULT_OUTPUT;
			output_t outputTransition = (specification->isOutputTransition()) ? specification->getOutput(states[node->state], input) : DEFAULT_OUTPUT;
			state = getIdx(states, state);
			TestNodeSPY* nextNode = new TestNodeSPY(state, outputState, outputTransition);
			node->next[input] = nextNode;
			if (node->isConfirmed) {
				if (coveredTransitions[node->state][input]) {
					nextNode->isConfirmed = true;
					confirmedNodes[state].push_back(nextNode);
				}
			}
			node = nextNode;
		}
		//printTStree(coreNodes[0]);
	}

	static void addSeparatingSequence(state_t state, const sequence_in_t& seq) {
		int maxPref = -1;
		TestNodeSPY* finNode;
		sequence_in_t finSeq;
		for (auto n : confirmedNodes[state]) {
			auto node = n;
			int len = 0;
			for (auto sIt = seq.begin(); sIt != seq.end(); sIt++, len++) {
				if (node->next.empty()) {// TS contains prefix of seq
					if (maxPref < len) {
						maxPref = len;
						finNode = node;
						finSeq.assign(sIt, seq.end());
					}
					break;
				}
				auto nextIt = node->next.find(*sIt);
				if (nextIt == node->next.end()) {// new test case would have to be added
					break;
				}
				node = nextIt->second;
			}
			if (len == seq.size()) {// separating sequence is already in TS
				return;
			}
		}
		if (maxPref == -1) {// new test needs to be added, so append the separating sequence to the core node
			auto node = confirmedNodes[state][0];
			for (auto sIt = seq.begin(); sIt != seq.end(); sIt++) {
				auto nextIt = node->next.find(*sIt);
				if (nextIt == node->next.end()) {
					finSeq.assign(sIt, seq.end());
					appendSequence(node, finSeq);
					break;
				}
				node = nextIt->second;
			}
		}
		else {// prefix of the sequence was found
			appendSequence(finNode, finSeq);
		}
	}

	static void closure(TestNodeSPY* node) {
		node->isConfirmed = true;
		confirmedNodes[node->state].push_back(node);
		for (auto nextIt : node->next) {
			if (coveredTransitions[node->state][nextIt.first]) {
				closure(nextIt.second);
			}
		}
	}

	static void closure(state_t state, input_t input) {
		for (int i = 0; i < confirmedNodes[state].size(); i++) {
			auto n = confirmedNodes[state][i];
			auto nextIt = n->next.find(input);
			if (nextIt != n->next.end()) {
				closure(nextIt->second);
			}
		}
	}

	static void getSequences(TestNodeSPY* node, sequence_set_t& TS) {
		stack< pair<TestNodeSPY*, sequence_in_t> > lifo;
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

	sequence_set_t SPY_method(const unique_ptr<DFSM>& fsm, int extraStates) {
		if (extraStates < 0) {
			return sequence_set_t();
		}
		specification = fsm.get();
		states = fsm->getStates();

		auto H = getHarmonizedStateIdentifiers(fsm);

		if (fsm->isOutputState()) {
			for (state_t i = 0; i < H.size(); i++) {
				sequence_set_t tmp;
				for (auto origDS : H[i]) {
					sequence_in_t seq;
					for (auto input : origDS) {
						if (input == STOUT_INPUT) continue;
						seq.push_back(input);
					}
					tmp.insert(seq);
				}
				H[i] = tmp;
			}
		}

		// initial partition
		confirmedNodes.resize(fsm->getNumberOfStates());
		coveredTransitions.resize(fsm->getNumberOfStates());
		for (state_t i = 0; i < fsm->getNumberOfStates(); i++) {
			confirmedNodes[i].clear();
			coveredTransitions[i].assign(fsm->getNumberOfInputs(), false);
		}

		createBasicTree(fsm, H);
		//printTStree(coreNodes[0]);

		queue<sequence_in_t> fifo;
		for (auto en : uncoveredTransitions) {
			state_t nextState = fsm->getNextState(states[en.first], en.second);
			state_t nextStateIdx = getIdx(states, nextState);
			sequence_in_t seq;
			fifo.push(seq);
			while (!fifo.empty()) {
				seq = fifo.front();
				fifo.pop();
				state_t finalState = fsm->getEndPathState(nextState, seq);
				if (finalState == WRONG_STATE) continue;
				finalState = getIdx(states, finalState);
				for (auto distSeq : H[finalState]) {
					sequence_in_t extSeq(seq);
					extSeq.push_front(en.second);
					extSeq.insert(extSeq.end(), distSeq.begin(), distSeq.end());
					addSeparatingSequence(en.first, extSeq);
					extSeq.pop_front();
					addSeparatingSequence(nextStateIdx, extSeq);
				}
				if (seq.size() < extraStates) {
					for (input_t input = 0; input < fsm->getNumberOfInputs(); input++) {
						sequence_in_t extSeq(seq);
						extSeq.push_back(input);
						fifo.push(extSeq);
					}
				}
			}
			coveredTransitions[en.first][en.second] = true;
			closure(en.first, en.second);
		}

		// obtain TS
		//printTStree(coreNodes[0]);
		sequence_set_t TS;
		getSequences(coreNodes[0], TS);

		cleanup();
		return TS;
	}
}
