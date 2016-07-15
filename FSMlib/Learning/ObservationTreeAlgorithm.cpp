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

#include <numeric>
#include "FSMlearning.h"

namespace FSMlearning {
	struct ot_node_t {
		sequence_in_t accessSequence;
		output_t incomingOutput;
		state_t state;
		output_t stateOutput;
		vector<shared_ptr<ot_node_t>> next;
		vector<input_t> nextInputs;
		
		weak_ptr<ot_node_t> parent;
		input_t incomingInputIdx;
		input_t distInputIdx;

		state_t extraStateLevel;

		list<ot_node_t*> consistentNodes;
		set<state_t> refStates;

		ot_node_t(vector<input_t> inputs) : 
			incomingOutput(DEFAULT_OUTPUT), state(NULL_STATE), stateOutput(DEFAULT_OUTPUT),
			next(inputs.size()), nextInputs(move(inputs)), extraStateLevel(NULL_STATE), distInputIdx(STOUT_INPUT) {
		}

		ot_node_t(const shared_ptr<ot_node_t>& parent, input_t input, input_t idx, 
			output_t transitionOutput, output_t stateOutput, vector<input_t> inputs) :
			accessSequence(parent->accessSequence), incomingOutput(transitionOutput), state(NULL_STATE),
			stateOutput(stateOutput), next(inputs.size()), nextInputs(move(inputs)), 
			parent(parent), incomingInputIdx(idx), distInputIdx(STOUT_INPUT),
			extraStateLevel(parent->extraStateLevel + 1) {
			accessSequence.push_back(input);
		}
	};

	struct ObservationTree {
		vector<shared_ptr<ot_node_t>> stateNodes;
		shared_ptr<ot_node_t> bbNode;
		list<shared_ptr<ot_node_t>> uncheckedNodes;
		state_t numberOfExtraStates;
		unique_ptr<DFSM> conjecture;
		bool spaceEfficient = true;
	};

	typedef double frac_t;

	struct ads_t {
		list<shared_ptr<ot_node_t>> nodes;
		input_t input;
		map<output_t, shared_ptr<ads_t>> next;

		ads_t() : input(STOUT_INPUT) {}
		ads_t(const shared_ptr<ot_node_t>& node) : nodes({ node }), input(STOUT_INPUT) {}
	};

	static void checkNumberOfOutputs(const unique_ptr<Teacher>& teacher, const unique_ptr<DFSM>& conjecture) {
		if (conjecture->getNumberOfOutputs() != teacher->getNumberOfOutputs()) {
			conjecture->incNumberOfOutputs(teacher->getNumberOfOutputs() - conjecture->getNumberOfOutputs());
		}
	}

	static input_t getNextIdx(const shared_ptr<ot_node_t>& node, input_t input) {
		if ((node->nextInputs.size() > input) && (node->nextInputs[input] == input)) return input;
		auto lower = std::lower_bound(node->nextInputs.begin(), node->nextInputs.end(), input);
		if (lower != node->nextInputs.end() && *lower == input) {
			return input_t(std::distance(node->nextInputs.begin(), lower));
		}
		return STOUT_INPUT;
	}

	bool consistentComp(const ot_node_t* a, const ot_node_t* b) {
		if (a->accessSequence.size() == b->accessSequence.size())
			return a->accessSequence < b->accessSequence;
		return a->accessSequence.size() < b->accessSequence.size();
	}

	static list<ot_node_t*>::const_iterator getItToInsert(const list<ot_node_t*>& con, const ot_node_t* el) {
		return upper_bound(con.begin(), con.end(), el, consistentComp);
	}

	static list<ot_node_t*>::const_iterator getItOfElement(const list<ot_node_t*>& con, const ot_node_t* el) {
		auto it = lower_bound(con.begin(), con.end(), el, consistentComp);
		if ((it != con.end()) && (*it == el)) return it;
		return con.end();
	}

