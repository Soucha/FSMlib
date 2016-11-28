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

#include "FSMsequence.h"

namespace FSMsequence {
	struct st_node_t {// Splitting Tree node
		vector<state_t> block, nextStates;
		sequence_in_t sequence;
		vector<pair<output_t, shared_ptr<st_node_t>>> succ;
	};

	struct blockcomp {

		/**
		* compares two nodes of splitting tree firstly by their length,
		* larger first, then compares pointer address
		*/
		bool operator() (const shared_ptr<st_node_t> ln, const shared_ptr<st_node_t> rn) const {
			if (ln->block.size() != rn->block.size())
				return ln->block.size() < rn->block.size();
			return ln < rn;
		}
	};

	struct lencomp {

		/**
		* compares two pairs by sequence lenght saved in first element
		*/
		bool operator() (const pair<seq_len_t, state_t>& lp, const pair<seq_len_t, state_t>& rp) const {
			return lp.first < rp.first;
		}
	};

	sequence_vec_t getAdaptiveDistinguishingSet(const unique_ptr<DFSM>& fsm, const unique_ptr<AdaptiveDS>& ads) {
		RETURN_IF_UNREDUCED(fsm, "FSMsequence::getAdaptiveDistinguishingSet", sequence_vec_t());
		sequence_vec_t ADSet;
		if (!ads) return ADSet;
		stack<pair<AdaptiveDS*, sequence_in_t>> lifo;
		lifo.emplace(ads.get(), sequence_in_t());
		ADSet.resize(fsm->getNumberOfStates());
		while (!lifo.empty()) {
			auto p = move(lifo.top());
			lifo.pop();
			if (p.first->decision.empty()) {
				ADSet[p.first->initialStates.front()] = p.second;
			}
			else {
				p.second.insert(p.second.end(), p.first->input.begin(), p.first->input.end());
				for (auto it = p.first->decision.begin(); it != p.first->decision.end(); it++) {
					lifo.emplace(it->second.get(), p.second);
				}
			}
		}
		return ADSet;
	}

	sequence_vec_t getAdaptiveDistinguishingSet(const unique_ptr<DFSM>& fsm, bool omitUnnecessaryStoutInputs) {
		RETURN_IF_UNREDUCED(fsm, "FSMsequence::getAdaptiveDistinguishingSet", sequence_vec_t());
		auto ads = getAdaptiveDistinguishingSequence(fsm, omitUnnecessaryStoutInputs);
		return getAdaptiveDistinguishingSet(fsm, ads);
	}

	static void distinguishSTnode(const unique_ptr<DFSM>& fsm, const shared_ptr<st_node_t>& node) {
		for (input_t input = 0; input < fsm->getNumberOfInputs(); input++) {
			vector<set<state_t>> sameOutput;
			auto succCount = node->succ.size();
			auto invalidInput = false;
			// check input validity for all states in block
			for (const state_t& state : node->block) {
				// nextStates would contain two WRONG_STATE if there is WRONG_OUTPUT -> invalid input
				auto nextState = fsm->getNextState(state, input);
				auto output = fsm->getOutput(state, input);// possibly WRONG_OUTPUT or DEFAULT_OUTPUT
				bool found = false;
				for (auto i = succCount; i < node->succ.size(); i++) {
					if (node->succ[i].first == output) {
						if (!sameOutput[i - succCount].insert(nextState).second) {
							invalidInput = true;
							break;
						}
						node->succ[i].second->block.emplace_back(state);
						found = true;
						break;
					}
				}
				if (invalidInput) break;
				if (!found) {
					auto next = make_shared<st_node_t>();
					next->block.emplace_back(state);
					node->succ.emplace_back(output, move(next));
					sameOutput.emplace_back(set<state_t>({ nextState }));
				}
				// add the next state in the first successor
				node->succ[succCount].second->nextStates.emplace_back(nextState);
			}
			if (invalidInput) {
				while (node->succ.size() > succCount) node->succ.pop_back();
			}
			else if (succCount + 1 < node->succ.size()) {// distinguishing input
				node->nextStates = node->succ[succCount].second->nextStates;
				node->succ[succCount].second->nextStates.clear();
				node->sequence.clear();
				node->sequence.push_back(input);
				if (succCount != 0) {// shift successor
					output_t idx = 0;
					for (; succCount < node->succ.size(); succCount++, idx++) {
						node->succ[idx].swap(node->succ[succCount]);
					}
					while (node->succ.size() > idx) node->succ.pop_back();
				}
				break;
			}
			else {// valid input
				node->sequence.push_back(input);
			}
		}
	}

