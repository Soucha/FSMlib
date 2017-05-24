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

	struct dependent_info_t {
		size_t distCounter, initDepSize;
		vector<shared_ptr<st_node_t>> dependent;
		vector<vector<pair<input_t, state_t>>> link;
		priority_queue<pair<seq_len_t, state_t>, vector<pair<seq_len_t, state_t>>, lencomp> bfsqueue;
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

	static void distinguishSTnode(const unique_ptr<DFSM>& fsm, const shared_ptr<st_node_t>& node, bool allowInvalidInputs = false) {
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
		size_t invalidScore(node->block.size() * node->block.size());
		state_t undistinguished(0);
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
			if (currUndist == undistinguished) {
				invalidScore -= (diffStates.size() * node->block.size() + 1);
			}
		}
		invalidScore *= node->block.size();
		invalidScore += undistinguished;
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
			input_t bestInput(STOUT_INPUT), bestInvalidInput(STOUT_INPUT);
			size_t bestInvalidScore;
			bool hasLinkToOtherDependent = false;
			auto depSize = depInfo.dependent.size();
			// check other valid input
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
							hasLinkToOtherDependent = true;
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
							&& (next->block.size() > node->succ[i].second->nextStates.size())) {
							// next has an invalid input -> check for a better input fot current nextStates
							size_t depI = depInfo.initDepSize;
							for(; depI < depInfo.dependent.size(); depI++) {
								if (node->succ[i].second->nextStates == depInfo.dependent[depI]->block) {
									break;
								}
							}
							if (depI < depInfo.dependent.size()) {
								if (depInfo.dependent[depI]->nextStates.size() == 1) {// undistinguished
									depInfo.link[depI].emplace_back(i, dI);
									//hasLinkToOtherDependent = true;
								}
								else {
									bestNext = depInfo.dependent[depI];
									bestInput = i;
								}
							}
							else {
								auto nextStateNode = make_shared<st_node_t>();
								nextStateNode->block = node->succ[i].second->nextStates;
								distinguishSTnode(fsm, nextStateNode, true);
								if (!nextStateNode->nextStates.empty()) {// valid separating input
									bestNext = nextStateNode;
									bestInput = i;
								}
								else {// check other inputs
									nextStateNode->nextStates.emplace_back(state_t(depInfo.dependent.size()));
									depInfo.distCounter++;
								}
								depInfo.dependent.emplace_back(nextStateNode);
								depInfo.link.emplace_back(vector<pair<input_t, state_t>>({ make_pair(i, dI) }));
							}
						} else if (!bestNext || (bestNext->undistinguishedStates > next->undistinguishedStates) ||
								((bestNext->undistinguishedStates == next->undistinguishedStates) &&
								(bestNext->sequence.size() > next->sequence.size()))) {
							bestNext = next;
							bestInput = i;
						}
					}
				}
				else {// invalid input
					size_t invalidScore(node->block.size() * node->block.size());
					for (const auto& p : node->succ[i].second->succ) {
						if (p.second->undistinguishedStates == 0) {
							invalidScore -= (p.second->block.size() * node->block.size() + 1);
						}
					}
					invalidScore *= node->block.size();
					invalidScore += node->succ[i].second->undistinguishedStates;
					if ((bestInvalidInput == STOUT_INPUT) || (bestInvalidScore > invalidScore)) {
						bestInvalidInput = i;
						bestInvalidScore = invalidScore;
					}
				}
			}
			if (bestNext) {
				if (bestNext->undistinguishedStates == 0) {
					depInfo.bfsqueue.emplace(seq_len_t(bestNext->sequence.size()), dI);
					// bestNext is either at the back of dependent, or befor depSize, or distinguished in the ST
					if ((bestNext == depInfo.dependent.back()) && (depInfo.dependent.size() > depSize)) {
						// let bestNext in the dependent
						depInfo.dependent[depSize].swap(depInfo.dependent.back());
						depSize++;
					}
					while (depInfo.dependent.size() > depSize) {// remove added blocks that are not necessary
						depInfo.dependent.pop_back();
						depInfo.link.pop_back();
						depInfo.distCounter--;
					}
				}
				else if (bestInvalidInput != STOUT_INPUT) {
					//compare invalid inputs
					auto invalidScore = getInvalidScore(node, bestInput, bestNext);
					if (invalidScore % node->block.size() != bestNext->undistinguishedStates) {// should not be possible
						throw "";
					}
//					if (undistinguished == 0) {// valid input for next states
	//					depInfo.bfsqueue.emplace(seq_len_t(bestNext->sequence.size()), dI);
					//} else 
					if (bestInvalidScore > invalidScore) {
						bestInvalidInput = bestInput;
						bestInvalidScore = invalidScore;
					}
					else {
						bestInput = bestInvalidInput;
						bestNext = nullptr;
					}
					node->undistinguishedStates = bestInvalidScore;
				}
				node->succ.emplace_back(bestInput, move(bestNext));
			}
			else if (bestInvalidInput != STOUT_INPUT) {
				//if (hasLinkToOtherDependent) {
				node->undistinguishedStates = bestInvalidScore;
				node->succ.emplace_back(bestInvalidInput, nullptr);
				//}
				//else {// only invalid inputs

				//}
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
					node->undistinguishedStates %= node->block.size();
					// remove unused output
					for (long i = long(node->succ.size() - 1); i >= 0; i--) {
						if (node->succ[i].second->block.empty()) {
							node->succ[i].swap(node->succ.back());
							node->succ.pop_back();
						}
					}
				}
				else {// invalid input
					next = node->succ[node->succ.back().first].second;
					node->undistinguishedStates = next->undistinguishedStates;
					node->nextStates.swap(next->nextStates);
					node->succ.swap(next->succ);
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
						if (node->undistinguishedStates == 0) {// valid input sequences
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

	static void prepareLinksOfInvalid(dependent_info_t& depInfo) {
		// check dependent
		//depInfo.distCounter = 0;
		for (state_t dI = 0; dI < depInfo.dependent.size(); dI++) {
			auto& node = depInfo.dependent[dI];
			if (node->nextStates.size() > 1) continue; // already separated
			if (node->sequence.size() != node->succ.size()) {// best set
				depInfo.bfsqueue.emplace(seq_len_t(node->undistinguishedStates), dI);
			}
			//depInfo.distCounter++;
		}
	}

	unique_ptr<SplittingTree> getSplittingTree(const unique_ptr<DFSM>& fsm, bool allowInvalidInputs, bool useStout) {
		RETURN_IF_UNREDUCED(fsm, "FSMsequence::getSplittingTree", nullptr);
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
			if (!depInfo.dependent.empty() && (partition.empty() || (partition.top()->block.size() != node->block.size()))) {
				depInfo.initDepSize = depInfo.distCounter = depInfo.dependent.size(); 
				prepareLinks(fsm, depInfo, st);
				// check that all dependent was divided
				if (!processDependent(depInfo, partition, st, useStout)) {
					if (allowInvalidInputs) {
						if (!processDependent(depInfo, partition, st, useStout)) {
							throw "";
						}
					}
					else {
						return nullptr;
					}
				}
				depInfo.dependent.clear();
			}
		}
		return st;
	}

	unique_ptr<AdaptiveDS> buildADS(const vector<state_t>& block,
		const unique_ptr<SplittingTree>& st, bool useStout) {
		if (!st) return nullptr;
		auto outADS = make_unique<AdaptiveDS>();
		outADS->initialStates = outADS->currentStates = block;

		queue<AdaptiveDS*> fifo;
		fifo.push(outADS.get());
		while (!fifo.empty()) {
			auto adsNode = fifo.front();
			fifo.pop();
			if (adsNode->currentStates.size() == 1) continue;
			auto pivot = adsNode->currentStates[0];
			auto next = st->distinguished[getStatePairIdx(pivot, adsNode->currentStates[1])];
			// find lowest node of ST with block of all currentStates
			for (state_t j = 2; j < adsNode->currentStates.size(); j++) {
				auto idx = getStatePairIdx(pivot, adsNode->currentStates[j]);
				if (next->block.size() < st->distinguished[idx]->block.size()) {
					next = st->distinguished[idx];
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
		bool useStout = !omitUnnecessaryStoutInputs && fsm->isOutputState();
		auto st = getSplittingTree(fsm, false, useStout);
		if (!st) return nullptr;
		// build ADS from ST
		return buildADS(st->rootST->block, st, useStout);
	}
}