	static bool areNodesDifferent(const shared_ptr<ot_node_t>& n1, const shared_ptr<ot_node_t>& n2) {
		if ((n1->stateOutput != n2->stateOutput) || (n1->nextInputs != n2->nextInputs)) return true;
		for (input_t i = 0; i < n1->nextInputs.size(); i++) {
			if ((n1->next[i]) && (n2->next[i]) && ((n1->next[i]->incomingOutput != n2->next[i]->incomingOutput)
				|| areNodesDifferent(n1->next[i], n2->next[i])))
				return true;
		}
		return false;
	}

	static bool areNodesDifferentUnder(const shared_ptr<ot_node_t>& n1, const shared_ptr<ot_node_t>& n2) {
		if ((n1->stateOutput != n2->stateOutput) || (n1->nextInputs != n2->nextInputs)) return true;
		if (n1->distInputIdx == STOUT_INPUT) return false;
		auto& idx = n1->distInputIdx;
		if ((n2->next[idx]) && ((n1->next[idx]->incomingOutput != n2->next[idx]->incomingOutput)
				|| areNodesDifferentUnder(n1->next[idx], n2->next[idx])))
				return true;
		return false;
	}

	static bool areNextNodesDifferent(const shared_ptr<ot_node_t>& n1, const shared_ptr<ot_node_t>& n2) {
		return (n1->stateOutput != n2->stateOutput) || (n1->incomingOutput != n2->incomingOutput)
			|| (n1->nextInputs != n2->nextInputs) || (getItOfElement(n2->consistentNodes, n1.get()) == n2->consistentNodes.end());
	}
	
	static void findConsistentNodes(const shared_ptr<ot_node_t>& node, ObservationTree& ot) {
		stack<shared_ptr<ot_node_t>> nodes;
		for (const auto& sn : ot.stateNodes) {
			nodes.emplace(sn);
			while (!nodes.empty()) {
				auto otNode = move(nodes.top());
				nodes.pop();
				if ((node != otNode) && (node->stateOutput == otNode->stateOutput) && (node->nextInputs == otNode->nextInputs)) {
					node->consistentNodes.emplace(getItToInsert(node->consistentNodes, otNode.get()), otNode.get());
					otNode->consistentNodes.emplace(getItToInsert(otNode->consistentNodes, node.get()), node.get());
					if (otNode->state != NULL_STATE) {
						node->refStates.emplace(otNode->state);
					}
				}
				for (const auto& nn : otNode->next) {
					if (nn && (nn->state == NULL_STATE)) nodes.emplace(nn);
				}
			}
		}
	}

	static void findConsistentStateNodes(const shared_ptr<ot_node_t>& node, ObservationTree& ot) {
		for (const auto& sn : ot.stateNodes) {
			if ((node->stateOutput == sn->stateOutput) && (node->nextInputs == sn->nextInputs)) {
				node->refStates.emplace(sn->state);
				sn->consistentNodes.emplace(getItToInsert(sn->consistentNodes, node.get()), node.get());
			}
		}
	}

	static void addNewStateToConsistentNodes(const shared_ptr<ot_node_t>& node, ObservationTree& ot) {
		stack<shared_ptr<ot_node_t>> nodes;
		for (const auto& sn : ot.stateNodes) {
			nodes.emplace(sn);
			while (!nodes.empty()) {
				auto otNode = move(nodes.top());
				nodes.pop();
				if ((node != otNode) && (otNode->state == NULL_STATE) && !areNodesDifferent(node, otNode)) {
					otNode->refStates.emplace(node->state);
					node->consistentNodes.emplace(getItToInsert(node->consistentNodes, otNode.get()), otNode.get());
				}
				for (const auto& nn : otNode->next) {
					if (nn && (nn->state == NULL_STATE)) nodes.emplace(nn);
				}
			}
		}
	}

