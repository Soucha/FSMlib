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

		set<ot_node_t*> consistentNodes;
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
			|| (n1->nextInputs != n2->nextInputs) || (n2->consistentNodes.find(n1.get()) == n2->consistentNodes.end());
	}
	
	static void findConsistentNodes(const shared_ptr<ot_node_t>& node, ObservationTree& ot) {
		stack<shared_ptr<ot_node_t>> nodes;
		for (const auto& sn : ot.stateNodes) {
			nodes.emplace(sn);
			while (!nodes.empty()) {
				auto otNode = move(nodes.top());
				nodes.pop();
				if ((node != otNode) && (node->stateOutput == otNode->stateOutput) && (node->nextInputs == otNode->nextInputs)) {
					node->consistentNodes.emplace(otNode.get());
					otNode->consistentNodes.emplace(node.get());
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
			
			auto currNode = node;
			state_t level = 0;
			while ((currNode->distInputIdx != STOUT_INPUT) && (level < ot.numberOfExtraStates)) {
				currNode = currNode->next[currNode->distInputIdx];
				if (currNode->state != NULL_STATE) break;
				currNode->extraStateLevel = ++level;
			}
			
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
		}
		/*
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
				for (auto snIt = node->refStates.begin(); snIt != node->refStates.end();) {
					if (areNodesDifferentUnder(node, ot.stateNodes[*snIt])) {
						snIt = node->refStates.erase(snIt);
					} else ++snIt;
				}
			} else {
				for (auto cnIt = node->consistentNodes.begin(); cnIt != node->consistentNodes.end();) {
					if ((*cnIt)->next[idx] && (areNextNodesDifferent(node->next[idx], (*cnIt)->next[idx]))) {
						(*cnIt)->consistentNodes.erase(node.get());
						if (node->state != NULL_STATE) {
							(*cnIt)->refStates.erase(node->state);
							auto parent = (*cnIt)->parent.lock();// it must have parent
							auto cnPtr = parent->next[(*cnIt)->incomingInputIdx];
							checkNode(cnPtr, ot);
						}
						else if ((*cnIt)->state != NULL_STATE) {
							node->refStates.erase((*cnIt)->state);
						}
						cnIt = node->consistentNodes.erase(cnIt);
					}
					else ++cnIt;
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

	static void checkAndQueryNext(const shared_ptr<ot_node_t>& node, ObservationTree& ot) {
		checkPrevious(node, ot);
		
		//can we query anything from currNode ? ;

		if (ot.uncheckedNodes.empty()) {
			ot.uncheckedNodes.assign(ot.stateNodes.begin(), ot.stateNodes.end());
		}
	}

	typedef pair<size_t, size_t> frac_t;

	static frac_t fracSum(const frac_t& f1, const frac_t& f2) {
		if (f1.second == f2.second)
			return frac_t(f1.first + f2.first, f1.second);
		if (f2.second % f1.second == 0)
			return frac_t(f1.first * f2.second / f1.second + f2.first, f2.second);
		if (f1.second % f2.second == 0)
			return frac_t(f2.first * f1.second / f2.second + f1.first, f1.second);
		return frac_t(f1.first * f2.second + f1.second * f2.first, f1.second * f2.second);
	}

	static bool fracLess(const frac_t& f1, const frac_t& f2) {
		return f1.first * f2.second < f2.first * f1.second;
	}

	static bool areDistinguished(vector<shared_ptr<ot_node_t>>& nodes, bool spaceEfficient) {
		for (auto it1 = nodes.begin(); it1 != nodes.end(); ++it1) {
			auto it2 = it1;
			for (++it2; it2 != nodes.end(); ++it2) {
				if (spaceEfficient) {
					if (areNodesDifferent(*it1, *it2)) return true;
				} else {
					if ((*it1)->consistentNodes.size() < (*it2)->consistentNodes.size()) {
						if ((*it1)->consistentNodes.find((*it2).get()) == (*it1)->consistentNodes.end()) return true;
					}
					else if ((*it2)->consistentNodes.find((*it1).get()) == (*it2)->consistentNodes.end()) return true;
				}
			}
		}
		return false;
	}

	static void chooseADS(vector<shared_ptr<ot_node_t>>& nodes, const ObservationTree& ot, frac_t& bestVal, frac_t& currVal,
			seq_len_t& totalLen, seq_len_t currLength = 1, state_t undistinguishedStates = 0, size_t prob = 1) {
		auto numStates = nodes.size() + undistinguishedStates;
		frac_t localBest(1, 1);
		seq_len_t subtreeLen, minLen = seq_len_t(-1);
		for (input_t i = 0; i < ot.conjecture->getNumberOfInputs(); i++) {
			map<output_t, vector<shared_ptr<ot_node_t>>> next;
			undistinguishedStates = numStates - nodes.size();
			for (auto& node : nodes) {
				auto idx = getNextIdx(node, i);
				if ((idx == STOUT_INPUT) || (!node->next[idx])) {
					undistinguishedStates++;
				} else {
					auto it = next.find(node->next[idx]->incomingOutput);
					if (it == next.end()) {
						next.emplace(node->next[idx]->incomingOutput, vector<shared_ptr<ot_node_t>>({ node->next[idx] }));
					}
					else {
						it->second.emplace_back(node->next[idx]);
					}
				}
			}
			if (next.empty()) continue;
			auto adsVal = currVal;
			subtreeLen = 0;
			for (auto p : next) {
				if ((p.second.size() == 1) || (!areDistinguished(p.second, ot.spaceEfficient))) {
					p.second.front()->distInputIdx = STOUT_INPUT;
					adsVal = fracSum(adsVal, frac_t(p.second.size() + undistinguishedStates - 1, prob * next.size() * (numStates - 1)));
					subtreeLen += (currLength * (p.second.size() + undistinguishedStates));// (currLength * p.second.size());//
				} else {
					chooseADS(p.second, ot, bestVal, adsVal, subtreeLen, currLength + 1, undistinguishedStates, prob * next.size());
				}
				if (fracLess(bestVal, adsVal)) {// prune
					break;
				}
			}
			if (fracLess(adsVal, localBest) || (!fracLess(localBest, adsVal) && (minLen > subtreeLen))) {
				localBest = adsVal;
				minLen = subtreeLen;
				nodes.front()->distInputIdx = i;
				if ((prob == 1) && fracLess(adsVal, bestVal)) {// update bestVal
					bestVal = adsVal;
				}
			}
		}
		currVal = localBest;
		totalLen += minLen;
	}

	static void identify(shared_ptr<ot_node_t>& node, ObservationTree& ot, const unique_ptr<Teacher>& teacher) {
		state_t expectedState = NULL_STATE;
		if (node->extraStateLevel > 1) {
			auto& parent = node->parent.lock();
			expectedState = ot.conjecture->getNextState(*(parent->refStates.begin()), parent->nextInputs[node->incomingInputIdx]);
		}
		vector<shared_ptr<ot_node_t>> nodes;
		for (auto& state : node->refStates) {
			nodes.emplace_back(ot.stateNodes[state]);
		}
		if (node->refStates.size() == 1) {
			nodes.front()->distInputIdx = node->parent.lock()->nextInputs[node->incomingInputIdx];
		} else {
			seq_len_t totalLen = 0;
			chooseADS(nodes, ot, frac_t(1, 1), frac_t(0, 1), totalLen);
		}
		// simulate ADS
		auto currNode = node;
		auto idx = getNextIdx(currNode, nodes.front()->distInputIdx);
		//if (idx == STOUT_INPUT) newState;
		while (idx != STOUT_INPUT) {
			if (!currNode->next[idx]) {
				query(currNode, idx, ot, teacher);
			}
			currNode = currNode->next[idx];
			vector<shared_ptr<ot_node_t>> next;
			for (auto& node : nodes) {
				auto idx = getNextIdx(node, nodes.front()->distInputIdx);
				if ((idx == STOUT_INPUT) || (!node->next[idx])) {
					continue;
				}
				if (node->next[idx]->incomingOutput == currNode->incomingOutput) {
					next.emplace_back(node->next[idx]);
				}
			}
			if (next.empty() || (next.front()->distInputIdx == STOUT_INPUT)) {
				break;
			}
			nodes.swap(next);
			/*
			if ((expectedState != NULL_STATE) &&
				(find(adsDec->initialStates.begin(), adsDec->initialStates.end(), expectedState) == adsDec->initialStates.end())) {
				break;
			}*/
			idx = getNextIdx(currNode, nodes.front()->distInputIdx);
			//if (idx == STOUT_INPUT) newState;
		}
		checkAndQueryNext(currNode, ot);
	}

	unique_ptr<DFSM> ObservationTreeAlgorithm(const unique_ptr<Teacher>& teacher, state_t maxExtraStates,
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
				if (ot.numberOfExtraStates > maxExtraStates) break;
			}
			auto nextNode = node;
			for (input_t i = 0; i < node->nextInputs.size(); i++) {
				if (!node->next[i]) {
					// apply
					query(node, i, ot, teacher);
					nextNode = node->next[i];
					if (node->state != NULL_STATE) {
						if (ot.conjecture->isOutputState() && nextNode->refStates.empty()) {// new state
							checkAndQueryNext(nextNode, ot);
							nextNode = nullptr;
						}
					} else if (!checkResponse(node, i, ot)) {// new state
						checkAndQueryNext(nextNode, ot);
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