	static vector<vector<pair<input_t, state_t>>> prepareLinks(const vector<shared_ptr<st_node_t>>& dependent,
		priority_queue<pair<seq_len_t, state_t>, vector<pair<seq_len_t, state_t>>, lencomp>& bfsqueue,
		const vector<shared_ptr<st_node_t>>& curNode, const vector<shared_ptr<st_node_t>>& distinguished) {

		vector<vector<pair<input_t, state_t>>> link(dependent.size());
		
		// add link to dependent vector
		for (state_t dI = 0; dI < dependent.size(); dI++) {
			dependent[dI]->nextStates.emplace_back(dI);
		}
		// check dependent
		for (state_t dI = 0; dI < dependent.size(); dI++) {
			auto node = dependent[dI];
			auto seqInIt = node->sequence.begin();
			shared_ptr<st_node_t> bestNext;
			input_t bestInput = STOUT_INPUT;
			// check other valid input
			for (input_t i = 0; i < node->sequence.size(); i++, seqInIt++) {
				node->succ[i].first = *seqInIt; // for easier access to right input
				vector<state_t> diffStates;
				state_t pivot = node->succ[i].second->nextStates[0];
				for (const auto& stateI : node->succ[i].second->nextStates) {
					if (curNode[pivot] != curNode[stateI]) {
						bool inDiff = false;
						for (const auto& diffState : diffStates) {
							if (curNode[stateI] == curNode[diffState]) {
								inDiff = true;
								break;
							}
						}
						if (!inDiff) {
							diffStates.emplace_back(stateI);
						}
					}
				}
				if (diffStates.empty()) {// input to another dependent block
					if (curNode[pivot] != node) {
						link[curNode[pivot]->nextStates[0]].emplace_back(i, dI);
					}
				}
				else {// distinguishing input
					auto next = curNode[pivot];
					// find lowest node of ST with block of all diffStates and pivot
					for (const auto& diffState : diffStates) {
						auto idx = getStatePairIdx(pivot, diffState);
						if (next->block.size() < distinguished[idx]->block.size()) {
							next = distinguished[idx];
						}
					}
					if (!bestNext || (bestNext->sequence.size() > next->sequence.size())) {
						bestNext = next;
						bestInput = i;
					}
				}
			}
			if (bestNext) {
				bfsqueue.emplace(seq_len_t(bestNext->sequence.size()), dI);
				node->succ.emplace_back(bestInput, move(bestNext));
			}
		}
		return link;
	}