	static void checkNode(const shared_ptr<ot_node_t>& node, ObservationTree& ot) {
		if (node->refStates.empty()) {// new state
			node->state = ot.conjecture->addState(node->stateOutput);
			node->extraStateLevel = 0;
			ot.stateNodes.emplace_back(node);
			if (ot.spaceEfficient) {
				addNewStateToConsistentNodes(node, ot);
			} else {
				for (auto& cn : node->consistentNodes) {
					cn->refStates.emplace(node->state);
				}
			}
			node->refStates.emplace(node->state);
			
			auto parent = node->parent.lock();
			if (parent && (parent->state != NULL_STATE)) {
				ot.conjecture->setTransition(parent->state, parent->nextInputs[node->incomingInputIdx], node->state, 
					(ot.conjecture->isOutputTransition() ? node->incomingOutput : DEFAULT_OUTPUT));
			}
			for (input_t i = 0; i < node->nextInputs.size(); i++) {
				if (node->next[i] && (node->next[i]->refStates.size() == 1)) {
					ot.conjecture->setTransition(node->state, node->nextInputs[i], *(node->next[i]->refStates.begin()), 
						(ot.conjecture->isOutputTransition() ? node->next[i]->incomingOutput : DEFAULT_OUTPUT));
				}
			}
			
			ot.uncheckedNodes.clear();// the sign of new state was found
		} else if (node->state != NULL_STATE) {
			if (node->next[node->distInputIdx]->refStates.size() == 1) {
				ot.conjecture->setTransition(node->state, node->nextInputs[node->distInputIdx],
					*(node->next[node->distInputIdx]->refStates.begin()), 
					(ot.conjecture->isOutputTransition() ? node->next[node->distInputIdx]->incomingOutput : DEFAULT_OUTPUT));
			}
		}/* 
		else if ((node->refStates.size() == 1) && (node->state == NULL_STATE)) {
			auto state = *(node->refStates.begin());
			if (ot.stateNodes[state]->accessSequence.size() > node->accessSequence.size()) {
				// swap states
			}
		}*/		
	}

	static void checkPrevious(const shared_ptr<ot_node_t>& node, ObservationTree& ot) {
		auto& idx = node->distInputIdx;
		if (idx != STOUT_INPUT) {
			if (ot.spaceEfficient) {
				if (node->state == NULL_STATE) {
					for (auto snIt = node->refStates.begin(); snIt != node->refStates.end();) {
						if (areNodesDifferentUnder(node, ot.stateNodes[*snIt])) {
							ot.stateNodes[*snIt]->consistentNodes.erase(getItOfElement(ot.stateNodes[*snIt]->consistentNodes, node.get()));
							snIt = node->refStates.erase(snIt);
						}
						else ++snIt;
					}
				} else {// state node
					for (auto cnIt = node->consistentNodes.begin(); cnIt != node->consistentNodes.end();) {
						auto parent = (*cnIt)->parent.lock();// it must have parent
						auto cnPtr = parent->next[(*cnIt)->incomingInputIdx];
						if (areNodesDifferentUnder(node, cnPtr)) {
							cnPtr->refStates.erase(node->state);
							if (cnPtr->refStates.size() == 1) {
								if (parent->state != NULL_STATE) {
									ot.conjecture->setTransition(parent->state, parent->nextInputs[cnPtr->incomingInputIdx], 
										*(cnPtr->refStates.begin()),
										(ot.conjecture->isOutputTransition() ? cnPtr->incomingOutput : DEFAULT_OUTPUT));
								}
							} else {
								checkNode(cnPtr, ot);
							}
							cnIt = node->consistentNodes.erase(cnIt);
						} else ++cnIt;
					}
				}
			} else {
				for (auto cnIt = node->consistentNodes.begin(); cnIt != node->consistentNodes.end();) {
					if ((*cnIt)->next[idx] && (areNextNodesDifferent(node->next[idx], (*cnIt)->next[idx]))) {
						(*cnIt)->consistentNodes.erase(getItOfElement((*cnIt)->consistentNodes, node.get()));
						if (node->state != NULL_STATE) {
							(*cnIt)->refStates.erase(node->state);
							auto parent = (*cnIt)->parent.lock();// it must have parent
							auto cnPtr = parent->next[(*cnIt)->incomingInputIdx];
							if (cnPtr->refStates.size() == 1) {
								if (parent->state != NULL_STATE) {
									ot.conjecture->setTransition(parent->state, parent->nextInputs[cnPtr->incomingInputIdx],
										*(cnPtr->refStates.begin()),
										(ot.conjecture->isOutputTransition() ? cnPtr->incomingOutput : DEFAULT_OUTPUT));
								}
							} else {
								checkNode(cnPtr, ot);
							}
						} else if ((*cnIt)->state != NULL_STATE) {
							node->refStates.erase((*cnIt)->state);
						}
						cnIt = node->consistentNodes.erase(cnIt);
					} else ++cnIt;
				}
			}
		}
		checkNode(node, ot);
		auto parent = node->parent.lock();
		if (parent) {
			parent->distInputIdx = node->incomingInputIdx;
			checkPrevious(parent, ot);
		}
	}

