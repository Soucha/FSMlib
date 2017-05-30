/* Copyright (c) Michal Soucha, 2017
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
	struct blockcomp {

		/**
		* compares two nodes of splitting tree firstly by their length,
		* larger first, then compares pointer address
		*/
		bool operator() (const shared_ptr<st_node_t>& ln, const shared_ptr<st_node_t>& rn) const {
			if (ln->block.size() != rn->block.size())
				return ln->block.size() < rn->block.size();
			return ln->block < rn->block;
		}
	};

	struct lencomp {

		/**
		* compares two pairs by sequence lenght saved in first element
		*/
		bool operator() (const pair<seq_len_t, state_t>& lp, const pair<seq_len_t, state_t>& rp) const {
			return lp.first > rp.first;
		}
	};

	struct dependent_info_t {
		size_t distCounter, initDepSize;
		vector<shared_ptr<st_node_t>> dependent;
		vector<vector<pair<input_t, state_t>>> link;
		set<shared_ptr<st_node_t>, blockcomp> extraDependent;
		priority_queue<pair<seq_len_t, state_t>, vector<pair<seq_len_t, state_t>>, lencomp> bfsqueue;
	};

	static void distinguishSTnode(const unique_ptr<DFSM>& fsm, const shared_ptr<st_node_t>& node, bool allowInvalidInputs) {
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
							if (allowInvalidInputs) {
								node->succ[i].second->undistinguishedStates++;
							}
							else {
								break;
							}
						}
						node->succ[i].second->block.emplace_back(state);
						found = true;
						break;
					}
				}
				if (invalidInput && !allowInvalidInputs) break;
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
				if (allowInvalidInputs) {
					auto& refSucc = node->succ[succCount].second;
					auto next = make_shared<st_node_t>();
					next->block.swap(refSucc->block);
					next->undistinguishedStates = refSucc->undistinguishedStates;
					refSucc->succ.emplace_back(node->succ[succCount].first, move(next));
					for (auto i = succCount + 1; i < node->succ.size(); i++) {
						refSucc->undistinguishedStates += node->succ[i].second->undistinguishedStates;
						refSucc->succ.emplace_back(move(node->succ[i]));
					}
					while (node->succ.size() > succCount + 1) node->succ.pop_back();
					node->sequence.push_back(input);
				}
				else {
					while (node->succ.size() > succCount) node->succ.pop_back();
				}
			}
			else if (succCount + 1 < node->succ.size()) {// distinguishing input
				node->nextStates.clear();
				node->nextStates.swap(node->succ[succCount].second->nextStates);
				node->sequence.clear();
				node->sequence.push_back(input);
				if (succCount != 0) {// shift successors
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

	static size_t getInvalidScore(const shared_ptr<st_node_t>& node, input_t input, const shared_ptr<st_node_t>& next) {
		size_t B = node->block.size();
		size_t invalidScore(B * B);
		state_t undistinguished(0), succCount(0);
		auto& nextStates = node->succ[input].second->nextStates;
		for (const auto& p : next->succ) {
			set<state_t> diffStates;
			auto currUndist = undistinguished;
			for (const auto& nState : p.second->block) {
				if (find(nextStates.begin(), nextStates.end(), nState) != nextStates.end()) {
					auto it = lower_bound(next->block.begin(), next->block.end(), nState);
					auto nIt = next->nextStates.begin();
					advance(nIt, distance(next->block.begin(), it));
					if (!diffStates.insert(*nIt).second) {
						undistinguished++;
					}
				}
			}
			if (!diffStates.empty()) {
				succCount++;
			}
			if (currUndist == undistinguished) {
				invalidScore -= (diffStates.size() * B + 1);
			}
		}
		invalidScore *= B;
		invalidScore -= succCount;
		invalidScore *= B;
		invalidScore += undistinguished + node->succ[input].second->undistinguishedStates;
		invalidScore *= B;
		invalidScore += next->sequence.size() + 1;// +((incomingUndistinguished > 0) ? 1 : 0);
		return invalidScore;
	}

	static void prepareLinks(const unique_ptr<DFSM>& fsm, dependent_info_t& depInfo, const unique_ptr<SplittingTree>& st) {
		depInfo.link.clear();
		depInfo.link.resize(depInfo.dependent.size());

		// add link to dependent vector
		for (state_t dI = 0; dI < depInfo.dependent.size(); dI++) {
			depInfo.dependent[dI]->nextStates.emplace_back(dI);
		}
		// check dependent
		for (state_t dI = 0; dI < depInfo.dependent.size(); dI++) {
			auto node = depInfo.dependent[dI];
			if (node->nextStates.size() > 1) continue;// added block with a valid input
			auto seqInIt = node->sequence.begin();
			shared_ptr<st_node_t> bestNext;
			input_t bestInput(STOUT_INPUT);
			auto depSize = depInfo.dependent.size();
			// check valid transfering inputs
			for (input_t i = 0; i < node->sequence.size(); i++, seqInIt++) {
				node->succ[i].first = *seqInIt; // for easier access to right input
				if (node->succ[i].second->undistinguishedStates == 0) {// valid input
					list<state_t> diffStates;
					state_t pivot = node->succ[i].second->nextStates[0];
					for (const auto& stateI : node->succ[i].second->nextStates) {
						if (st->curNode[pivot] != st->curNode[stateI]) {
							bool inDiff = false;
							for (const auto& diffState : diffStates) {
								if (st->curNode[stateI] == st->curNode[diffState]) {
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
						if (st->curNode[pivot] != node) {
							// st->curNode[pivot]->nextStates[0] is index to dependent vector of node st->curNode[pivot]
							depInfo.link[st->curNode[pivot]->nextStates[0]].emplace_back(i, dI);
						}
					}
					else {// distinguishing input
						auto next = st->curNode[pivot];
						// find the lowest node of ST with block of all diffStates and pivot
						for (const auto& diffState : diffStates) {
							auto idx = getStatePairIdx(pivot, diffState);
							if (next->block.size() < st->distinguished[idx]->block.size()) {
								next = st->distinguished[idx];
							}
						}
						if ((next->undistinguishedStates > 0) // next has an invalid sequence
							&& (!bestNext || (bestNext->undistinguishedStates > 0)) // bestNext as well
							&& (next->block.size() > node->block.size())) {
							// next has an invalid input -> check for a better input fot current nextStates
							auto nextStatesSorted = node->succ[i].second->nextStates;
							sort(nextStatesSorted.begin(), nextStatesSorted.end());
							auto nextStateNode = make_shared<st_node_t>();
							nextStateNode->block = move(nextStatesSorted);
							auto itED = depInfo.extraDependent.find(nextStateNode);
							if (itED != depInfo.extraDependent.end()) {
								if ((*itED)->nextStates.size() == 1) {// undistinguished
									auto& idx = (*itED)->nextStates[0];
									if ((idx >= depInfo.dependent.size()) || (depInfo.dependent[idx] != *itED)) {
										idx = state_t(depInfo.dependent.size());
										depInfo.dependent.emplace_back(*itED);
										depInfo.distCounter++;
										depInfo.link.emplace_back(vector<pair<input_t, state_t>>({ make_pair(i, dI) }));
									} else if (idx != dI) {
										depInfo.link[idx].emplace_back(i, dI);
									}
								}
								else if (!bestNext || (bestNext->undistinguishedStates > (*itED)->undistinguishedStates) ||
									((bestNext->undistinguishedStates == (*itED)->undistinguishedStates) &&
									(bestNext->sequence.size() > (*itED)->sequence.size()))) {
									bestNext = (*itED);
									bestInput = i;
								}
							}
							else {
								distinguishSTnode(fsm, nextStateNode, true);
								if (!nextStateNode->nextStates.empty()) {// valid separating input
									bestNext = nextStateNode;
									bestInput = i;
								}
								else {// check other inputs
									nextStateNode->nextStates.emplace_back(state_t(depInfo.dependent.size()));
									depInfo.distCounter++;
								}
								depInfo.extraDependent.insert(nextStateNode);
								depInfo.dependent.emplace_back(move(nextStateNode));
								depInfo.link.emplace_back(vector<pair<input_t, state_t>>({ make_pair(i, dI) }));
							}
						}
						else if (!bestNext || (bestNext->undistinguishedStates > next->undistinguishedStates) ||
							((bestNext->undistinguishedStates == next->undistinguishedStates) &&
							(bestNext->sequence.size() > next->sequence.size()))) {
							bestNext = next;
							bestInput = i;
						}
					}
				}
			}
			if (bestNext) {
				if (bestNext->undistinguishedStates == 0) {
					depInfo.bfsqueue.emplace(seq_len_t(bestNext->sequence.size()), dI);
					// bestNext is either at the back of dependent, or before depSize, or distinguished in the ST
					if ((bestNext == depInfo.dependent.back()) && (depInfo.dependent.size() > depSize)) {
						// let bestNext in the dependent
						depInfo.dependent[depSize].swap(depInfo.dependent.back());
						depSize++;
					}
					while (depInfo.dependent.size() > depSize) {// remove added blocks that are not necessary
						if (depInfo.dependent.back()->nextStates.size() == 1) depInfo.distCounter--;
						depInfo.dependent.pop_back();
						depInfo.link.pop_back();
					}
				}
				node->succ.emplace_back(bestInput, move(bestNext));
			}
		}
	}

	static void prepareLinksOfInvalid(const unique_ptr<DFSM>& fsm, dependent_info_t& depInfo, const unique_ptr<SplittingTree>& st) {
		auto initDepSize = depInfo.dependent.size();
		// check dependent
		for (state_t dI = 0; dI < depInfo.dependent.size(); dI++) {
			auto node = depInfo.dependent[dI];
			if (node->nextStates.size() > 1) continue; // already separated
			shared_ptr<st_node_t> bestInvalidNext, bestNext;
			input_t bestInvalidInput(STOUT_INPUT), bestInput(STOUT_INPUT);
			size_t bestInvalidScore(0);
			auto seqInIt = node->sequence.begin();
			auto depSize = depInfo.dependent.size();
			// check invalid inputs
			for (input_t i = 0; i < node->sequence.size(); i++, seqInIt++) {
				node->succ[i].first = *seqInIt; // for easier access to right input
				if (node->succ[i].second->undistinguishedStates != 0) {// invalid input
					if (node->succ[i].second->succ.size() == 1) {// invalid transfering input
						if (node->succ[i].second->undistinguishedStates + 1 < node->block.size()) {
							// some states still can be distinguished
							list<state_t> diffStates;
							set<state_t> diffNS;
							state_t pivot = node->succ[i].second->nextStates[0];
							for (const auto& stateI : node->succ[i].second->nextStates) {
								diffNS.insert(stateI);
								if (st->curNode[pivot] != st->curNode[stateI]) {
									bool inDiff = false;
									for (const auto& diffState : diffStates) {
										if (st->curNode[stateI] == st->curNode[diffState]) {
											inDiff = true;
											break;
										}
									}
									if (!inDiff) {
										diffStates.emplace_back(stateI);
									}
								}
							}
							auto next = st->curNode[pivot];
							// find the lowest node of ST with block of all diffStates and pivot
							for (const auto& diffState : diffStates) {
								auto idx = getStatePairIdx(pivot, diffState);
								if (next->block.size() < st->distinguished[idx]->block.size()) {
									next = st->distinguished[idx];
								}
							}
							if (diffStates.empty() || (next->undistinguishedStates > 0)) {/* &&
								(next->block.size() > diffNS.size()))) {
								if (next->block.size() == diffNS.size()) {
									depInfo.link[next->nextStates[0]].emplace_back(i, dI);
									next = nullptr;
								}
								else {*/
									auto nextStateNode = make_shared<st_node_t>();
									nextStateNode->block.assign(diffNS.begin(), diffNS.end());
									auto itED = depInfo.extraDependent.find(nextStateNode);
									if (itED != depInfo.extraDependent.end()) {
										if ((*itED)->nextStates.size() == 1) {// undistinguished
											auto& idx = (*itED)->nextStates[0];
											if ((idx >= depInfo.dependent.size()) || (depInfo.dependent[idx] != *itED)) {
												idx = state_t(depInfo.dependent.size());
												depInfo.dependent.emplace_back(*itED);
												depInfo.distCounter++;
												depInfo.link.emplace_back(vector<pair<input_t, state_t>>({ make_pair(i, dI) }));
											}
											else if (idx != dI) {
												depInfo.link[idx].emplace_back(i, dI);
											}
											next = nullptr;
										}
										else {
											next = (*itED);
										}
									}
									else {
										distinguishSTnode(fsm, nextStateNode, true);
										if (!nextStateNode->nextStates.empty()) {// valid separating input
											next = nextStateNode;
										}
										else {// check other inputs
											nextStateNode->nextStates.emplace_back(state_t(depInfo.dependent.size()));
											depInfo.distCounter++;
											next = nullptr;
										}
										depInfo.extraDependent.insert(nextStateNode);
										depInfo.dependent.emplace_back(move(nextStateNode));
										depInfo.link.emplace_back(vector<pair<input_t, state_t>>({ make_pair(i, dI) }));
									}
								//}
							}
							if (next) {
								auto invalidScore = getInvalidScore(node, i, next);
								if ((bestInvalidInput == STOUT_INPUT) || (bestInvalidScore > invalidScore)) {
									bestInvalidNext = next;
									bestInvalidInput = i;
									bestInvalidScore = invalidScore;
								}
							}
						}
					}
					else {// invalid distinguishing input
						size_t invalidScore(node->block.size() * node->block.size());
						for (const auto& p : node->succ[i].second->succ) {
							if (p.second->undistinguishedStates == 0) {
								invalidScore -= (p.second->block.size() * node->block.size() + 1);
							}
						}
						invalidScore *= node->block.size();
						invalidScore -= node->succ[i].second->succ.size();
						invalidScore *= node->block.size();
						invalidScore += node->succ[i].second->undistinguishedStates;
						invalidScore *= node->block.size();
						invalidScore += 1;
						if ((bestInvalidInput == STOUT_INPUT) || (bestInvalidScore > invalidScore)) {
							bestInvalidNext = nullptr;
							bestInvalidInput = i;
							bestInvalidScore = invalidScore;
						}
					}
				}
				else if (dI >= initDepSize) {// check valid inputs of extra dependent nodes
					list<state_t> diffStates;
					state_t pivot = node->succ[i].second->nextStates[0];
					for (const auto& stateI : node->succ[i].second->nextStates) {
						if (st->curNode[pivot] != st->curNode[stateI]) {
							bool inDiff = false;
							for (const auto& diffState : diffStates) {
								if (st->curNode[stateI] == st->curNode[diffState]) {
									inDiff = true;
									break;
								}
							}
							if (!inDiff) {
								diffStates.emplace_back(stateI);
							}
						}
					}
					auto next = st->curNode[pivot];
					// find the lowest node of ST with block of all diffStates and pivot
					for (const auto& diffState : diffStates) {
						auto idx = getStatePairIdx(pivot, diffState);
						if (next->block.size() < st->distinguished[idx]->block.size()) {
							next = st->distinguished[idx];
						}
					}
					if (diffStates.empty() || (next->undistinguishedStates > 0)) {
					//if ((next->undistinguishedStates > 0) // next has an invalid sequence
						//&& (!bestNext || (bestNext->undistinguishedStates > 0)) // bestNext as well
						//&& (next->block.size() > node->block.size())) {
						// next has an invalid input -> check for a better input fot current nextStates
						auto nextStatesSorted = node->succ[i].second->nextStates;
						sort(nextStatesSorted.begin(), nextStatesSorted.end());
						auto nextStateNode = make_shared<st_node_t>();
						nextStateNode->block = move(nextStatesSorted);
						auto itED = depInfo.extraDependent.find(nextStateNode);
						if (itED != depInfo.extraDependent.end()) {
							if ((*itED)->nextStates.size() == 1) {// undistinguished
								auto& idx = (*itED)->nextStates[0];
								if ((idx >= depInfo.dependent.size()) || (depInfo.dependent[idx] != *itED)) {
									idx = state_t(depInfo.dependent.size());
									depInfo.dependent.emplace_back(*itED);
									depInfo.distCounter++;
									depInfo.link.emplace_back(vector<pair<input_t, state_t>>({ make_pair(i, dI) }));
								}
								else if (idx != dI) {
									depInfo.link[idx].emplace_back(i, dI);
								}
							}
							else if (!bestNext || (bestNext->undistinguishedStates > (*itED)->undistinguishedStates) ||
									((bestNext->undistinguishedStates == (*itED)->undistinguishedStates) &&
									(bestNext->sequence.size() > (*itED)->sequence.size()))) {
								bestNext = (*itED);
								bestInput = i;
							}
						}
						else {
							distinguishSTnode(fsm, nextStateNode, true);
							if (!nextStateNode->nextStates.empty()) {// valid separating input
								bestNext = nextStateNode;
								bestInput = i;
							}
							else {// check other inputs
								nextStateNode->nextStates.emplace_back(state_t(depInfo.dependent.size()));
								depInfo.distCounter++;
							}
							depInfo.extraDependent.insert(nextStateNode);
							depInfo.dependent.emplace_back(move(nextStateNode));
							depInfo.link.emplace_back(vector<pair<input_t, state_t>>({ make_pair(i, dI) }));
						}
					}
					else if (!bestNext || (bestNext->undistinguishedStates > next->undistinguishedStates) ||
						((bestNext->undistinguishedStates == next->undistinguishedStates) &&
						(bestNext->sequence.size() > next->sequence.size()))) {
						bestNext = next;
						bestInput = i;
					}					
				}
			}
			if (node->sequence.size() != node->succ.size()) {
				bestInput = node->succ.back().first;
				bestNext = node->succ.back().second;
				node->succ.pop_back();
			}
			if (bestNext) {
				if (bestNext->undistinguishedStates == 0) {
					depInfo.bfsqueue.emplace(seq_len_t(bestNext->sequence.size()), dI);
					// bestNext is either at the back of dependent, or before depSize, or distinguished in the ST
					if ((bestNext == depInfo.dependent.back()) && (depInfo.dependent.size() > depSize)) {
						// let bestNext in the dependent
						depInfo.dependent[depSize].swap(depInfo.dependent.back());
						depSize++;
					}
					while (depInfo.dependent.size() > depSize) {// remove added blocks that are not necessary
						if (depInfo.dependent.back()->nextStates.size() == 1) depInfo.distCounter--;
						depInfo.dependent.pop_back();
						depInfo.link.pop_back();
					}
					node->succ.emplace_back(bestInput, move(bestNext));
				}
				else {
					auto invalidScore = getInvalidScore(node, bestInput, bestNext);
					if (bestInvalidInput != STOUT_INPUT) {//compare invalid inputs
						if (bestInvalidScore > invalidScore) {
							bestInvalidScore = invalidScore;
						}
						else {
							bestInput = bestInvalidInput;
							bestNext = bestInvalidNext;
						}
					}
					node->undistinguishedStates = bestInvalidScore;
					node->succ.emplace_back(bestInput, move(bestNext));
					depInfo.bfsqueue.emplace(seq_len_t(node->undistinguishedStates), dI);
				}
			}
			else if (bestInvalidInput != STOUT_INPUT) {
				node->undistinguishedStates = bestInvalidScore;
				node->succ.emplace_back(bestInvalidInput, move(bestInvalidNext));
				depInfo.bfsqueue.emplace(seq_len_t(node->undistinguishedStates), dI);
			}
		}
	}
	
	static bool processDependent(dependent_info_t& depInfo,
		priority_queue<shared_ptr<st_node_t>, vector<shared_ptr<st_node_t>>, blockcomp>& partition,
		const unique_ptr<SplittingTree>& st, bool useStout) {

		while (!depInfo.bfsqueue.empty()) {
			auto dI = depInfo.bfsqueue.top().second;
			auto node = depInfo.dependent[dI];
			depInfo.bfsqueue.pop();
			if (node->nextStates.size() == 1) {// undistinguished
				node->sequence.clear();
				// hack (input stored in place of output -> it does not have to be found in node->sequence by iteration first)
				node->sequence.emplace_back(node->succ[node->succ.back().first].first);
				auto next = node->succ.back().second;// best successor
				if (next) {// valid input
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
							node->succ[i].second->succ.clear();
							node->succ[i].second->undistinguishedStates = 0;
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
					if (node->undistinguishedStates > 0) {
						node->undistinguishedStates -= (next->sequence.size() + 1);
						node->undistinguishedStates /= node->block.size();
						node->undistinguishedStates %= node->block.size();
					}
				}
				else {// invalid input
					next = node->succ[node->succ.back().first].second;
					node->undistinguishedStates = next->undistinguishedStates;
					node->nextStates.swap(next->nextStates);
					node->succ.swap(next->succ);
					for (const auto& p : node->succ) {
						p.second->undistinguishedStates = 0;
					}
				}
				if (dI < depInfo.initDepSize) {
					// update links
					for (output_t i = 0; i < node->succ.size(); i++) {
						next = node->succ[i].second;
						if (next->block.size() > 1) {// child is not singleton
							partition.emplace(next);
						}
						for (state_t stateI : next->block) {
							st->curNode[stateI] = next;
							for (output_t j = i + 1; j < node->succ.size(); j++) {
								for (state_t stateJ : node->succ[j].second->block) {
									auto idx = getStatePairIdx(stateI, stateJ);
									st->distinguished[idx] = node;// where two states were distinguished
								}
							}
						}
					}
				}
				// count resolved dependent
				depInfo.distCounter--;
				// push unresolved dependent to queue
				for (const auto& p : depInfo.link[dI]) {
					next = depInfo.dependent[p.second];
					if (next->nextStates.size() == 1) {// still unresolved
						if ((node->undistinguishedStates == 0) && 
							(node->block.size() == next->block.size())) {// valid input sequences
							if (next->sequence.size() == next->succ.size()) {// best not set
								next->succ.emplace_back(p.first, node);
								depInfo.bfsqueue.emplace(seq_len_t(node->sequence.size()), next->nextStates[0]);
							}
							else if (!next->succ.back().second ||
								(next->succ.back().second->undistinguishedStates > 0) ||
								((next->succ.back().second->undistinguishedStates == 0) &&
								(next->succ.back().second->sequence.size() > node->sequence.size()))) {// update best
								next->succ.back().first = p.first;
								next->succ.back().second = node;
								next->undistinguishedStates = 0;
								depInfo.bfsqueue.emplace(seq_len_t(node->sequence.size()), next->nextStates[0]);
							}
						}
						else {// invalid input sequences
							auto invalidScore = getInvalidScore(next, p.first, node);
							if (next->sequence.size() == next->succ.size()) {// best not set
								next->succ.emplace_back(p.first, node);
								next->undistinguishedStates = invalidScore;
								depInfo.bfsqueue.emplace(seq_len_t(invalidScore), next->nextStates[0]);
							}
							else if (invalidScore < next->undistinguishedStates) {
								next->succ.back().first = p.first;
								next->succ.back().second = node;
								next->undistinguishedStates = invalidScore;
								depInfo.bfsqueue.emplace(seq_len_t(invalidScore), next->nextStates[0]);
							}
						}
					}
				}
				depInfo.link[dI].clear();
				//depInfo.dependent[dI].reset();
			}
		}
		return (depInfo.distCounter == 0);
	}
	
	unique_ptr<SplittingTree> getSplittingTree(const unique_ptr<DFSM>& fsm, bool allowInvalidInputs, bool omitUnnecessaryStoutInputs) {
		RETURN_IF_UNREDUCED(fsm, "FSMsequence::getSplittingTree", nullptr);
		bool useStout = !omitUnnecessaryStoutInputs && fsm->isOutputState();
		state_t N = fsm->getNumberOfStates();
		priority_queue<shared_ptr<st_node_t>, vector<shared_ptr<st_node_t>>, blockcomp> partition;
		auto st = make_unique<SplittingTree>(N);
		dependent_info_t depInfo;
		if (fsm->isOutputState()) {
			auto node = st->rootST;
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
					st->curNode[stateI] = next;
					for (output_t j = i + 1; j < node->succ.size(); j++) {
						for (state_t stateJ : node->succ[j].second->block) {
							auto idx = getStatePairIdx(stateI, stateJ);
							st->distinguished[idx] = node;// where two states were distinguished
						}
					}
				}
			}
		}
		else {// Mealy
			for (state_t state = 0; state < N; state++) {
				st->rootST->block.emplace_back(state);
			}
			partition.emplace(st->rootST);
		}
		while (!partition.empty()) {
			auto node = partition.top();
			partition.pop();
			auto dIt = depInfo.extraDependent.find(node);
			if (dIt != depInfo.extraDependent.end()) {// already analysed
				node->nextStates.swap((*dIt)->nextStates);
				node->sequence.swap((*dIt)->sequence);
				node->succ.swap((*dIt)->succ);
				node->undistinguishedStates = (*dIt)->undistinguishedStates;
				depInfo.extraDependent.erase(dIt);
				if (node->nextStates.size() == 1) node->nextStates.clear();//undistinguished
			}
			else {
				// find distinguishing input or sort out other valid inputs
				distinguishSTnode(fsm, node, allowInvalidInputs);
				if (node->succ.empty()) {// no valid input - possible only by root
					return nullptr;
				}
			}
			if (!node->nextStates.empty()) {// block is distinguished by an input
				for (output_t i = 0; i < node->succ.size(); i++) {
					const auto& next = node->succ[i].second;
					if (next->block.size() > 1) {// child is not singleton
						partition.emplace(next);
					}
					for (state_t stateI : next->block) {
						st->curNode[stateI] = next;
						for (output_t j = i + 1; j < node->succ.size(); j++) {
							for (state_t stateJ : node->succ[j].second->block) {
								auto idx = getStatePairIdx(stateI, stateJ);
								st->distinguished[idx] = node;// where two states were distinguished
							}
						}
					}
				}
			}
			else {
				depInfo.dependent.emplace_back(node);
			}
			// are all blocks with max cardinality distinguished by one input?
			//if (!depInfo.dependent.empty() && partition.empty()) {
			if (!depInfo.dependent.empty() && (partition.empty() || (partition.top()->block.size() != node->block.size()))) {
				depInfo.initDepSize = depInfo.distCounter = depInfo.dependent.size();
				prepareLinks(fsm, depInfo, st);
				// check that all dependent was divided
				if (!processDependent(depInfo, partition, st, useStout)) {
					if (allowInvalidInputs) {
						prepareLinksOfInvalid(fsm, depInfo, st);
						if (!processDependent(depInfo, partition, st, useStout)) {
							throw "";
						}
					}
					else {
						return nullptr;
					}
				}
				depInfo.dependent.clear();
				//depInfo.extraDependent.clear();
			}
		}
		return st;
	}

	sequence_in_t getSeparatingSequenceFromSplittingTree(const unique_ptr<DFSM>& fsm, const unique_ptr<SplittingTree>& st,
		state_t state, set<state_t> states, bool omitUnnecessaryStoutInputs) {
		if ((state > fsm->getNumberOfStates()) || states.empty() || !states.count(state) || !st) {
			ERROR_MESSAGE("FSMsequence::getSeparatingSequenceFromSplittingTree - invalid parameters");
			return sequence_in_t();
		}
		bool useStout = !omitUnnecessaryStoutInputs && fsm->isOutputState();
		sequence_in_t seq;
		while (states.size() > 1) {
			auto it = states.begin();
			auto pivot = *it;
			++it;
			auto next = st->distinguished[getStatePairIdx(pivot, *it)];
			// find lowest node of ST with block of all states
			for (++it; it != states.end(); ++it) {
				auto idx = getStatePairIdx(pivot, *it);
				if (next->block.size() < st->distinguished[idx]->block.size()) {
					next = st->distinguished[idx];
				}
			}
			seq.insert(seq.end(), next->sequence.begin(), next->sequence.end());
			if (useStout && (seq.back() != STOUT_INPUT)) seq.push_back(STOUT_INPUT);
			auto out = fsm->getOutputAlongPath(state, next->sequence).back();
			set<state_t> nextStates;
			for (const auto &p : next->succ) {
				if (p.first == out) {
					auto bIt = next->block.begin();
					auto nIt = next->nextStates.begin();
					auto nbIt = p.second->block.begin();
					for (auto& s : states) {
						for (; (nbIt != p.second->block.end()) && (*nbIt < s); ++nbIt);
						if (nbIt == p.second->block.end()) break;
						if (*nbIt == s) {// state s has the same output
							for (; (bIt != next->block.end()) && (*bIt < s); ++bIt, ++nIt);
							nextStates.insert(*nIt);
						}
					}
					break;
				}
			}
			/* alternatively get next states from FSM
			for (auto& s : states) {
			if (out == fsm->getOutputAlongPath(s, next->sequence).back()) {
			nextStates.insert(fsm->getEndPathState(s, next->sequence));
			}
			}
			*/
			state = fsm->getEndPathState(state, next->sequence);
			states.swap(nextStates);
		}
		return seq;
	}

	sequence_vec_t getStatePairsSeparatingSequencesFromSplittingTree(const unique_ptr<DFSM>& fsm, bool omitUnnecessaryStoutInputs) {
		RETURN_IF_UNREDUCED(fsm, "FSMsequence::getStatePairsSeparatingSequencesFromSplittingTree", sequence_vec_t());
		bool useStout = !omitUnnecessaryStoutInputs && fsm->isOutputState();
		auto st = getSplittingTree(fsm, true, omitUnnecessaryStoutInputs);
		state_t M, N = fsm->getNumberOfStates();
		M = ((N - 1) * N) / 2;
		sequence_vec_t seq(M);
		for (state_t j = 1; j < N; j++) {
			for (state_t i = 0; i < j; i++) {
				auto idx = getStatePairIdx(i, j);
				seq[idx] = st->distinguished[idx]->sequence;
			}
		}
		return seq;
	}

	vector<sequence_set_t> getHarmonizedStateIdentifiers(const unique_ptr<DFSM>& fsm,
		const unique_ptr<SplittingTree>& st, bool omitUnnecessaryStoutInputs) {
		RETURN_IF_UNREDUCED(fsm, "FSMsequence::getHarmonizedStateIdentifiers", vector<sequence_set_t>());
		bool useStout = !omitUnnecessaryStoutInputs && fsm->isOutputState();
		state_t N = fsm->getNumberOfStates();
		vector<sequence_set_t> outSCSets(N);
		set<state_t> stateSet;
		for (state_t i = 0; i < N; i++) {
			stateSet.insert(i);
		}
		for (state_t i = 0; i < N; i++) {
			set<state_t> states(stateSet);
			do {
				auto seq = getSeparatingSequenceFromSplittingTree(fsm, st, i, states, omitUnnecessaryStoutInputs);
				outSCSets[i].insert(seq);
				auto refOut = fsm->getOutputAlongPath(i, seq);
				for (auto it = states.begin(); it != states.end();) {
					if (refOut != fsm->getOutputAlongPath(*it, seq)) {
						it = states.erase(it);
					}
					else ++it;
				}
			} while (states.size() > 1);
		}
		return outSCSets;
	}

}