	static bool processDependent(const vector<shared_ptr<st_node_t>>& dependent,
		vector<vector<pair<input_t, state_t>>>& link,
		priority_queue<shared_ptr<st_node_t>, vector<shared_ptr<st_node_t>>, blockcomp>& partition,
		priority_queue<pair<seq_len_t, state_t>, vector<pair<seq_len_t, state_t>>, lencomp>& bfsqueue,
		vector<shared_ptr<st_node_t>>& curNode, vector<shared_ptr<st_node_t>>& distinguished, bool useStout) {
		
		auto distCounter = dependent.size();
		while (!bfsqueue.empty()) {
			auto dI = bfsqueue.top().second;
			auto node = dependent[dI];
			bfsqueue.pop();
			if (node->nextStates.size() == 1) {// undistinguished
				node->sequence.clear();
				// hack (input stored in place of output -> it does not have to be found in node->sequence by iteration first)
				node->sequence.emplace_back(node->succ[node->succ.back().first].first);
				auto next = node->succ.back().second;// best successor
				if (useStout && (next->sequence.front() != STOUT_INPUT)) node->sequence.push_back(STOUT_INPUT);
				node->sequence.insert(node->sequence.end(), next->sequence.begin(), next->sequence.end());
				// update next states after the valid input
				node->nextStates.swap(node->succ[node->succ.back().first].second->nextStates);
				// prepare succ
				node->succ.pop_back(); // link to next
				if (node->succ.size() > next->succ.size()) node->succ.resize(next->succ.size());
				for (output_t i = 0; i < next->succ.size(); i++) {
					if (i == node->succ.size()) {
						node->succ.emplace_back(next->succ[i].first, make_shared<st_node_t>());
					}
					else {
						node->succ[i].first = next->succ[i].first;
						node->succ[i].second->block.clear();
						node->succ[i].second->nextStates.clear();
					}
				}
				// distinguish block of states
				for (state_t sI = 0; sI < node->block.size(); sI++) {
					for (output_t i = 0; i < next->succ.size(); i++) {
						if (binary_search(next->succ[i].second->block.begin(),
								next->succ[i].second->block.end(), node->nextStates[sI])) {
							node->succ[i].second->block.push_back(node->block[sI]);
							// update next state after applying entire input sequence
							state_t idx;
							for (idx = 0; next->block[idx] != node->nextStates[sI]; idx++);
							node->nextStates[sI] = next->nextStates[idx];
							break;
						}
					}
				}
				// remove unused output
				for (long i = long(node->succ.size() - 1); i >= 0; i--) {
					if (node->succ[i].second->block.empty()) {
						node->succ[i].swap(node->succ.back());
						node->succ.pop_back();
					}
				}
				// update links
				for (output_t i = 0; i < node->succ.size(); i++) {
					next = node->succ[i].second;
					if (next->block.size() > 1) {// child is not singleton
						partition.emplace(next);
					}
					for (state_t stateI : next->block) {
						curNode[stateI] = next;
						for (output_t j = i + 1; j < node->succ.size(); j++) {
							for (state_t stateJ : node->succ[j].second->block) {
								auto idx = getStatePairIdx(stateI, stateJ);
								distinguished[idx] = node;// where two states were distinguished
							}
						}
					}
				}
				// count resolved dependent
				distCounter--;
				// push unresolved dependent to queue
				for (const auto& p : link[dI]) {
					next = dependent[p.second];
					if (next->nextStates.size() == 1) {// still unresolved
						if (next->sequence.size() == next->succ.size()) {// best not set
							next->succ.emplace_back(p.first, node);
							bfsqueue.emplace(seq_len_t(node->sequence.size()), next->nextStates[0]);
						}
						else if (next->succ.back().second->sequence.size() > node->sequence.size()) {// update best
							next->succ.back().first = p.first;
							next->succ.back().second = node;
							bfsqueue.emplace(seq_len_t(node->sequence.size()), next->nextStates[0]);
						}
					}
				}
				link[dI].clear();
			}
		}
		return (distCounter == 0);
	}

	static unique_ptr<AdaptiveDS> buildAds(const vector<state_t>& block, 
			const vector<shared_ptr<st_node_t>>& curNode, const vector<shared_ptr<st_node_t>>& distinguished, bool useStout) {
		auto outADS = make_unique<AdaptiveDS>();
		outADS->initialStates = outADS->currentStates = block;

		queue<AdaptiveDS*> fifo;
		fifo.push(outADS.get());
		while (!fifo.empty()) {
			auto adsNode = fifo.front();
			fifo.pop();
			if (adsNode->currentStates.size() == 1) continue;
			auto pivot = adsNode->currentStates[0];
			auto next = curNode[pivot];
			// find lowest node of ST with block of all currentStates
			for (state_t j = 1; j < adsNode->currentStates.size(); j++) {
				auto idx = getStatePairIdx(pivot, adsNode->currentStates[j]);
				if (next->block.size() < distinguished[idx]->block.size()) {
					next = distinguished[idx];
				}
			}
			// set distinguishing input sequence
			adsNode->input.insert(adsNode->input.end(), next->sequence.begin(), next->sequence.end());
			for (state_t sI = 0; sI < adsNode->initialStates.size(); sI++) {
				for (const auto &p : next->succ) {
					if (binary_search(p.second->block.begin(), p.second->block.end(), adsNode->currentStates[sI])) {
						state_t idx;
						for (idx = 0; next->block[idx] != adsNode->currentStates[sI]; idx++);
						auto outIt = adsNode->decision.find(p.first);
						if (outIt == adsNode->decision.end()) {
							auto adsNext = make_unique<AdaptiveDS>();
							if (useStout && (adsNode->input.back() != STOUT_INPUT)) adsNext->input.push_back(STOUT_INPUT);
							adsNext->initialStates.push_back(adsNode->initialStates[sI]);
							adsNext->currentStates.push_back(next->nextStates[idx]);
							fifo.push(adsNext.get());
							adsNode->decision[p.first] = move(adsNext);
						}
						else {
							outIt->second->initialStates.push_back(adsNode->initialStates[sI]);
							outIt->second->currentStates.push_back(next->nextStates[idx]);
						}
						break;
					}
				}
			}
		}
		return outADS;
	}