	static void query(const shared_ptr<ot_node_t>& node, input_t idx, ObservationTree& ot, const unique_ptr<Teacher>& teacher) {
		auto& input = node->nextInputs[idx];
		state_t transitionOutput, stateOutput = DEFAULT_OUTPUT;
		if (!teacher->isProvidedOnlyMQ() && (ot.conjecture->getType() == TYPE_DFSM)) {
			sequence_in_t suffix({ input, STOUT_INPUT });
			auto output = (ot.bbNode == node) ? teacher->outputQuery(suffix) : 
				teacher->resetAndOutputQueryOnSuffix(node->accessSequence, suffix);
			transitionOutput = output.front();
			stateOutput = output.back();
		} else {
			transitionOutput = (ot.bbNode == node) ? teacher->outputQuery(input) :
				teacher->resetAndOutputQueryOnSuffix(node->accessSequence, input);
			if (ot.conjecture->getType() == TYPE_DFSM) {
				stateOutput = teacher->outputQuery(STOUT_INPUT);
			} else if (!ot.conjecture->isOutputTransition()) {// Moore, DFA
				stateOutput = transitionOutput;
				//transitionOutput = DEFAULT_OUTPUT;
			}
		}
		checkNumberOfOutputs(teacher, ot.conjecture);
		auto leaf = make_shared<ot_node_t>(node, input, idx, transitionOutput, stateOutput, teacher->getNextPossibleInputs());
		if (ot.conjecture->getNumberOfInputs() != teacher->getNumberOfInputs()) {
			ot.conjecture->incNumberOfInputs(teacher->getNumberOfInputs() - ot.conjecture->getNumberOfInputs());
		}
		node->next[idx] = leaf;
		if (ot.spaceEfficient) {
			findConsistentStateNodes(leaf, ot);
		} else {
			findConsistentNodes(leaf, ot);
		}
		ot.bbNode = leaf;
	}

	static bool checkResponse(const shared_ptr<ot_node_t>& node, input_t idx, ObservationTree& ot) {
		if (ot.conjecture->isOutputTransition()) {
			if (ot.conjecture->getOutput(*(node->refStates.begin()), node->nextInputs[idx]) != node->next[idx]->incomingOutput)
				return false;
		}
		if (ot.conjecture->isOutputState()) {
			if (ot.conjecture->getOutput(ot.conjecture->getNextState(*(node->refStates.begin()), node->nextInputs[idx]), STOUT_INPUT)
					!= node->next[idx]->stateOutput)
				return false;
		}
		return true;
	}

	static void tryExtendQueriedPath(shared_ptr<ot_node_t> node, ObservationTree& ot, const unique_ptr<Teacher>& teacher);

	static void checkAndQueryNext(const shared_ptr<ot_node_t>& node, ObservationTree& ot, const unique_ptr<Teacher>& teacher) {
		checkPrevious(node, ot);
		
		tryExtendQueriedPath(node, ot, teacher);

		if (ot.uncheckedNodes.empty()) {
			ot.uncheckedNodes.assign(ot.stateNodes.begin(), ot.stateNodes.end());
			ot.uncheckedNodes.sort([](const shared_ptr<ot_node_t>& n1, const shared_ptr<ot_node_t>& n2){
				return n1->accessSequence.size() < n2->accessSequence.size(); });
		}
	}

