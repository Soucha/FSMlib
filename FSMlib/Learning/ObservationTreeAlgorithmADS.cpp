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

	struct ot_ads_t;
	struct ads_node_t {
		vector<state_t> initialStates;
		vector<shared_ptr<ot_node_t>> currentNode;
		input_t currIdx;
		vector<shared_ptr<ot_ads_t>> next;
	};

	struct ot_ads_t {
		input_t input;
		map<output_t, shared_ptr<ads_node_t>> decision;

		ot_ads_t(input_t input) : input(input) {}
	};

	struct ObservationTree {
		vector<shared_ptr<ot_node_t>> stateNodes;
		shared_ptr<ot_node_t> bbNode;
		list<shared_ptr<ot_node_t>> uncheckedNodes;
		shared_ptr<ot_ads_t> ads;
		state_t numberOfExtraStates;
		unique_ptr<DFSM> conjecture;
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

	static bool areNextNodesDifferent(const shared_ptr<ot_node_t>& n1, const shared_ptr<ot_node_t>& n2) {
		return (n1->stateOutput != n2->stateOutput) || (n1->incomingOutput != n2->incomingOutput)
			|| (n1->nextInputs != n2->nextInputs) || (n2->consistentNodes.find(n1.get()) == n2->consistentNodes.end());
	}
	
	static vector<shared_ptr<ot_ads_t>> createADSforeachInput(const input_t& numInputs) {
		vector<shared_ptr<ot_ads_t>> ads;
		for (input_t input = 0; input < numInputs; input++) {
			ads.emplace_back(make_shared<ot_ads_t>(input));
		}
		return ads;
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

	static shared_ptr<ads_node_t> updateADSdecision(const shared_ptr<ot_ads_t>& ads, const state_t& refState, const shared_ptr<ot_node_t>& node) {
		auto it = ads->decision.find(node->incomingOutput);
		if (it == ads->decision.end()) {
			auto adsDec = make_shared<ads_node_t>();
			adsDec->initialStates.push_back(refState);
			adsDec->currentNode.emplace_back(node);
			adsDec->currIdx = STOUT_INPUT;
			ads->decision.emplace(node->incomingOutput, adsDec);
			return adsDec;
		}
		it->second->initialStates.emplace_back(refState);
		it->second->currentNode.emplace_back(node);
		return it->second;
	}

	static void updateADSwithState(shared_ptr<ot_ads_t> ads, const state_t& refState, const shared_ptr<ot_node_t>& node) {
		auto idx = getNextIdx(node, ads->input);
		//if (idx == STOUT_INPUT);
		if (node->next[idx]) {
			auto adsDec = updateADSdecision(ads, refState, node->next[idx]);
			for (auto& nn : adsDec->next) {
				updateADSwithState(nn, refState, node->next[idx]);
			}
		}
	}

	static void addADS(shared_ptr<ot_node_t> node, ObservationTree& ot) {
		state_t refState = node->state;
		if (node->distInputIdx == STOUT_INPUT) {
			auto adsDec = make_shared<ads_node_t>();
			adsDec->initialStates.push_back(refState);
			adsDec->currentNode.emplace_back(node);
			adsDec->next = createADSforeachInput(ot.conjecture->getNumberOfInputs());
			ot.ads->decision.emplace(node->stateOutput, move(adsDec));
			return;
		}
		auto& adsVec = ot.ads->decision.at(node->stateOutput)->next;
		for (input_t i = 0; i < node->nextInputs.size(); i++) {
			if (node->next[i] && (i != node->distInputIdx)) {
				updateADSwithState(adsVec[node->nextInputs[i]], refState, node);
			}
		}
		auto ads = adsVec[node->nextInputs[node->distInputIdx]];
		do {
			node = node->next[node->distInputIdx];
			auto adsDec = updateADSdecision(ads, refState, node);
			if (adsDec->initialStates.size() == 1) {// new node
				break;
			}
			if (node->distInputIdx == STOUT_INPUT) {
				break; // the end of the new sequence
			}
			ads = nullptr;
			for (auto& nn : adsDec->next) {
				if (nn->input == node->nextInputs[node->distInputIdx]) {
					ads = nn;
				}
				else {
					updateADSwithState(nn, refState, node);
				}
			}
			if (!ads) {
				auto& input = node->nextInputs[node->distInputIdx];
				ads = make_shared<ot_ads_t>(input);
				for (state_t sIdx = 0; sIdx < adsDec->initialStates.size() - 1; sIdx++) {// all but the current refState
					updateADSwithState(ads, adsDec->initialStates[sIdx], adsDec->currentNode[sIdx]);
				}
				adsDec->next.emplace_back(ads);
			}

		} while (node->distInputIdx != STOUT_INPUT);
	}

	static void checkADS(shared_ptr<ot_node_t> node, ObservationTree& ot) {
		state_t refState = node->state;
		auto ads = ot.ads->decision.at(node->stateOutput)->next[node->nextInputs[node->distInputIdx]];
		do {
			node = node->next[node->distInputIdx];
			auto it = ads->decision.find(node->incomingOutput);
			if (it == ads->decision.end()) {
				auto adsDec = make_shared<ads_node_t>();
				adsDec->initialStates.push_back(refState);
				adsDec->currentNode.emplace_back(node);
				ads->decision.emplace(node->incomingOutput, adsDec);
				return;
			}
			auto& adsDec = it->second;
			if (adsDec->initialStates.end() == find(adsDec->initialStates.begin(), adsDec->initialStates.end(), refState)) {
				adsDec->initialStates.emplace_back(refState);
				adsDec->currentNode.emplace_back(node);
			}
			if (node->distInputIdx == STOUT_INPUT) {
				return;
			}
			ads = nullptr;
			auto& input = node->nextInputs[node->distInputIdx];
			for (auto& nn : adsDec->next) {
				if (nn->input == input) {
					ads = nn;
					break;
				}
			}
			if (!ads) {
				for (state_t sIdx = 0; sIdx < adsDec->initialStates.size(); sIdx++) {// all but the current refState
					if (adsDec->currentNode[sIdx] != node) {
						auto cnIdx = getNextIdx(adsDec->currentNode[sIdx], input);
						//if (cnIdx == STOUT_INPUT) continue;
						auto& nn = adsDec->currentNode[sIdx]->next[cnIdx];
						if (nn && areNextNodesDifferent(node->next[node->distInputIdx], nn)) {
							ads = make_shared<ot_ads_t>(input);
							break;
						}
					}
				}
				if (ads) {
					for (state_t sIdx = 0; sIdx < adsDec->initialStates.size(); sIdx++) {
						updateADSwithState(ads, adsDec->initialStates[sIdx], adsDec->currentNode[sIdx]);
					}
					adsDec->next.emplace_back(ads);
				}
			}
		} while (ads && (node->distInputIdx != STOUT_INPUT));
	}

	static void checkNode(const shared_ptr<ot_node_t>& node, ObservationTree& ot) {
		if (node->refStates.empty()) {// new state
			node->state = ot.conjecture->addState(node->stateOutput);
			node->extraStateLevel = 0;
			ot.stateNodes.emplace_back(node);
			for (auto& cn : node->consistentNodes) {
				cn->refStates.emplace(node->state);
			}
			node->refStates.emplace(node->state);
			
			auto currNode = node;
			state_t level = 0;
			while ((currNode->distInputIdx != STOUT_INPUT) && (level < ot.numberOfExtraStates)) {
				currNode = currNode->next[currNode->distInputIdx];
				if (currNode->state != NULL_STATE) break;
				currNode->extraStateLevel = ++level;
			}
			addADS(node, ot);

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
			checkADS(node, ot);

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
			for (auto cnIt = node->consistentNodes.begin(); cnIt != node->consistentNodes.end();) {
				if ((*cnIt)->next[idx] && (areNextNodesDifferent(node->next[idx], (*cnIt)->next[idx]))) {
					(*cnIt)->consistentNodes.erase(node.get());
					if (node->state != NULL_STATE) {// is this possible? there needs to be the same path from the consistent node and the path was not from this stateNode
						(*cnIt)->refStates.erase(node->state);
						auto parent = (*cnIt)->parent.lock();// it must have parent
						auto cnPtr = parent->next[(*cnIt)->incomingInputIdx];
						// update distinguishing input
						auto nn = node;
						auto cnNextPtr = cnPtr;
						do {
							auto cnIdx = getNextIdx(cnNextPtr, nn->nextInputs[nn->distInputIdx]);
							//if (cnIdx == STOUT_INPUT);
							if (!cnNextPtr->next[cnIdx]) break;
							cnNextPtr->distInputIdx = cnIdx;
							nn = nn->next[nn->distInputIdx];
							cnNextPtr = cnNextPtr->next[cnIdx];
						} while (nn->distInputIdx != STOUT_INPUT);

						checkNode(cnPtr, ot);
						parent->distInputIdx = cnPtr->incomingInputIdx;
						checkPrevious(parent, ot);
					}
					else if ((*cnIt)->state != NULL_STATE) {
						node->refStates.erase((*cnIt)->state);
					}
					cnIt = node->consistentNodes.erase(cnIt);
				}
				else ++cnIt;
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
			// updata ot.ads
			/*
			for (input_t input = conjecture->getNumberOfInputs(); input < teacher->getNumberOfInputs(); input++) {
			sequence_in_t seq({ input });
			if (conjecture->isOutputState()) seq.emplace_back(STOUT_INPUT);
			pset.insert(move(seq));
			}*/
			ot.conjecture->incNumberOfInputs(teacher->getNumberOfInputs() - ot.conjecture->getNumberOfInputs());
		}
		node->next[idx] = leaf;
		findConsistentNodes(leaf, ot);
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

	static frac_t evaluateADS(const shared_ptr<ot_ads_t>& ads, vector<shared_ptr<ot_node_t>>& nodes, state_t undistinguishedStates,
			seq_len_t& totalLen, seq_len_t currLength) {
		auto numStates = nodes.size() + undistinguishedStates;
		map<output_t, vector<shared_ptr<ot_node_t>>> next;
		for (auto& node : nodes) {
			auto idx = getNextIdx(node, ads->input);
			if (node->next[idx]) {
				auto it = next.find(node->next[idx]->incomingOutput);
				if (it == next.end()) {
					next.emplace(node->next[idx]->incomingOutput, vector<shared_ptr<ot_node_t>>({ node->next[idx] }));
				} else {
					it->second.emplace_back(node->next[idx]);
				}
			} else undistinguishedStates++;
		}
		frac_t adsVal(0, 1);
		for (auto p : next) {
			auto& adsDec = ads->decision.at(p.first);
			if (adsDec->next.empty() || (p.second.size() == 1)) {
				adsDec->currIdx = STOUT_INPUT;
				adsVal = fracSum(adsVal, frac_t(numStates - p.second.size() - undistinguishedStates, next.size() * (numStates - 1)));
				totalLen += (currLength * (p.second.size() + undistinguishedStates));// (currLength * p.second.size());//
			} else {// choose the best next input for ADS
				frac_t bestVal(0, 1);
				seq_len_t subtreeLen, minLen = seq_len_t(-1);
				for (input_t i = 0; i < adsDec->next.size(); i++) {
					subtreeLen = 0;
					auto val = evaluateADS(adsDec->next[i], p.second, undistinguishedStates, subtreeLen, currLength + 1);
					if (fracLess(bestVal, val) || (!fracLess(val, bestVal) && (minLen > subtreeLen))) {
						bestVal = val;
						minLen = subtreeLen;
						adsDec->currIdx = i;
					}
				}
				bestVal.second *= next.size();
				adsVal = fracSum(adsVal, bestVal);
				totalLen += minLen;
			}
		}
		return adsVal;
	}

	static shared_ptr<ot_ads_t> chooseADS(const shared_ptr<ot_node_t>& node, state_t expectedState, const ObservationTree& ot) {
		auto& adsVec = ot.ads->decision.at(node->stateOutput)->next;
		input_t prevInput = node->parent.lock()->nextInputs[node->incomingInputIdx];
		if (node->refStates.size() == 1) {
			return adsVec[prevInput];
		}
		shared_ptr<ot_ads_t> ads;
		frac_t maxVal(0, 1);
		seq_len_t totalLen, minLen = seq_len_t(-1);
		vector<shared_ptr<ot_node_t>> nodes;
		for (auto& state : node->refStates) {
			nodes.emplace_back(ot.stateNodes[state]);
		}
		for (auto& currADS : adsVec) {
			totalLen = 0;
			auto val = evaluateADS(currADS, nodes, 0, totalLen, 1);
			if (fracLess(maxVal, val) || (!fracLess(val, maxVal) && ((minLen > totalLen)
				|| ((minLen == totalLen) && (currADS->input == prevInput))))) {
				maxVal = val;
				minLen = totalLen;
				ads = currADS;
			}
		}
		return ads;
	}

	static void identify(shared_ptr<ot_node_t>& node, ObservationTree& ot, const unique_ptr<Teacher>& teacher) {
		state_t expectedState = NULL_STATE;
		if (node->extraStateLevel > 1) {
			auto& parent = node->parent.lock();
			expectedState = ot.conjecture->getNextState(*(parent->refStates.begin()), parent->nextInputs[node->incomingInputIdx]);
		}
		auto ads = chooseADS(node, expectedState, ot);
		// simulate ADS
		auto currNode = node;
		auto idx = getNextIdx(currNode, ads->input);
		//if (idx == STOUT_INPUT) newState;
		while (idx != STOUT_INPUT) {
			if (!currNode->next[idx]) {
				query(currNode, idx, ot, teacher);
			}
			currNode = currNode->next[idx];
			auto it = ads->decision.find(currNode->incomingOutput);
			if (it == ads->decision.end()) {
				break;
			}
			auto& adsDec = it->second;
			if (adsDec->currIdx == STOUT_INPUT) {
				break;
			}
			if ((expectedState != NULL_STATE) &&
				(find(adsDec->initialStates.begin(), adsDec->initialStates.end(), expectedState) == adsDec->initialStates.end())) {
				break;
			}
			ads = adsDec->next[adsDec->currIdx];
			idx = getNextIdx(currNode, ads->input);
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
		// prepare distinguishing structures
		auto adsDec = make_shared<ads_node_t>();
		adsDec->initialStates.push_back(0);
		adsDec->currentNode.emplace_back(node);
		adsDec->next = createADSforeachInput(ot.conjecture->getNumberOfInputs());
		ot.ads = make_shared<ot_ads_t>(STOUT_INPUT);
		ot.ads->decision.emplace(node->stateOutput, move(adsDec));

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
						auto& ads = ot.ads->decision.at(node->stateOutput)->next[node->nextInputs[i]];
						updateADSdecision(ads, node->state, nextNode);
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
					nextNode = node->next[i];// the last unconfirmed next node
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

/*

static bool areNodesDifferent(const shared_ptr<ot_node_t>& n1, const shared_ptr<ot_node_t>& n2) {
if ((n1->stateOutput != n2->stateOutput) || (n1->nextInputs != n2->nextInputs)) return true;
for (input_t i = 0; i < n1->nextInputs.size(); i++) {
if ((n1->next[i]) && (n2->next[i]) && ((n1->next[i]->incomingOutput != n2->next[i]->incomingOutput)
|| areNodesDifferent(n1->next[i], n2->next[i])))
return true;
}
return false;
}

static bool areStateDomainsDifferent(const shared_ptr<ot_node_t>& n1, const shared_ptr<ot_node_t>& n2) {
if (n1->refStates.empty())  {
if (n2->refStates.empty()) // n1, n2 will be two new states, or the same one
return (n1->consistentNodes.find(n2.get()) == n1->consistentNodes.end());
return true; // n1 will be a new state
} else if (n2->refStates.empty()) return true;
return !any_of(n1->refStates.begin(), n1->refStates.end(), [&](state_t state){
return (n2->refStates.find(state) != n2->refStates.end());
});
}

static bool checkDomainsUnder(const shared_ptr<ot_node_t>& node, input_t idx, ObservationTree& ot,
function<bool(const shared_ptr<ot_node_t>&, const shared_ptr<ot_node_t>&)> areDifferent) {
bool checkPrevious = false, newStateFound = false;
for (auto cnIt = node->consistentNodes.begin(); cnIt != node->consistentNodes.end();) {
if ((*cnIt)->next[idx] && (areDifferent(node->next[idx], (*cnIt)->next[idx]))) {
(*cnIt)->consistentNodes.erase(node.get());
if (node->state != NULL_STATE) {// is this possible? there needs to be the same path from the consistent node and the path was not from this stateNode
(*cnIt)->refStates.erase(node->state);
auto cnPtr = shared_ptr<ot_node_t>(*cnIt);
if (cnPtr->parent.lock())// && (node->state == NULL_STATE)) // if the parent is not root and the node is not state
newStateFound |= checkDomainsUnder(cnPtr->parent.lock(), cnPtr->incomingInputIdx, ot, areDomainsDifferent);
newStateFound |= checkNode(cnPtr, ot);
} else if ((*cnIt)->state != NULL_STATE) {
node->refStates.erase((*cnIt)->state);
checkPrevious = true;
}
cnIt = node->consistentNodes.erase(cnIt);
}
else ++cnIt;
}
//if (checkPrevious) {
if (node->parent.lock())// && (node->state == NULL_STATE)) // if the parent is not root and the node is not state
newStateFound |= checkDomainsUnder(node->parent.lock(), node->incomingInputIdx, ot, areDomainsDifferent);
//}
newStateFound |= checkNode(node, ot);
return newStateFound;
}

// not used
static void query(shared_ptr<ot_node_t> node, sequence_in_t& seq, ObservationTree& ot, const unique_ptr<Teacher>& teacher) {

	if (seq.front() == STOUT_INPUT) seq.pop_front();
	auto idx = getNextIdx(node, seq.front());
	while (!node->next[idx]) {
		node = node->next[idx];
		seq.pop_front();
		if (seq.front() == STOUT_INPUT) seq.pop_front();
		idx = getNextIdx(node, seq.front());
	}
	// prepare teacher

	bool newStateFound = false;
	while (!newStateFound && !seq.empty()) {
		if (seq.front() == STOUT_INPUT) {
			seq.pop_front();
			continue;
		}
		idx = getNextIdx(node, seq.front());
		if (idx == STOUT_INPUT) break;//cannot be applied
		state_t transitionOutput, stateOutput = DEFAULT_OUTPUT;
		if (!teacher->isProvidedOnlyMQ() && (ot.conjecture->getType() == TYPE_DFSM)) {
			sequence_in_t suffix({ seq.front(), STOUT_INPUT });
			auto output = teacher->outputQuery(suffix);
			transitionOutput = output.front();
			stateOutput = output.back();
		}
		else {
			transitionOutput = teacher->outputQuery(seq.front());
			if (ot.conjecture->getType() == TYPE_DFSM) {
				stateOutput = teacher->outputQuery(STOUT_INPUT);
			}
			else if (!ot.conjecture->isOutputTransition()) {// Moore, DFA
				stateOutput = transitionOutput;
				transitionOutput = DEFAULT_OUTPUT;
			}
		}
		//if (node->state != NULL_STATE) updateADS(node, seq.front(), ot);
		checkNumberOfOutputs(teacher, ot.conjecture);
		auto leaf = make_shared<ot_node_t>(node, seq.front(), idx, transitionOutput, stateOutput, teacher->getNextPossibleInputs());
		seq.pop_front();
		if (ot.conjecture->getNumberOfInputs() != teacher->getNumberOfInputs()) {
			/*
			for (input_t input = conjecture->getNumberOfInputs(); input < teacher->getNumberOfInputs(); input++) {
			sequence_in_t seq({ input });
			if (conjecture->isOutputState()) seq.emplace_back(STOUT_INPUT);
			pset.insert(move(seq));
			}* /
			ot.conjecture->incNumberOfInputs(teacher->getNumberOfInputs() - ot.conjecture->getNumberOfInputs());
		}
		node->next[idx] = leaf;
		//if (conjecture->isOutputState()) {
		findConsistentNodes(leaf, ot, [](const shared_ptr<ot_node_t>& n1, const shared_ptr<ot_node_t>& n2){
			return ((n1->stateOutput == n2->stateOutput) && (n1->nextInputs == n2->nextInputs)); });
		newStateFound |= checkDomainsUnder(node, idx, ot, [](const shared_ptr<ot_node_t>& n1, const shared_ptr<ot_node_t>& n2){
			return ((n1->stateOutput != n2->stateOutput) || (n1->incomingOutput != n2->incomingOutput)
				|| (n1->nextInputs != n2->nextInputs)); });
		//newStateFound |= checkNode(leaf, ot);
		/*
		} else {// Mealy
		if (node->consistentNodes.empty()) {
		findConsistentNodes(node, ot, [&](const shared_ptr<ot_node_t>& n1, const shared_ptr<ot_node_t>& n2){
		return ((n1->nextInputs == n2->nextInputs) && (!n2->next[idx] ||
		((n2->next[idx]->incomingOutput == leaf->incomingOutput) && (n2->next[idx]->nextInputs == leaf->nextInputs)))); });
		if (node->parent.lock() && node->state == NULL_STATE) // if the parent is not root and the node is not state
		checkDomainsUnder(node->parent.lock(), node->incomingInputIdx, ot, areDomainsDifferent);
		} else {
		checkDomainsUnder(node, idx, ot, [](const shared_ptr<ot_node_t>& n1, const shared_ptr<ot_node_t>& n2){
		return ((n1->stateOutput != n2->stateOutput) || (n1->incomingOutput != n2->incomingOutput)
		|| (n1->nextInputs != n2->nextInputs)); });
		}
		checkNode(node, ot);
		}* /


		node = leaf;
	}
	ot.bbNode = node;
}
*/