	unique_ptr<AdaptiveDS> getAdaptiveDistinguishingSequence(const unique_ptr<DFSM>& fsm, bool omitUnnecessaryStoutInputs) {
		RETURN_IF_UNREDUCED(fsm, "FSMsequence::getAdaptiveDistinguishingSequence", nullptr);
		state_t N = fsm->getNumberOfStates();
		priority_queue<shared_ptr<st_node_t>, vector<shared_ptr<st_node_t>>, blockcomp> partition;
		auto rootST = make_shared<st_node_t>();
		vector<shared_ptr<st_node_t>> curNode(N, rootST);
		vector<shared_ptr<st_node_t>> distinguished(((N - 1) * N) / 2, nullptr);
		vector<shared_ptr<st_node_t>> dependent;
		bool useStout = !omitUnnecessaryStoutInputs && fsm->isOutputState();
		if (fsm->isOutputState()) {
			auto node = rootST;
			node->sequence.push_back(STOUT_INPUT);
			for (state_t state = 0; state < N; state++) {
				node->block.push_back(state);
				node->nextStates.push_back(state);
				bool found = false;
				auto output = fsm->getOutput(state, STOUT_INPUT);
				for (const auto &p : node->succ) {
					if (p.first == output) {
						p.second->block.emplace_back(state);
						found = true;
						break;
					}
				}
				if (!found) {
					auto next = make_shared<st_node_t>();
					next->block.emplace_back(state);
					node->succ.emplace_back(output, move(next));
				}
			}
			for (output_t i = 0; i < node->succ.size(); i++) {
				auto &next = node->succ[i].second;
				if (next->block.size() > 1) {// child is not singleton
					partition.emplace(next);
				}
				for (state_t stateI : next->block) {
					curNode[stateI] = next;
					for (output_t j = i + 1; j < node->succ.size(); j++) {
						for (state_t stateJ : node->succ[j].second->block) {
							auto idx = getStatePairIdx(stateI, stateJ);
							distinguished[idx] = node;// where two states were distinguished
						}
					}
				}
			}
		}
		else {
			for (state_t state = 0; state < N; state++) {
				rootST->block.emplace_back(state);
			}
			partition.emplace(rootST);
		}
		while (!partition.empty()) {
			auto node = partition.top();
			partition.pop();
			// find distinguishing input or sort out other valid inputs
			distinguishSTnode(fsm, node);
			if (node->succ.empty()) {// no valid input - possible only by root
				return nullptr;
			}
			if (!node->nextStates.empty()) {// block is distinguished by an input
				for (output_t i = 0; i < node->succ.size(); i++) {
					auto & next = node->succ[i].second;
					if (next->block.size() > 1) {// child is not singleton
						partition.emplace(next);
					}
					for (state_t stateI : next->block) {
						curNode[stateI] = next;
						for (output_t j = i + 1; j < node->succ.size(); j++) {
							for (state_t stateJ : node->succ[j].second->block) {
								auto idx = getStatePairIdx(stateI, stateJ);
								distinguished[idx] = node;// where two states were distinguished
							}
						}
					}
				}
			}
			else {
				dependent.emplace_back(node);
			}
			// are all blocks with max cardinality distinguished by one input?
			if (!dependent.empty() && (partition.empty() || (partition.top()->block.size() != node->block.size()))) {				
				priority_queue<pair<seq_len_t, state_t>, vector<pair<seq_len_t, state_t>>, lencomp> bfsqueue;
				auto link = prepareLinks(dependent, bfsqueue, curNode, distinguished);
				// check that all dependent was divided
				if (!processDependent(dependent, link, partition, bfsqueue, curNode, distinguished, useStout)) {
					return nullptr;
				}
				dependent.clear();
			}
		}
		// build ADS from ST
		return buildAds(rootST->block, curNode, distinguished, useStout);
	}
}