	static bool areDistinguished(list<shared_ptr<ot_node_t>>& nodes, bool spaceEfficient, bool isRefNode = false) {
		for (auto it1 = nodes.begin(); it1 != nodes.end(); ++it1) {
			auto it2 = it1;
			for (++it2; it2 != nodes.end(); ++it2) {
				if (spaceEfficient) {
					if (areNodesDifferent(*it1, *it2)) return true;
				} else {
					if ((*it1)->consistentNodes.size() < (*it2)->consistentNodes.size()) {
						if (getItOfElement((*it1)->consistentNodes, (*it2).get()) == (*it1)->consistentNodes.end()) return true;
					}
					else if (getItOfElement((*it2)->consistentNodes, (*it1).get()) == (*it2)->consistentNodes.end()) return true;
				}
			}
			if (isRefNode) break;
		}
		return false;
	}

	static void chooseADS(const shared_ptr<ads_t>& ads, const ObservationTree& ot, frac_t& bestVal, frac_t& currVal,
			seq_len_t& totalLen, seq_len_t currLength = 1, state_t undistinguishedStates = 0, double prob = 1) {
		auto numStates = state_t(ads->nodes.size()) + undistinguishedStates;
		frac_t localBest(1);
		seq_len_t subtreeLen, minLen = seq_len_t(-1);
		for (input_t i = 0; i < ot.conjecture->getNumberOfInputs(); i++) {
			undistinguishedStates = numStates - state_t(ads->nodes.size());
			map<output_t, shared_ptr<ads_t>> next;
			for (auto& node : ads->nodes) {
				auto idx = getNextIdx(node, i);
				if ((idx == STOUT_INPUT) || (!node->next[idx])) {
					undistinguishedStates++;
				} else {
					auto it = next.find(node->next[idx]->incomingOutput);
					if (it == next.end()) {
						next.emplace(node->next[idx]->incomingOutput, make_shared<ads_t>(node->next[idx]));
					} else {
						it->second->nodes.emplace_back(node->next[idx]);
					}
				}
			}
			if (next.empty()) continue;
			auto adsVal = currVal;
			subtreeLen = 0;
			for (auto p : next) {
				if ((p.second->nodes.size() == 1) || (!areDistinguished(p.second->nodes, ot.spaceEfficient))) {
					adsVal += frac_t(p.second->nodes.size() + undistinguishedStates - 1) / (prob * next.size() * (numStates - 1));
					subtreeLen += (currLength * (p.second->nodes.size() + undistinguishedStates));
				} else if (ot.conjecture->getType() == TYPE_DFSM) {
					for (auto& node : p.second->nodes) {
						auto it = p.second->next.find(node->stateOutput);
						if (it == p.second->next.end()) {
							p.second->next.emplace(node->stateOutput, make_shared<ads_t>(node));
						} else {
							it->second->nodes.emplace_back(node);
						}
					}
					for (auto& sp : p.second->next) {
						if ((sp.second->nodes.size() == 1) || (!areDistinguished(sp.second->nodes, ot.spaceEfficient))) {
							adsVal += frac_t(sp.second->nodes.size() + undistinguishedStates - 1) /
								(prob * next.size() * p.second->next.size() * (numStates - 1));
							subtreeLen += (currLength * (sp.second->nodes.size() + undistinguishedStates));
						} else {
							chooseADS(sp.second, ot, bestVal, adsVal, subtreeLen, currLength + 1,
								undistinguishedStates, prob * next.size() * p.second->next.size());
						}
					}
				} else {
					chooseADS(p.second, ot, bestVal, adsVal, subtreeLen, currLength + 1, undistinguishedStates, prob * next.size());
				}
				if (bestVal < adsVal) {// prune
					break;
				}
			}
			if ((adsVal < localBest) || (!(localBest < adsVal) && (minLen > subtreeLen))) {
				localBest = adsVal;
				minLen = subtreeLen;
				ads->input = i;
				ads->next.swap(next);
				if ((prob == 1) && (adsVal < bestVal)) {// update bestVal
					bestVal = adsVal;
				}
			}
		}
		currVal = localBest;
		totalLen += minLen;
	}

