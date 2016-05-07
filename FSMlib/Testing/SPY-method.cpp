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
		map<input_t, shared_ptr<TestNodeSPY>> next;

		TestNodeSPY(state_t state, output_t stateOutput, output_t inOutput) :
			incomingOutput(inOutput), stateOutput(stateOutput), state(state), isConfirmed(false) {
		}
	};

	static void printTStree(const shared_ptr<TestNodeSPY>& node, string prefix = "") {
		printf("%s%d/%d <- %d (conf%d)\n", prefix.c_str(), node->state, node->stateOutput, node->incomingOutput, int(node->isConfirmed));
		for (auto it : node->next) {
			printf("%s - %d ->\n", prefix.c_str(), it.first);
			printTStree(it.second, prefix + "\t");
		}
	}

	static shared_ptr<TestNodeSPY> createBasicTree(const unique_ptr<DFSM>& fsm, const vector<sequence_set_t>& H,
			vector<pair<state_t, input_t>>& uncoveredTransitions,
			vector<vector<bool>>& coveredTransitions, vector<vector<shared_ptr<TestNodeSPY>>>& confirmedNodes) {
		output_t outputState, outputTransition;
		vector<bool> covered(fsm->getNumberOfStates(), false);
		vector<shared_ptr<TestNodeSPY>> coreNodes; // stores SC

		// root
		outputState = (fsm->isOutputState()) ? fsm->getOutput(0, STOUT_INPUT) : DEFAULT_OUTPUT;
		coreNodes.emplace_back(make_shared<TestNodeSPY>(0, outputState, DEFAULT_OUTPUT));
		covered[0] = true;
		for (state_t idx = 0; idx != coreNodes.size(); idx++) {
			coreNodes[idx]->isConfirmed = true;
			confirmedNodes[coreNodes[idx]->state].emplace_back(coreNodes[idx]);
			for (input_t input = 0; input < fsm->getNumberOfInputs(); input++) {
				auto state = fsm->getNextState(coreNodes[idx]->state, input);
				if (state == NULL_STATE) continue;
				if (covered[state]) {
					uncoveredTransitions.emplace_back(coreNodes[idx]->state, input);
				}
				else {
					coveredTransitions[coreNodes[idx]->state][input] = true;
					outputState = (fsm->isOutputState()) ? fsm->getOutput(state, STOUT_INPUT) : DEFAULT_OUTPUT;
					outputTransition = (fsm->isOutputTransition()) ? fsm->getOutput(coreNodes[idx]->state, input) : DEFAULT_OUTPUT;
					auto node = make_shared<TestNodeSPY>(state, outputState, outputTransition);
					coreNodes[idx]->next[input] = node;
					coreNodes.emplace_back(move(node));
					covered[state] = true;
				}
			}
		}
		for (auto node : coreNodes) {
			for (const auto& seq : H[node->state]) {
				for (const auto& input : seq) {
					auto nIt = node->next.find(input);
					if (nIt == node->next.end()) {
						auto state = fsm->getNextState(node->state, input);
						outputState = (fsm->isOutputState()) ? fsm->getOutput(state, STOUT_INPUT) : DEFAULT_OUTPUT;
						outputTransition = (fsm->isOutputTransition()) ? fsm->getOutput(node->state, input) : DEFAULT_OUTPUT;
						auto nextNode = make_shared<TestNodeSPY>(state, outputState, outputTransition);
						node->next[input] = nextNode;
						node = move(nextNode);
					}
					else {
						node = nIt->second;
					}
				}
			}
		}
		return coreNodes[0];
	}

	static void appendSequence(shared_ptr<TestNodeSPY> node, const sequence_in_t& seq, const unique_ptr<DFSM>& fsm,
			vector<vector<bool>>& coveredTransitions, vector<vector<shared_ptr<TestNodeSPY>>>& confirmedNodes) {
		for (const auto& input : seq) {
			state_t state = fsm->getNextState(node->state, input);
			output_t outputState = (fsm->isOutputState()) ? fsm->getOutput(state, STOUT_INPUT) : DEFAULT_OUTPUT;
			output_t outputTransition = (fsm->isOutputTransition()) ? fsm->getOutput(node->state, input) : DEFAULT_OUTPUT;
			auto nextNode = make_shared<TestNodeSPY>(state, outputState, outputTransition);
			node->next[input] = nextNode;
			if (node->isConfirmed) {
				if (coveredTransitions[node->state][input]) {
					nextNode->isConfirmed = true;
					confirmedNodes[state].emplace_back(nextNode);
				}
			}
			node = move(nextNode);
		}
		//printTStree(coreNodes[0]);
	}

	static void addSeparatingSequence(state_t state, const sequence_in_t& seq, const unique_ptr<DFSM>& fsm,
			vector<vector<bool>>& coveredTransitions, vector<vector<shared_ptr<TestNodeSPY>>>& confirmedNodes) {
		int maxPref = -1;
		shared_ptr<TestNodeSPY> finNode;
		sequence_in_t finSeq;
		for (auto node : confirmedNodes[state]) {
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
					appendSequence(node, move(finSeq), fsm, coveredTransitions, confirmedNodes);
					break;
				}
				node = nextIt->second;
			}
		}
		else {// prefix of the sequence was found
			appendSequence(move(finNode), move(finSeq), fsm, coveredTransitions, confirmedNodes);
		}
	}

	static void closure(const shared_ptr<TestNodeSPY>& node, 
			vector<vector<bool>>& coveredTransitions, vector<vector<shared_ptr<TestNodeSPY>>>& confirmedNodes) {
		node->isConfirmed = true;
		confirmedNodes[node->state].emplace_back(node);
		for (const auto& nextIt : node->next) {
			if (coveredTransitions[node->state][nextIt.first]) {
				closure(nextIt.second, coveredTransitions, confirmedNodes);
			}
		}
	}

	static void closure(state_t state, input_t input,
			vector<vector<bool>>& coveredTransitions, vector<vector<shared_ptr<TestNodeSPY>>>& confirmedNodes) {
		for (int i = 0; i < confirmedNodes[state].size(); i++) {// confirmedNodes[state] can be changed in the inner closure function
			auto& n = confirmedNodes[state][i];
			auto nextIt = n->next.find(input);
			if (nextIt != n->next.end()) {
				closure(nextIt->second, coveredTransitions, confirmedNodes);
			}
		}
	}

	static sequence_set_t getSequences(const shared_ptr<TestNodeSPY>& node, const unique_ptr<DFSM>& fsm) {
		sequence_set_t TS;
		stack<pair<shared_ptr<TestNodeSPY>, sequence_in_t>> lifo;
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

	sequence_set_t SPY_method(const unique_ptr<DFSM>& fsm, int extraStates) {
		RETURN_IF_NONCOMPACT(fsm, "FSMtesting::SPY_method", sequence_set_t());
		if (extraStates < 0) {
			return sequence_set_t();
		}
				
		auto H = getHarmonizedStateIdentifiers(fsm);

		if (fsm->isOutputState()) {
			for (state_t i = 0; i < H.size(); i++) {
				sequence_set_t tmp;
				for (const auto& origDS : H[i]) {
					sequence_in_t seq;
					for (auto& input : origDS) {
						if (input == STOUT_INPUT) continue;
						seq.push_back(input);
					}
					tmp.emplace(move(seq));
				}
				H[i].swap(tmp);
			}
		}
		vector<pair<state_t, input_t>> uncoveredTransitions;
		vector<vector<bool>> coveredTransitions;
		vector<vector<shared_ptr<TestNodeSPY>>> confirmedNodes;
		confirmedNodes.resize(fsm->getNumberOfStates());
		coveredTransitions.resize(fsm->getNumberOfStates());
		for (state_t i = 0; i < fsm->getNumberOfStates(); i++) {
			coveredTransitions[i].resize(fsm->getNumberOfInputs(), false);
		}
		
		auto root = createBasicTree(fsm, H, uncoveredTransitions, coveredTransitions, confirmedNodes);
		//printTStree(root);

		queue<sequence_in_t> fifo;
		for (const auto& en : uncoveredTransitions) {
			auto nextState = fsm->getNextState(en.first, en.second);
			fifo.emplace(sequence_in_t());
			while (!fifo.empty()) {
				auto seq = move(fifo.front());
				fifo.pop();
				auto finalState = fsm->getEndPathState(nextState, seq);
				if (finalState == WRONG_STATE) continue;
				for (const auto& distSeq : H[finalState]) {
					sequence_in_t extSeq(seq);
					extSeq.push_front(en.second);
					extSeq.insert(extSeq.end(), distSeq.begin(), distSeq.end());
					addSeparatingSequence(en.first, extSeq, fsm, coveredTransitions, confirmedNodes);
					extSeq.pop_front();
					addSeparatingSequence(nextState, extSeq, fsm, coveredTransitions, confirmedNodes);
				}
				if (seq.size() < extraStates) {
					for (input_t input = 0; input < fsm->getNumberOfInputs(); input++) {
						sequence_in_t extSeq(seq);
						extSeq.push_back(input);
						fifo.emplace(move(extSeq));
					}
				}
			}
			coveredTransitions[en.first][en.second] = true;
			closure(en.first, en.second, coveredTransitions, confirmedNodes);
		}

		// obtain TS
		//printTStree(root);
		return getSequences(root, fsm);
	}
}
