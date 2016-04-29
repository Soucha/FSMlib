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
		vector< pair<output_t, st_node_t* > > succ;

		~st_node_t() {
			for (auto succPair : succ) {
				delete succPair.second;
			}
		}
	};

	struct blockcomp {

		/**
		* compares two nodes of splitting tree firstly by their length,
		* larger first, then compares pointer address
		*/
		bool operator() (const st_node_t* ln, const st_node_t* rn) const {
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

	sequence_vec_t getAdaptiveDistinguishingSet(DFSM * fsm, const unique_ptr<AdaptiveDS>& ads) {
		sequence_vec_t ADSet;
		if (!ads) return ADSet;
		stack< pair<AdaptiveDS*, sequence_in_t> > lifo;
		sequence_in_t seq;
		lifo.push(make_pair(ads.get(), seq));
		ADSet.resize(fsm->getNumberOfStates());
		auto states = fsm->getStates();
		while (!lifo.empty()) {
			auto p = lifo.top();
			lifo.pop();
			seq = p.second;
			seq.insert(seq.end(), p.first->input.begin(), p.first->input.end());
			for (auto it = p.first->decision.begin(); it != p.first->decision.end(); it++) {
				lifo.push(make_pair(it->second.get(), seq));
			}
			if (p.first->decision.empty()) {
				ADSet[getIdx(states, p.first->initialStates.front())] = p.second;
			}
		}
		return ADSet;
	}

	sequence_vec_t getAdaptiveDistinguishingSet(DFSM * fsm) {
		auto ads = getAdaptiveDistinguishingSequence(fsm);
		return getAdaptiveDistinguishingSet(fsm, ads);
	}

	unique_ptr<AdaptiveDS> getAdaptiveDistinguishingSequence(DFSM * fsm) {
		state_t N = fsm->getGreatestStateId(), idx;
		priority_queue<st_node_t*, vector<st_node_t*>, blockcomp> partition;
		priority_queue<pair<seq_len_t, state_t>, vector<pair<seq_len_t, state_t> >, lencomp> bfsqueue;
		st_node_t* rootST = new st_node_t, *node, *next, *bestNext;
		vector<st_node_t*> curNode(N, rootST), distinguished(((N - 1) * N) / 2), dependent;
		set<state_t> used;
		vector< set<state_t> > otherUsed;
		output_t output, outputSecond, outCounter;
		if (fsm->isOutputState()) {
			node = rootST;
			node->sequence.push_back(STOUT_INPUT);
			next = new st_node_t;
			output = fsm->getOutput(0, STOUT_INPUT);
			node->succ.push_back(make_pair(output, next));
			for (state_t state : fsm->getStates()) {
				node->block.push_back(state);
				node->nextStates.push_back(state);
				outputSecond = fsm->getOutput(state, STOUT_INPUT);
				if (output != outputSecond) {
					output_t i = 1;
					for (; (i < node->succ.size()) && (outputSecond != node->succ[i].first); i++);
					if (i == node->succ.size()) {
						st_node_t* newNode = new st_node_t;
						newNode->block.push_back(state);
						node->succ.push_back(make_pair(outputSecond, newNode));
					}
					else {
						node->succ[i].second->block.push_back(state);
					}
				}
				else {
					next->block.push_back(state);
				}
			}
			for (output_t i = 0; i < node->succ.size(); i++) {
				next = node->succ[i].second;
				if (next->block.size() > 1) {// child is not singleton
					partition.push(next);
				}
				for (state_t sI = 0; sI < next->block.size(); sI++) {
					state_t stateI = next->block[sI];
					curNode[stateI] = next;
					for (output_t j = i + 1; j < node->succ.size(); j++) {
						for (state_t sJ = 0; sJ < node->succ[j].second->block.size(); sJ++) {
							state_t stateJ = node->succ[j].second->block[sJ];
							// 2D -> 1D index
							idx = (stateI < stateJ) ?
								(stateI * N + stateJ - 1 - (stateI * (stateI + 3)) / 2) :
								(stateJ * N + stateI - 1 - (stateJ * (stateJ + 3)) / 2);
							distinguished[idx] = node;// where two states were distinguished
						}
					}
				}
			}
		}
		else {
			for (state_t i : fsm->getStates()) {
				rootST->block.push_back(i);
			}
			partition.push(rootST);
		}
		while (!partition.empty()) {
			node = partition.top();
			partition.pop();
			outCounter = 0;
			// find distinguishing input or sort out other valid inputs
			for (input_t input = 0; input < fsm->getNumberOfInputs(); input++) {
				next = new st_node_t;
				output = fsm->getOutput(node->block[0], input);// possibly WRONG_OUTPUT or DEFAULT_OUTPUT
				node->succ.push_back(make_pair(output, next));
				used.clear();
				otherUsed.clear();
				// check input validity for all states in block
				for (state_t state = 0; state < node->block.size(); state++) {
					next->nextStates.push_back(fsm->getNextState(node->block[state], input));
					outputSecond = fsm->getOutput(node->block[state], input);// possibly WRONG_OUTPUT or DEFAULT_OUTPUT
					// nextStates would contain two WRONG_STATE if there is WRONG_OUTPUT -> invalid input
					if (output != outputSecond) {
						output_t i = outCounter + 1;
						for (; (i < node->succ.size()) && (outputSecond != node->succ[i].first); i++);
						if (i == node->succ.size()) {
							st_node_t* newNode = new st_node_t;
							newNode->block.push_back(node->block[state]);
							node->succ.push_back(make_pair(outputSecond, newNode));
							set<state_t> tmp;
							tmp.insert(next->nextStates.back());
							otherUsed.push_back(tmp);
						}
						else if (!otherUsed[i - outCounter - 1].insert(next->nextStates.back()).second) {// invalid input
							used.clear();
							break;
						}
						else {
							node->succ[i].second->block.push_back(node->block[state]);
						}
					}
					else if (!used.insert(next->nextStates.back()).second) {// invalid input
						used.clear();
						break;
					}
					else {
						next->block.push_back(node->block[state]);
					}
				}
				if (!used.empty()) {// valid input
					if (outCounter + 1 < node->succ.size()) {// distinguishing input
						node->nextStates = next->nextStates;
						next->nextStates.clear();
						output_t size = min(outCounter, output_t(node->succ.size() - outCounter));
						for (output_t i = 0; i < size; i++) {
							delete node->succ[i].second;
							node->succ[i] = node->succ.back();
							node->succ.pop_back();
						}
						if (size != outCounter) {
							for (long i = long(node->succ.size() - 1); i >= long(size); i--) {
								delete node->succ[i].second;
								node->succ.pop_back();
							}
						}
						node->sequence.clear();
						node->sequence.push_back(input);
						break;
					}
					else {
						node->sequence.push_back(input);
						outCounter++;
					}
				}
				else {// invalid input -> delete allocated space
					for (long i = long(node->succ.size() - 1); i >= long(outCounter); i--) {
						delete node->succ[i].second;
						node->succ.pop_back();
					}
				}
			}
			if (node->succ.empty()) {// no valid input - possible only by root
				delete rootST;
				return nullptr;
			}
			if (!node->nextStates.empty()) {// block is distinguished by input
				for (output_t i = 0; i < node->succ.size(); i++) {
					next = node->succ[i].second;
					if (next->block.size() > 1) {// child is not singleton
						partition.push(next);
					}
					for (state_t sI = 0; sI < next->block.size(); sI++) {
						state_t stateI = next->block[sI];
						curNode[stateI] = next;
						for (output_t j = i + 1; j < node->succ.size(); j++) {
							for (state_t sJ = 0; sJ < node->succ[j].second->block.size(); sJ++) {
								state_t stateJ = node->succ[j].second->block[sJ];
								idx = (stateI < stateJ) ?
									(stateI * N + stateJ - 1 - (stateI * (stateI + 3)) / 2) :
									(stateJ * N + stateI - 1 - (stateJ * (stateJ + 3)) / 2);
								distinguished[idx] = node;
							}
						}
					}
				}
			}
			else {
				dependent.push_back(node);
			}
			// are all blocks with max cardinality distinguished by one input?
			if (!dependent.empty() && (partition.empty() || (partition.top()->block.size() != node->block.size()))) {
				vector< vector<pair<input_t, state_t> > > link(dependent.size());
				vector<state_t> diffStates;
				sequence_in_t::iterator seqInIt;
				// add link to dependent vector
				for (state_t dI = 0; dI < dependent.size(); dI++) {
					dependent[dI]->nextStates.push_back(dI);
				}
				// check dependent
				for (state_t dI = 0; dI < dependent.size(); dI++) {
					node = dependent[dI];
					seqInIt = node->sequence.begin();
					bestNext = NULL;
					input_t bestInput = STOUT_INPUT;
					// check other valid input
					for (input_t i = 0; i < node->sequence.size(); i++, seqInIt++) {
						node->succ[i].first = *seqInIt; // for easier access to right input
						diffStates.clear();
						state_t pivot = node->succ[i].second->nextStates[0], stateI;
						for (state_t sI = 1; sI < node->block.size(); sI++) {
							stateI = node->succ[i].second->nextStates[sI];
							if (curNode[pivot] != curNode[stateI]) {
								state_t j;
								for (j = 0; j < diffStates.size(); j++) {
									if (curNode[stateI] == curNode[diffStates[j]])
										break;
								}
								if (j == diffStates.size()) {
									diffStates.push_back(stateI);
								}
							}
						}
						if (diffStates.empty()) {// input to another dependent block
							if (curNode[pivot] != node) {
								link[curNode[pivot]->nextStates[0]].push_back(make_pair(i, dI));
							}
						}
						else {// distinguishing input
							next = curNode[pivot];
							// find lowest node of ST with block of all diffStates and pivot
							for (state_t j = 0; j < diffStates.size(); j++) {
								idx = (pivot < diffStates[j]) ?
									(pivot * N + diffStates[j] - 1 - (pivot * (pivot + 3)) / 2) :
									(diffStates[j] * N + pivot - 1 - (diffStates[j] * (diffStates[j] + 3)) / 2);
								if (next->block.size() < distinguished[idx]->block.size()) {
									next = distinguished[idx];
								}
							}
							if (bestNext == NULL || bestNext->sequence.size() > next->sequence.size()) {
								bestNext = next;
								bestInput = i;
							}
						}
					}
					if (bestNext != NULL) {
						bfsqueue.push(make_pair(seq_len_t(bestNext->sequence.size()), dI));
						node->succ.push_back(make_pair(bestInput, bestNext));
					}
				}
				state_t distCounter = state_t(dependent.size());
				while (!bfsqueue.empty()) {
					state_t dI = bfsqueue.top().second;
					node = dependent[dI];
					bfsqueue.pop();
					if (node->nextStates.size() == 1) {
						node->sequence.clear();
						node->sequence.push_back(node->succ[node->succ.back().first].first);
						next = node->succ.back().second;
						node->sequence.insert(node->sequence.end(),
							next->sequence.begin(), next->sequence.end());
						// update next states after one valid input
						node->nextStates = node->succ[node->succ.back().first].second->nextStates;
						// prepare succ
						node->succ.pop_back(); // link to next
						for (long i = long(node->succ.size() - 1); i >= long(next->succ.size()); i--) {
							delete node->succ[i].second;
							node->succ.pop_back();
						}
						for (output_t i = 0; i < next->succ.size(); i++) {
							if (i == node->succ.size()) {
								st_node_t* newNode = new st_node_t;
								node->succ.push_back(make_pair(next->succ[i].first, newNode));
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
									for (idx = 0; next->block[idx] != node->nextStates[sI]; idx++);
									node->nextStates[sI] = next->nextStates[idx];
									break;
								}
							}
						}
						// remove unused output
						for (long i = long(node->succ.size() - 1); i >= 0; i--) {
							if (node->succ[i].second->block.empty()) {
								delete node->succ[i].second;
								node->succ[i] = node->succ.back();
								node->succ.pop_back();
							}
						}
						// update links
						for (output_t i = 0; i < node->succ.size(); i++) {
							next = node->succ[i].second;
							if (next->block.size() > 1) {// child is not singleton
								partition.push(next);
							}
							for (state_t sI = 0; sI < next->block.size(); sI++) {
								state_t stateI = next->block[sI];
								curNode[stateI] = next;
								for (output_t j = i + 1; j < node->succ.size(); j++) {
									for (state_t sJ = 0; sJ < node->succ[j].second->block.size(); sJ++) {
										state_t stateJ = node->succ[j].second->block[sJ];
										idx = (stateI < stateJ) ?
											(stateI * N + stateJ - 1 - (stateI * (stateI + 3)) / 2) :
											(stateJ * N + stateI - 1 - (stateJ * (stateJ + 3)) / 2);
										distinguished[idx] = node;
									}
								}
							}
						}
						// count resolved dependent
						distCounter--;
						// push unresolved dependent to queue
						for (state_t i = 0; i < link[dI].size(); i++) {
							next = dependent[link[dI][i].second];
							if (next->nextStates.size() == 1) {// still unresolved
								if (next->sequence.size() == next->succ.size()) {
									next->succ.push_back(make_pair(link[dI][i].first, node));
									bfsqueue.push(make_pair(seq_len_t(node->sequence.size()), next->nextStates[0]));
								}
								else if (next->succ.back().second->sequence.size() >
									node->sequence.size()) {
									next->succ.back().first = link[dI][i].first;
									next->succ.back().second = node;
									bfsqueue.push(make_pair(seq_len_t(node->sequence.size()), next->nextStates[0]));
								}
							}
						}
						link[dI].clear();
					}
				}
				// check that all dependent was divided
				if (distCounter != 0) {
					delete rootST;
					return nullptr;
				}
				dependent.clear();
			}
		}
		// build ADS from ST
		auto outADS = unique_ptr<AdaptiveDS>(new AdaptiveDS);
		outADS->initialStates = outADS->currentStates = rootST->block;

		queue<AdaptiveDS*> fifo;
		AdaptiveDS* adsNode;
		map<output_t, unique_ptr<AdaptiveDS>>::iterator outIt;
		state_t pivot;
		fifo.push(outADS.get());
		while (!fifo.empty()) {
			adsNode = fifo.front();
			fifo.pop();
			if (adsNode->currentStates.size() == 1) continue;
			pivot = adsNode->currentStates[0];
			next = curNode[pivot];
			// find lowest node of ST with block of all currentStates
			for (state_t j = 1; j < adsNode->currentStates.size(); j++) {
				idx = (pivot < adsNode->currentStates[j]) ?
					(pivot * N + adsNode->currentStates[j] - 1 - (pivot * (pivot + 3)) / 2) :
					(adsNode->currentStates[j] * N + pivot - 1 -
					(adsNode->currentStates[j] * (adsNode->currentStates[j] + 3)) / 2);
				if (next->block.size() < distinguished[idx]->block.size()) {
					next = distinguished[idx];
				}
			}
			// set distinguishing input sequence
			adsNode->input = next->sequence;
			for (state_t sI = 0; sI < adsNode->initialStates.size(); sI++) {
				for (output_t i = 0; i < next->succ.size(); i++) {
					if (binary_search(next->succ[i].second->block.begin(),
						next->succ[i].second->block.end(), adsNode->currentStates[sI])) {
						if ((outIt = adsNode->decision.find(next->succ[i].first)) == adsNode->decision.end()) {
							auto adsNext = unique_ptr<AdaptiveDS>(new AdaptiveDS);
							adsNext->initialStates.push_back(adsNode->initialStates[sI]);
							for (idx = 0; next->block[idx] != adsNode->currentStates[sI]; idx++);
							adsNext->currentStates.push_back(next->nextStates[idx]);
							fifo.push(adsNext.get()); 
							adsNode->decision[next->succ[i].first] = move(adsNext);
						}
						else {
							outIt->second->initialStates.push_back(adsNode->initialStates[sI]);
							for (idx = 0; next->block[idx] != adsNode->currentStates[sI]; idx++);
							outIt->second->currentStates.push_back(next->nextStates[idx]);
						}
						break;
					}
				}
			}
		}
		delete rootST;
		return outADS;
	}
}