	static void chooseSVS(const shared_ptr<ads_t>& svs, const ObservationTree& ot, size_t& bestVal, 
			seq_len_t& minLen, seq_len_t currLength = 1, state_t undistinguishedStates = 0) {
		if ((bestVal == 1) && (currLength >= minLen)) return;
		auto numStates = state_t(svs->nodes.size()) + undistinguishedStates;
		seq_len_t len;
		size_t svsVal;
		for (input_t i = 0; i < ot.conjecture->getNumberOfInputs(); i++) {
			auto& refNode = svs->nodes.front();
			auto idx = getNextIdx(refNode, i);
			if ((idx == STOUT_INPUT) || (!refNode->next[idx])) {
				continue;
			}
			undistinguishedStates = numStates - state_t(svs->nodes.size());
			auto& refOutput = refNode->next[idx]->incomingOutput;
			auto nextSVS = make_shared<ads_t>();
			for (auto& node : svs->nodes) {
				auto idx = getNextIdx(node, i);
				if ((idx == STOUT_INPUT) || (!node->next[idx])) {
					undistinguishedStates++;
				} else if (node->next[idx]->incomingOutput == refOutput) {
					nextSVS->nodes.emplace_back(node->next[idx]);
				}
			}		
			if ((nextSVS->nodes.size() == 1) || (!areDistinguished(nextSVS->nodes, ot.spaceEfficient, true))) {
				svsVal = nextSVS->nodes.size() + undistinguishedStates;
				len = currLength;
			} else if (ot.conjecture->getType() == TYPE_DFSM) {
				auto nextStoutSVS = make_shared<ads_t>();
				auto& refStateOutput = nextSVS->nodes.front()->stateOutput;
				for (auto& node : nextSVS->nodes) {
					if (node->stateOutput == refStateOutput) {
						nextStoutSVS->nodes.emplace_back(node);
					}
				}
				if ((nextStoutSVS->nodes.size() == 1) || (!areDistinguished(nextStoutSVS->nodes, ot.spaceEfficient, true))) {
					svsVal = nextStoutSVS->nodes.size() + undistinguishedStates;
					len = currLength;
				} else {
					svsVal = bestVal;
					len = minLen;
					chooseSVS(nextStoutSVS, ot, svsVal, len, currLength + 1, undistinguishedStates);
				}
				nextSVS->next.emplace(refStateOutput, move(nextStoutSVS));
			} else {
				svsVal = bestVal;
				len = minLen;
				chooseSVS(nextSVS, ot, svsVal, len, currLength + 1, undistinguishedStates);
			}
			if ((svsVal < bestVal) || (!(bestVal < svsVal) && (minLen > len))) {
				bestVal = svsVal;
				minLen = len;
				svs->input = i;
				svs->next.clear();
				svs->next.emplace(refOutput, move(nextSVS));
			}
		}
	}

	static void identify(const shared_ptr<ot_node_t>& node, ObservationTree& ot, const unique_ptr<Teacher>& teacher, shared_ptr<ads_t> ads = nullptr) {
		if (!ads) {
			state_t expectedState = NULL_STATE;
			if (node->extraStateLevel > 1) {
				auto& parent = node->parent.lock();
				expectedState = ot.conjecture->getNextState(*(parent->refStates.begin()), parent->nextInputs[node->incomingInputIdx]);
			}
			ads = make_shared<ads_t>();
			for (auto& state : node->refStates) {
				if (state != expectedState)
					ads->nodes.emplace_back(ot.stateNodes[state]);
				else
					ads->nodes.push_front(ot.stateNodes[state]);
			}
			if (node->refStates.size() == 1) {
				ads->input = node->parent.lock()->nextInputs[node->incomingInputIdx];
			}
			else if (expectedState != NULL_STATE) {
				size_t bestVal(node->refStates.size());
				seq_len_t len(-1);
				chooseSVS(ads, ot, bestVal, len);
			}
			else {
				seq_len_t totalLen = 0;
				frac_t bestVal(1), currVal(0);
				chooseADS(ads, ot, bestVal, currVal, totalLen);
			}
		}
		// simulate ADS
		auto currNode = node;
		auto idx = getNextIdx(currNode, ads->input);
		while (idx != STOUT_INPUT) {
			if (!currNode->next[idx]) {
				query(currNode, idx, ot, teacher);
			}
			currNode = currNode->next[idx];
			auto it = ads->next.find(currNode->incomingOutput);
			if ((it == ads->next.end()) || (it->second->next.empty())) {
				break;
			}
			ads = it->second;
			if (ot.conjecture->getType() == TYPE_DFSM) {
				it = ads->next.find(currNode->stateOutput);
				if ((it == ads->next.end()) || (it->second->input == STOUT_INPUT)) {
					break;
				}
				ads = it->second;
			}
			idx = getNextIdx(currNode, ads->input);
		}
		checkAndQueryNext(currNode, ot, teacher);
	}

	static shared_ptr<ads_t> getADSwithFixedPrefix(shared_ptr<ot_node_t> node, state_t expectedState, ObservationTree& ot) {
		auto ads = make_shared<ads_t>();
		for (auto& state : node->refStates) {
			if (state != expectedState)
				ads->nodes.emplace_back(ot.stateNodes[state]);
			else
				ads->nodes.push_front(ot.stateNodes[state]);
		}
		auto currAds = ads;
		while (node->distInputIdx != STOUT_INPUT) {
			currAds->input = node->nextInputs[node->distInputIdx];
			node = node->next[node->distInputIdx];
			if (expectedState != NULL_STATE) {
				auto& refNode = currAds->nodes.front();
				auto idx = getNextIdx(refNode, currAds->input);
				if ((idx == STOUT_INPUT) || (!refNode->next[idx])) {
					return nullptr;
				}
			}
			auto nextADS = make_shared<ads_t>();
			for (auto& nn : currAds->nodes) {
				auto idx = getNextIdx(nn, currAds->input);
				if ((idx != STOUT_INPUT) && nn->next[idx]) {
					nextADS->nodes.emplace_back(nn->next[idx]);
				}
			}
			currAds->next.emplace(node->incomingOutput, nextADS);
			currAds = nextADS;
		}
		if ((currAds->nodes.size() == 1) || (!areDistinguished(currAds->nodes, ot.spaceEfficient, (expectedState != NULL_STATE)))) {
			return nullptr;
		}
		if (expectedState != NULL_STATE) {
			size_t bestVal(currAds->nodes.size());
			seq_len_t len(-1);
			chooseSVS(currAds, ot, bestVal, len);
		} else {
			seq_len_t totalLen = 0;
			frac_t bestVal(1), currVal(0);
			chooseADS(currAds, ot, bestVal, currVal, totalLen);
		}
		return ads;
	}

	static void tryExtendQueriedPath(shared_ptr<ot_node_t> node, ObservationTree& ot, const unique_ptr<Teacher>& teacher) {
		while (node->state == NULL_STATE) {// find the lowest state node
			node = node->parent.lock();
		}
		if (node->distInputIdx == STOUT_INPUT) {//the state node is leaf
			if (!node->nextInputs.empty()) {
				// apply
				query(node, 0, ot, teacher);
				if (ot.conjecture->isOutputState() && node->next[0]->refStates.empty()) {// new state
					return checkAndQueryNext(node->next[0], ot, teacher);
				}
				return identify(node->next[0], ot, teacher);
			}
		}
		else {
			auto level = node->extraStateLevel;
			auto state = node->state;
			while ((node->distInputIdx != STOUT_INPUT) && (level <= ot.numberOfExtraStates)) {
				if (state != NULL_STATE) {
					auto idx = getNextIdx(ot.stateNodes[state], node->nextInputs[node->distInputIdx]);
					if (ot.stateNodes[state]->next[idx]->refStates.size() == 1) {
						state = *(ot.stateNodes[state]->next[idx]->refStates.begin());
					} else state = NULL_STATE;
				}
				node = node->next[node->distInputIdx];
				if (node->refStates.size() > 1) {
					auto ads = getADSwithFixedPrefix(node, state, ot);
					if (ads) {
						return identify(node, ot, teacher, ads);
					}
				}
				node->extraStateLevel = ++level;
			}
		}
	}

	unique_ptr<DFSM> ObservationTreeAlgorithm(const unique_ptr<Teacher>& teacher, state_t maxExtraStates, bool isEQallowed,
		function<bool(const unique_ptr<DFSM>& conjecture)> provideTentativeModel) {
		if (!teacher->isBlackBoxResettable()) {
			ERROR_MESSAGE("FSMlearning::ObservationTreeAlgorithm - the Black Box needs to be resettable");
			return nullptr;
		}

		/// Observation Tree
		ObservationTree ot;
		ot.conjecture = FSMmodel::createFSM(teacher->getBlackBoxModelType(), 1, teacher->getNumberOfInputs(), teacher->getNumberOfOutputs());
		teacher->resetBlackBox();
		auto node = make_shared<ot_node_t>(teacher->getNextPossibleInputs()); // root
		ot.stateNodes.emplace_back(node);
		node->state = 0;
		node->refStates.emplace(0);
		node->extraStateLevel = 0;
		if (ot.conjecture->isOutputState()) {
			node->stateOutput = teacher->outputQuery(STOUT_INPUT);
			checkNumberOfOutputs(teacher, ot.conjecture);
			ot.conjecture->setOutput(0, node->stateOutput);
		}
		ot.uncheckedNodes.emplace_back(node);
		ot.bbNode = node;
		ot.numberOfExtraStates = 0;

		while (!ot.uncheckedNodes.empty()) {
			node = ot.uncheckedNodes.front();
			if (node->extraStateLevel > ot.numberOfExtraStates) {
				ot.numberOfExtraStates++;
				if (ot.numberOfExtraStates > maxExtraStates) {
					if (isEQallowed) {
						auto ce = teacher->equivalenceQuery(ot.conjecture);
						if (!ce.empty()) {
							ot.numberOfExtraStates--;
							auto currNode = ot.stateNodes[0];// root
							for (auto& input : ce) {
								if (input == STOUT_INPUT) continue;
								auto idx = getNextIdx(currNode, input);
								if (!currNode->next[idx]) {
									query(currNode, idx, ot, teacher);
								}
								currNode = currNode->next[idx];
							}
							checkAndQueryNext(currNode, ot, teacher);
							continue;
						}
					}
					break;
				}
			}
			auto nextNode = node;
			for (input_t i = 0; i < node->nextInputs.size(); i++) {
				if (!node->next[i]) {
					// apply
					query(node, i, ot, teacher);
					nextNode = node->next[i];
					if (node->state != NULL_STATE) {
						if (ot.conjecture->isOutputState() && nextNode->refStates.empty()) {// new state
							checkAndQueryNext(nextNode, ot, teacher);
							nextNode = nullptr;
						}
					} else if (!checkResponse(node, i, ot)) {// new state
						checkAndQueryNext(nextNode, ot, teacher);
						nextNode = nullptr;
					}
					break;
				}
				if ((nextNode == node) && (node->next[i]->refStates.size() > 1)) {
					nextNode = node->next[i];// the first unconfirmed next node
					nextNode->extraStateLevel = node->extraStateLevel + 1;
				}
			}
			if (nextNode == node) {// node checked
				for (auto& nn : node->next) {
					if (nn->state == NULL_STATE) {
						nn->extraStateLevel = node->extraStateLevel + 1;
						ot.uncheckedNodes.emplace_back(nn);
					}
				}
				ot.uncheckedNodes.pop_front();
				continue;
			}
			if (nextNode) {// new state not found
				// identify or confirm
				identify(nextNode, ot, teacher);
			}
			if (provideTentativeModel) {
				if (!provideTentativeModel(ot.conjecture)) break;
			}
		}
		return move(ot.conjecture);
	}
}
