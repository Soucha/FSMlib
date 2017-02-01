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
	struct spy_node_t;
	//bool consistentComp(const spy_node_t* a, const spy_node_t* b);

	struct spy_node_t {
		sequence_in_t accessSequence;
		output_t incomingOutput;
		state_t state;
		output_t stateOutput;
		vector<shared_ptr<spy_node_t>> next;
		
		weak_ptr<spy_node_t> parent;
		//input_t incomingInputIdx;
		input_t distInputIdx;

		
		//set<spy_node_t*, bool(*)(const spy_node_t* a, const spy_node_t* b)> consistentNodes;
		set<state_t> refStates;

		state_t assumedState;
		seq_len_t maxSuffixLen;

		spy_node_t(input_t numberOfInputs) :
			incomingOutput(DEFAULT_OUTPUT), state(NULL_STATE), stateOutput(DEFAULT_OUTPUT),
			next(numberOfInputs), distInputIdx(STOUT_INPUT), assumedState(NULL_STATE), maxSuffixLen(0)
			//consistentNodes(consistentComp) 
			{
		}

		spy_node_t(const shared_ptr<spy_node_t>& parent, input_t input, //input_t idx,
			output_t transitionOutput, output_t stateOutput, input_t numInputs) : //vector<input_t> inputs) :
			accessSequence(parent->accessSequence), incomingOutput(transitionOutput), state(NULL_STATE),
			stateOutput(stateOutput), next(numInputs), parent(parent), distInputIdx(STOUT_INPUT),
			assumedState(NULL_STATE), maxSuffixLen(0) //consistentNodes(consistentComp) incomingInputIdx(idx),
			{
			accessSequence.push_back(input);
		}
	};

	struct requested_query_node_t {
		size_t seqCount = 0;
		list<size_t> relatedTransition;
		map<input_t, shared_ptr<requested_query_node_t>> next;
	};

	struct convergent_node_t {
		list<shared_ptr<spy_node_t>> convergent;
		//set<state_t> domain;
		vector<shared_ptr<convergent_node_t>> next;
		//input_t unconfirmed;
		shared_ptr<requested_query_node_t> requestedQueries;

		convergent_node_t(const shared_ptr<spy_node_t>& node) {
			convergent.emplace_back(node);
			requestedQueries = make_shared<requested_query_node_t>();
		}
	};

	typedef double frac_t;

	struct ads_cv_t {
		list<list<shared_ptr<spy_node_t>>> nodes;
		input_t input;
		map<output_t, shared_ptr<ads_cv_t>> next;

		ads_cv_t() : input(STOUT_INPUT) {}
		ads_cv_t(const shared_ptr<spy_node_t>& node) : nodes({ list<shared_ptr<spy_node_t>>({ node }) }), input(STOUT_INPUT) {}
	};

	struct SPYObservationTree {
		vector<shared_ptr<convergent_node_t>> stateNodes;
		shared_ptr<spy_node_t> bbNode;
		
		state_t numberOfExtraStates;
		unique_ptr<DFSM> conjecture;

		map<size_t, list<sequence_in_t>> queriesFromNextState;
		size_t unconfirmedTransitions;
		list<pair<state_t, input_t>> unprocessedConfirmedTransition;
		vector<sequence_set_t> stateIdentifier;
		vector<sequence_in_t> separatingSequences;
		//map<sequence_in_t, list<set<state_t>>> partitionBySepSeq;
		list<shared_ptr<spy_node_t>> inconsistentNodes;
		list<shared_ptr<spy_node_t>> identifiedNodes;
		state_t testedState;
		input_t testedInput;
		size_t repairingInconsistency;
	};

	static void checkNumberOfOutputs(const unique_ptr<Teacher>& teacher, const unique_ptr<DFSM>& conjecture) {
		if (conjecture->getNumberOfOutputs() != teacher->getNumberOfOutputs()) {
			conjecture->incNumberOfOutputs(teacher->getNumberOfOutputs() - conjecture->getNumberOfOutputs());
		}
	}

	static sequence_in_t getAccessSequence(const shared_ptr<spy_node_t>& parent, const shared_ptr<spy_node_t>& child) {
		auto it = child->accessSequence.begin();
		std::advance(it, parent->accessSequence.size());
		return sequence_in_t(it, child->accessSequence.end());
	}

	static void storeInconsistentNode(const shared_ptr<spy_node_t>& node, const shared_ptr<spy_node_t>& diffNode, SPYObservationTree& ot) {
		auto state = diffNode->assumedState;
		if (state == WRONG_STATE) state = diffNode->state;
		if ((diffNode == ot.stateNodes[state]->convergent.front()) && 
				(diffNode->assumedState != WRONG_STATE) && (node->assumedState != WRONG_STATE)) {
			diffNode->assumedState = WRONG_STATE;
		}
		const auto& refNode = ot.stateNodes[state]->convergent.front();// the sep seq will be quiried from there
		if (ot.inconsistentNodes.empty() || (refNode->accessSequence.size() < ot.inconsistentNodes.front()->accessSequence.size())) {
			ot.inconsistentNodes.emplace_front(diffNode);
			ot.inconsistentNodes.emplace_front(node);
		} else {
			ot.inconsistentNodes.emplace_back(node);
			ot.inconsistentNodes.emplace_back(diffNode);
		}
		ot.unprocessedConfirmedTransition.clear();
	}

	static void storeIdentifiedNode(const shared_ptr<spy_node_t>& node, SPYObservationTree& ot) {
		if (ot.identifiedNodes.empty()) {
			ot.identifiedNodes.emplace_back(node);
		} else if (node->refStates.empty()) {
			if (!ot.identifiedNodes.front()->refStates.empty() ||
					(ot.identifiedNodes.front()->accessSequence.size() > node->accessSequence.size())) {
				ot.identifiedNodes.clear();
				ot.identifiedNodes.emplace_back(node);
			}
		} else if (!ot.identifiedNodes.front()->refStates.empty()) {
			if (node->accessSequence.size() < ot.identifiedNodes.front()->accessSequence.size()) {
				ot.identifiedNodes.emplace_front(node); 
			} else {
				ot.identifiedNodes.emplace_back(node);
			}
		}
	}
	
	static bool areNodesDifferentUnder(const shared_ptr<spy_node_t>& n1, const shared_ptr<spy_node_t>& n2, seq_len_t len) {
		if ((n1->stateOutput != n2->stateOutput)) return true;
		if ((n1->distInputIdx == STOUT_INPUT) || (n2->maxSuffixLen < len)) return false;
		auto& idx = n1->distInputIdx;
		return ((n2->next[idx]) && ((n1->next[idx]->incomingOutput != n2->next[idx]->incomingOutput)
			|| areNodesDifferentUnder(n1->next[idx], n2->next[idx], len - 1)));
	}

	static bool areNodesDifferent(const shared_ptr<spy_node_t>& n1, const shared_ptr<spy_node_t>& n2) {
		if (n1->stateOutput != n2->stateOutput) return true;
		for (input_t i = 0; i < n1->next.size(); i++) {
			if ((n1->next[i]) && (n2->next[i]) && ((n1->next[i]->incomingOutput != n2->next[i]->incomingOutput)
				|| areNodesDifferent(n1->next[i], n2->next[i])))
				return true;
		}
		return false;
	}

	static void appendRequestedQuery(shared_ptr<requested_query_node_t>& rq, const sequence_in_t& seq, bool incSeqCount = false) {
		auto inIt = seq.begin();
		for (; inIt != seq.end(); ++inIt) {
			auto it = rq->next.find(*inIt);
			if (it == rq->next.end()) {
				break;
			}
			else {
				rq = it->second;
				if (incSeqCount) rq->seqCount++;
			}
		}
		for (; inIt != seq.end(); ++inIt) {
			auto nn = make_shared<requested_query_node_t>();
			rq->next.emplace(*inIt, nn);
			rq = nn;
			if (incSeqCount) rq->seqCount++;
		}
	}

	static void appendStateIdentifier(const shared_ptr<requested_query_node_t>& rq, state_t state, seq_len_t len, SPYObservationTree& ot) {
		if (len > 0) {
			for (input_t i = 0; i < ot.conjecture->getNumberOfInputs(); i++) {
				auto nn = make_shared<requested_query_node_t>();
				appendStateIdentifier(nn, ot.conjecture->getNextState(state, i), len - 1, ot);
				rq->next.emplace(i, move(nn));
			}
		}
		for (auto& seq : ot.stateIdentifier[state]) {
			auto actRq = rq;
			appendRequestedQuery(actRq, seq);
		}
	}

	static void updateQueriesFromNextState(const shared_ptr<requested_query_node_t>& rq, 
			sequence_in_t& seq, list<sequence_in_t>& queries, SPYObservationTree& ot) {
		if (rq->next.empty()) {
			rq->relatedTransition.emplace_back(-1);
			rq->seqCount = 1;
			queries.emplace_back(seq);
		} else {
			rq->seqCount = 0;
			for (auto& p : rq->next) {
				seq.emplace_back(p.first);
				updateQueriesFromNextState(p.second, seq, queries, ot);
				seq.pop_back();
				rq->seqCount += p.second->seqCount;
			}
		}
	}

	static void generateRequestedQueries(SPYObservationTree& ot) {
		size_t tranId = 0;
		vector<state_t> nextStates(ot.conjecture->getNumberOfStates() * ot.conjecture->getNumberOfInputs());
		ot.unconfirmedTransitions = 0;
		ot.queriesFromNextState.clear();
		for (state_t state = 0; state < ot.conjecture->getNumberOfStates(); state++) {
			auto& cn = ot.stateNodes[state];
			
			// clear convergent
			for (auto& conv : cn->convergent) {
				conv->assumedState = conv->state = NULL_STATE;
			}
			auto stateNode = cn->convergent.front();
			stateNode->assumedState = stateNode->state = state;
			cn->convergent.clear();
			cn->convergent.emplace_back(stateNode);
			
			cn->next.clear();
			cn->next.resize(ot.conjecture->getNumberOfInputs());

			// init requested queries
			cn->requestedQueries->next.clear();
			cn->requestedQueries->seqCount = 0;
			
			// update QueriesFromNextState, RelatedTransitions and SeqCounts
			for (input_t i = 0; i < ot.conjecture->getNumberOfInputs(); i++, tranId++) {
				auto& succ = stateNode->next[i];
				if (!succ || (succ->state == NULL_STATE) || (succ != ot.stateNodes[succ->state]->convergent.front())) {// unconfirmed transition
					auto rq = make_shared<requested_query_node_t>();
					if (ot.numberOfExtraStates > 0) {
						appendStateIdentifier(rq, ot.conjecture->getNextState(state, i), ot.numberOfExtraStates, ot);

						list<sequence_in_t> queries;
						sequence_in_t seq;
						updateQueriesFromNextState(rq, seq, queries, ot);
						queries.sort();
						ot.queriesFromNextState.emplace(tranId, move(queries));
						nextStates[tranId] = ot.conjecture->getNextState(state, i);
					} else {
						rq->seqCount = 1;
					}
 					cn->requestedQueries->seqCount += rq->seqCount;
					cn->requestedQueries->next.emplace(i, move(rq));
					ot.unconfirmedTransitions++;
				} else {
					cn->next[i] = ot.stateNodes[succ->state];
					if (ot.numberOfExtraStates == 0) {
						ot.conjecture->setTransition(state, i, succ->state,
							(ot.conjecture->isOutputTransition() ? succ->incomingOutput : DEFAULT_OUTPUT));
					}
				}
			}
		}
		for (auto& p : ot.queriesFromNextState) {
			for (auto& seq : p.second) {
				auto rq = ot.stateNodes[nextStates[p.first]]->requestedQueries;
				appendRequestedQuery(rq, seq);
				rq->relatedTransition.emplace_back(p.first);
			}
		}
	}

	static void decreaseSeqCount(shared_ptr<requested_query_node_t> rq, const sequence_in_t& seq) {
		rq->seqCount--;
		for (auto input : seq) {
			rq = rq->next[input];
			rq->seqCount--;
		}
	}

	static void checkTransition(state_t state, input_t input, SPYObservationTree& ot) {
		auto& ssRq = ot.stateNodes[state]->requestedQueries;
		auto it = ssRq->next.find(input);
		if ((it == ssRq->next.end()) || (it->second->seqCount == 0)) {// all seqs applied
			ot.unprocessedConfirmedTransition.emplace_back(state, input);
		}
	}

	static void processRelatedTransitions(const shared_ptr<requested_query_node_t>& rq, state_t testedState, 
			const sequence_in_t& testedSeq, SPYObservationTree& ot) {
		if (rq->relatedTransition.empty()) return;

		auto numInputs = ot.conjecture->getNumberOfInputs();
		for (auto& relTran : rq->relatedTransition) {
			if (relTran == size_t(-1)) {
				decreaseSeqCount(ot.stateNodes[testedState]->requestedQueries, testedSeq);
				// check tran if verified
				auto qIt = ot.queriesFromNextState.find(testedState * numInputs + testedSeq.front());
				if ((qIt == ot.queriesFromNextState.end()) || (qIt->second.empty())) {
					checkTransition(testedState, testedSeq.front(), ot);
				}
			} else {
				auto& queries = ot.queriesFromNextState[relTran];
				queries.erase(lower_bound(queries.begin(), queries.end(), testedSeq));
				if (queries.empty()) {
					// check tran if verified
					state_t startState(state_t(relTran / numInputs));
					input_t input = relTran % numInputs;
					checkTransition(startState, input, ot);
				}
			}
		}
		rq->relatedTransition.clear();
	}

	static bool updateRequestedQueries(const shared_ptr<requested_query_node_t>& rq, const shared_ptr<spy_node_t>& spyNode,
		const state_t& state, sequence_in_t& testedSeq, SPYObservationTree& ot) {

		processRelatedTransitions(rq, state, testedSeq, ot);
		if (rq->next.empty()) return true;

		for (auto it = rq->next.begin(); it != rq->next.end();) {
			if (spyNode->next[it->first]) {
				testedSeq.emplace_back(it->first);
				if (updateRequestedQueries(it->second, spyNode->next[it->first], state, testedSeq, ot)) {
					it = rq->next.erase(it);
				} else {
					++it;
				}
				testedSeq.pop_back();
			} else {
				++it;
			}
		}
		return (rq->next.empty());
	}

	static set<state_t> getSuccessorDomain(const shared_ptr<spy_node_t>& spyNode, input_t input, SPYObservationTree& ot) {
		auto domain = spyNode->next[input]->refStates;
		for (auto& sn : ot.stateNodes[spyNode->state]->convergent) {
			auto& nn = sn->next[input];
			if (nn) {
				// set intersection
				auto dIt = domain.begin();
				auto sIt = nn->refStates.begin();
				while ((dIt != domain.end()) && (sIt != nn->refStates.end())) {
					if (*dIt < *sIt) {
						dIt = domain.erase(dIt);
					} else {
						if (!(*sIt < *dIt)) {// equal
							auto oSnIt = ot.stateNodes[*sIt]->convergent.begin();
							for (++oSnIt; oSnIt != ot.stateNodes[*sIt]->convergent.end(); ++oSnIt) {
								if (areNodesDifferent(nn, *oSnIt)) {
									break;
								}
							}
							if (oSnIt == ot.stateNodes[*sIt]->convergent.end()) {
								++dIt;
							}
						}
						++sIt;
					}
				}
				while (dIt != domain.end()) dIt = domain.erase(dIt);
			}
		}
		return domain;
	}

	static bool checkRequestedQueries(const shared_ptr<spy_node_t>& node, input_t input, SPYObservationTree& ot) {
		auto nextState = node->next[input]->state;
		if (nextState == NULL_STATE) {
			auto domain = getSuccessorDomain(node, input, ot);
			if (domain.empty()) {// inconsistency found
				for (const auto& n : ot.stateNodes[node->state]->convergent) {
					if (n->next[input] && (node != n) && ((node->next[input]->incomingOutput != n->next[input]->incomingOutput)
							|| areNodesDifferent(node->next[input], n->next[input]))) {
						node->assumedState = WRONG_STATE;
						storeInconsistentNode(node, n, ot);
						break;
					}
				}
				if (node->assumedState != WRONG_STATE) {
					node->assumedState = WRONG_STATE;
					storeInconsistentNode(node->next[input], node, ot);
				}
			} else if (domain.size() == 1) {// identified
				nextState = *(domain.begin());
			}
		}
		if (nextState != NULL_STATE) {
			ot.unprocessedConfirmedTransition.emplace_back(node->state, input);
			ot.conjecture->setTransition(node->state, input, nextState,
				(ot.conjecture->isOutputTransition() ? node->next[input]->incomingOutput : DEFAULT_OUTPUT));
			return true;
		}
		return false;
	}

	static bool checkRequestedQueries(const shared_ptr<spy_node_t>& node, SPYObservationTree& ot) {
		auto& rq = ot.stateNodes[node->state]->requestedQueries;
		auto it = rq->next.find(node->distInputIdx);
		if (it != rq->next.end() && checkRequestedQueries(node, node->distInputIdx, ot)) {
			rq->seqCount--;
			rq->next.erase(it);
		}
		return !ot.inconsistentNodes.empty();
	}

	static bool checkConvergentNodes(const shared_ptr<spy_node_t>& node, seq_len_t suffixLen, SPYObservationTree& ot) {
		if (node != ot.stateNodes[node->state]->convergent.front()) {
			for (const auto& n : ot.stateNodes[node->state]->convergent) {
				if ((node != n) && areNodesDifferentUnder(node, n, suffixLen)) {
					node->assumedState = WRONG_STATE;
					storeInconsistentNode(node, n, ot);
					break;
				}
			}
		}
		return !ot.inconsistentNodes.empty();
	}
	
	static bool mergeConvergent(state_t state, const shared_ptr<spy_node_t>& spyNode, SPYObservationTree& ot, bool forceMerge = false) {

		if (!spyNode->refStates.count(state)) {// discrepancy found
			spyNode->assumedState = state;
			storeInconsistentNode(spyNode, spyNode->parent.lock(), ot);
			return false;
		}
		if ((spyNode->state != NULL_STATE) && !forceMerge) {
			// already merged + hack for refNode
			return true;
		}

		spyNode->assumedState = spyNode->state = state;
		const auto& cn = ot.stateNodes[state];
		// updating requestedQueries - it cannot be removed if ES == 0 and the next state is not identified
		if (ot.numberOfExtraStates == 0) {
			auto& rq = cn->requestedQueries;
			for (auto it = rq->next.begin(); it != rq->next.end();) {
				if (spyNode->next[it->first]) {
					if (checkRequestedQueries(spyNode, it->first, ot)) {
						rq->seqCount--;
						it = rq->next.erase(it);
					} else if (!ot.inconsistentNodes.empty()) {
						cn->convergent.emplace_back(spyNode);
						return false;
					} else {// let reqQuery (a transition) there for identification
						++it;
					}
				} else {// let reqQuery (a transition) there for identification
					++it;
				}
			}
		} else {
			// check requestedQueries of cn
			sequence_in_t seq;
			updateRequestedQueries(cn->requestedQueries, spyNode, state, seq, ot);
		}
		for (input_t i = 0; i < ot.conjecture->getNumberOfInputs(); i++) {
			if (spyNode->next[i]) {
				if (cn->next[i]) {
					if (!mergeConvergent(cn->next[i]->convergent.front()->state, spyNode->next[i], ot)) {
						cn->convergent.emplace_back(spyNode); 
						return false;
					}
				} else if (ot.numberOfExtraStates == 0) {
					for (const auto& n : cn->convergent) {
						if (n->next[i] && ((spyNode->next[i]->incomingOutput != n->next[i]->incomingOutput)
								|| areNodesDifferent(spyNode->next[i], n->next[i]))) {
							cn->convergent.emplace_back(spyNode); 
							spyNode->assumedState = WRONG_STATE;
							storeInconsistentNode(spyNode, n, ot);
							return false;
						}
					}
				}
			}
		}
		
		if (spyNode->accessSequence.size() < cn->convergent.front()->accessSequence.size()) {
			//change refStates
			//cn->convergent.emplace_front(spyNode);
			cn->convergent.emplace_back(spyNode);
		} else if (spyNode != cn->convergent.front()) {
			cn->convergent.emplace_back(spyNode);
		}
		return true;
	}

	static void processConfirmedTransitions(SPYObservationTree& ot) {
		while (!ot.unprocessedConfirmedTransition.empty()) {
			auto state = ot.unprocessedConfirmedTransition.front().first;
			auto input = ot.unprocessedConfirmedTransition.front().second;
			auto nextState = ot.conjecture->getNextState(state, input);
			auto& fromCN = ot.stateNodes[state];// ->next[input];
			ot.stateNodes[state]->next[input] = ot.stateNodes[nextState];
			for (auto& fcn : fromCN->convergent) {
				if (fcn->next[input]) {
					if (!mergeConvergent(nextState, fcn->next[input], ot))  {
						return;
					}
				}
			}
			ot.unprocessedConfirmedTransition.pop_front();
			ot.unconfirmedTransitions--;
		}
	}

	static void updateOTreeWithNewState(const shared_ptr<spy_node_t>& node, SPYObservationTree& ot) {
		stack<shared_ptr<spy_node_t>> nodes;
		nodes.emplace(ot.stateNodes[0]->convergent.front());//root
		while (!nodes.empty()) {
			auto otNode = move(nodes.top());
			nodes.pop();
			otNode->assumedState = NULL_STATE;
			if ((node == otNode) || !areNodesDifferent(node, otNode)) {
				otNode->refStates.emplace(node->state);
			}
			if (otNode->refStates.size() <= 1) {
				storeIdentifiedNode(otNode, ot);
			}
			for (const auto& nn : otNode->next) {
				if (nn) nodes.emplace(nn);
			}
		}	
	}

	static bool isSeparatingSequenceQueried(shared_ptr<spy_node_t> node, state_t state, sequence_in_t& sepSeq, SPYObservationTree& ot) {
		auto otherNode = ot.stateNodes[state]->convergent.front();
		if (node->stateOutput != otherNode->stateOutput) return true; 
		
		auto& seq = ot.separatingSequences[FSMsequence::getStatePairIdx(state, node->assumedState)];
		for (auto input : seq) {
			if (!node->next[input]) return false; // not queried
			sepSeq.push_back(input);
			node = node->next[input];
			otherNode = otherNode->next[input];
			if ((node->incomingOutput != otherNode->incomingOutput) || (node->stateOutput != otherNode->stateOutput)) {
				// already distinguished
				return true;
			}
		}
		// not distinuished
		return false;
	}

	static sequence_in_t getQueriedSeparatingSequence(const shared_ptr<spy_node_t>& n1, const shared_ptr<spy_node_t>& n2) {
		if (n1->stateOutput != n2->stateOutput) {
			return sequence_in_t();
		}
		queue<pair<shared_ptr<spy_node_t>, shared_ptr<spy_node_t>>> fifo;
		fifo.emplace(n1, n2);
		while (!fifo.empty()) {
			auto& p = fifo.front();
			for (input_t i = 0; i < p.first->next.size(); i++) {
				if (p.first->next[i] && p.second->next[i]) {
					if ((p.first->next[i]->incomingOutput != p.second->next[i]->incomingOutput)
						|| (p.first->next[i]->stateOutput != p.second->next[i]->stateOutput)) {
						return getAccessSequence(n1, p.first->next[i]);
					}
					fifo.emplace(p.first->next[i], p.second->next[i]);
				}
			}
			fifo.pop();
		}
		return sequence_in_t();
	}

	static bool isPrefix(const sequence_in_t& seq, const sequence_in_t& base) {
		if (seq.size() > base.size()) return false;
		auto it = base.begin();
		for (auto& input : seq) {
			if (input != *it) return false;
			++it;
		}
		return true;
	}

	static bool isPrefix(const sequence_in_t& seq, const sequence_set_t& H) {
		if (seq.empty()) return true;
		for (auto& hSeq : H) {
			if (isPrefix(seq, hSeq)) return true;
		}
		return false;
	}

	static void removePrefixes(const sequence_in_t& seq, sequence_set_t& H) {
		for (auto it = H.begin(); it != H.end();) {
			if (isPrefix(*it, seq)) {
				it = H.erase(it);
			}
			else ++it;
		}
	}

	static bool processInconsistent(SPYObservationTree& ot) {
		for (auto it = ot.inconsistentNodes.begin(); it != ot.inconsistentNodes.end();) {
			auto node = *it;
			it = ot.inconsistentNodes.erase(it);
			auto otNode = *it;
			it = ot.inconsistentNodes.erase(it);
			if (otNode->assumedState == WRONG_STATE) {// domain of successor was empty due to inconsistent convergent nodes
				otNode->assumedState = ot.testedState = otNode->state;
				auto& input = node->accessSequence.back();
				ot.testedInput = input;
				auto& refNode = ot.stateNodes[otNode->state]->convergent.front()->next[input];
				for (auto& sn : ot.stateNodes) {
					sn->requestedQueries->next.clear();
					sn->requestedQueries->seqCount = 0;
				}
				ot.repairingInconsistency = 1;
				sequence_set_t hsi;
				for (auto state : refNode->refStates) {
					auto diffNode = node;
					auto sn = ot.stateNodes[state]->convergent.front();
					if (node->refStates.count(state)) {
						for (auto& n : ot.stateNodes[otNode->state]->convergent) {
							if (n->next[input] && !n->next[input]->refStates.count(state)) {
								diffNode = n->next[input];
								break;
							}
						}
						if (node == diffNode) {
							diffNode = nullptr;
							for (auto& n : ot.stateNodes[otNode->state]->convergent) {
								auto& nn = n->next[input];
								if (nn) {
									auto oSnIt = ot.stateNodes[state]->convergent.begin();
									for (++oSnIt; oSnIt != ot.stateNodes[state]->convergent.end(); ++oSnIt) {
										if (areNodesDifferent(nn, *oSnIt)) {
											sn = *oSnIt;
											diffNode = nn;
											break;
										}
									}
									if (diffNode) break;
								}
							}
						}
					}
					auto sepSeq = getQueriedSeparatingSequence(diffNode, sn);
					if (sn != ot.stateNodes[state]->convergent.front()) {
						auto& oRq = ot.stateNodes[state]->requestedQueries;
						oRq->seqCount = 1;
						auto nRq = oRq;
						appendRequestedQuery(nRq, sepSeq, true);
						ot.repairingInconsistency++;
						if (ot.testedState > state) {
							ot.testedState = state;
							ot.testedInput = sepSeq.front();
						}
					}
					if (diffNode != refNode) {
						sepSeq.push_front(input);
						if (!isPrefix(sepSeq, hsi)) {
							removePrefixes(sepSeq, hsi);
							hsi.emplace(sepSeq);
						}
					}
				}
				auto& rq = ot.stateNodes[otNode->state]->requestedQueries;
				rq->seqCount += hsi.size();
				for (auto& sepSeq : hsi) {
					auto nRq = rq;
					appendRequestedQuery(nRq, sepSeq, true);
				}
				ot.repairingInconsistency += hsi.size();

				break;
			} else if((node->assumedState != NULL_STATE) && (!node->refStates.count(node->assumedState))) {
				// find distinguishing suffix and apply it where necessary
				ot.testedState = otNode->assumedState;
				auto sepSeq = getQueriedSeparatingSequence(node,
					(node->assumedState == WRONG_STATE) ? otNode : ot.stateNodes[node->assumedState]->convergent.front());
				if (node->assumedState == WRONG_STATE) {
					ot.testedInput = sepSeq.front();
					node->assumedState = ot.testedState;
				} else {
					ot.testedInput = node->accessSequence.back();
					sepSeq.push_front(ot.testedInput);
				}
				auto& rq = ot.stateNodes[ot.testedState]->requestedQueries;
				rq->next.clear();
				rq->seqCount = 1;
				auto nRq = rq;
				appendRequestedQuery(nRq, sepSeq, true);
				ot.repairingInconsistency = 2;
				break;
			}
		}
		return (ot.repairingInconsistency > 0);
	}

	static void makeStateNode(shared_ptr<spy_node_t> node, SPYObservationTree& ot) {
		sequence_set_t hsi;
		// update hsi
		for (state_t state = 0; state < ot.stateNodes.size(); state++) {
			sequence_in_t sepSeq;
			if ((node->assumedState == NULL_STATE) || (node->assumedState == state) ||
					!isSeparatingSequenceQueried(node, state, sepSeq, ot)) {
				sepSeq = getQueriedSeparatingSequence(node, ot.stateNodes[state]->convergent.front());
				if (!isPrefix(sepSeq, ot.stateIdentifier[state])) {
					removePrefixes(sepSeq, ot.stateIdentifier[state]);
					ot.stateIdentifier[state].emplace(sepSeq);
				}
			}// else sepSeq is set to a separating prefix of the sep. seq. of (state,node->assumedState), see isSeparatingSequenceQueried
			if (!isPrefix(sepSeq, hsi)) {
				removePrefixes(sepSeq, hsi);
				hsi.emplace(sepSeq);
			}
			ot.separatingSequences.emplace_back(move(sepSeq));
		}
		ot.stateIdentifier.emplace_back(move(hsi));

		node->state = ot.conjecture->addState(node->stateOutput);
		ot.stateNodes.emplace_back(make_shared<convergent_node_t>(node));

		// update OTree with new state and fill ot.identifiedNodes with nodes with |domain|<= 1
		ot.identifiedNodes.clear();
		updateOTreeWithNewState(node, ot);

		ot.numberOfExtraStates = 0;
		if (!ot.identifiedNodes.empty() && (ot.identifiedNodes.front()->refStates.empty())) {// new state
			return makeStateNode(move(ot.identifiedNodes.front()), ot);
		}
		ot.testedState = 0;
		generateRequestedQueries(ot);
		
		ot.repairingInconsistency = 0;
		ot.inconsistentNodes.clear();
		for (auto& in : ot.identifiedNodes) {
			auto& iState = *(in->refStates.begin());
			if (!mergeConvergent(iState, in, ot, in == ot.stateNodes[iState]->convergent.front())) {
				break;
			}
		}
		processConfirmedTransitions(ot);
		ot.identifiedNodes.clear();
	}

	static bool checkNode(const shared_ptr<spy_node_t>& node, SPYObservationTree& ot) {
		if (node->refStates.empty()) {// new state
			storeIdentifiedNode(node, ot);
			return true;
		} else if ((node->assumedState != NULL_STATE) && !node->refStates.count(node->assumedState)) {// inconsistent node
			storeInconsistentNode(node, node->parent.lock(), ot);
			return true;
		} else if ((ot.numberOfExtraStates == 0) && (node->state == NULL_STATE) && (node->refStates.size() == 1)) {
			storeIdentifiedNode(node, ot);
		}
		return false;
	}

	static bool checkAgainstAllNodes(const shared_ptr<spy_node_t>& node, const seq_len_t& suffixLen, SPYObservationTree& ot) {
		bool newStateOrInconsistencyFound = false;
		stack<shared_ptr<spy_node_t>> nodes;
		nodes.emplace(ot.stateNodes[0]->convergent.front());//root
		while (!nodes.empty()) {
			auto otNode = move(nodes.top());
			nodes.pop();
			if ((otNode != node) && (otNode->refStates.count(node->state))) {
				if (areNodesDifferentUnder(node, otNode, suffixLen)) {
					otNode->refStates.erase(node->state);
					newStateOrInconsistencyFound |= checkNode(otNode, ot);
				}
			}
			for (const auto& nn : otNode->next) {
				if (nn && (nn->maxSuffixLen >= suffixLen)) nodes.emplace(nn);
			}
		}
		return newStateOrInconsistencyFound;
	}

	static bool checkPrevious(shared_ptr<spy_node_t> node, SPYObservationTree& ot) {
		bool newStateOrInconsistencyFound = (ot.repairingInconsistency > 0) || !ot.inconsistentNodes.empty();
		seq_len_t suffixLen = 0;
		input_t input(STOUT_INPUT);
		do {
			node->distInputIdx = input;
			if (suffixLen > node->maxSuffixLen) node->maxSuffixLen = suffixLen;
			if ((node->state == NULL_STATE) || (node != ot.stateNodes[node->state]->convergent.front())) {
				for (auto snIt = node->refStates.begin(); snIt != node->refStates.end();) {
					if (areNodesDifferentUnder(node, ot.stateNodes[*snIt]->convergent.front(), suffixLen)) {
						snIt = node->refStates.erase(snIt);
					}
					else ++snIt;
				}
				newStateOrInconsistencyFound |= checkNode(node, ot);
			} else {// state node
				newStateOrInconsistencyFound |= checkAgainstAllNodes(node, suffixLen, ot);
			}
			if (!newStateOrInconsistencyFound && (ot.numberOfExtraStates == 0) && (node->state != NULL_STATE)) {
				newStateOrInconsistencyFound |= checkRequestedQueries(node, ot) || checkConvergentNodes(node, suffixLen, ot);
			}
			if (!node->accessSequence.empty()) input = node->accessSequence.back();
			else {// all nodes checked
				if (!ot.identifiedNodes.empty()) {
					if (ot.identifiedNodes.front()->refStates.empty()) {// new state
						makeStateNode(move(ot.identifiedNodes.front()), ot);
					} else if (!newStateOrInconsistencyFound) {
						for (const auto& n : ot.identifiedNodes) {
							if (!mergeConvergent(*(n->refStates.begin()), n, ot)) {
								newStateOrInconsistencyFound = true;
								break;
							}
							if ((ot.numberOfExtraStates == 0) && !isPrefix(n->accessSequence, ot.bbNode->accessSequence)) {
								auto& parent = n->parent.lock();
								if (parent->state != NULL_STATE) {
									parent->distInputIdx = n->accessSequence.back();
									if (checkRequestedQueries(parent, ot)) {
										newStateOrInconsistencyFound = true;
										break;
									}
								}
							}
						}
						processConfirmedTransitions(ot);
						ot.identifiedNodes.clear();
					}
				} else {
					processConfirmedTransitions(ot);
				}
				if (ot.repairingInconsistency > 1) {
					newStateOrInconsistencyFound = false;
				} else {
					newStateOrInconsistencyFound |= processInconsistent(ot);
				}
			}
			node = node->parent.lock();
			suffixLen++;
		} while (node);
		return newStateOrInconsistencyFound;
	}

	static void query(const shared_ptr<spy_node_t>& node, input_t input, SPYObservationTree& ot, const unique_ptr<Teacher>& teacher) {
		state_t transitionOutput, stateOutput = DEFAULT_OUTPUT;
		if (!teacher->isProvidedOnlyMQ() && (ot.conjecture->getType() == TYPE_DFSM)) {
			sequence_in_t suffix({ input, STOUT_INPUT });
			auto output = (ot.bbNode == node) ? teacher->outputQuery(suffix) :
				teacher->resetAndOutputQueryOnSuffix(node->accessSequence, suffix);
			transitionOutput = output.front();
			stateOutput = output.back();
#if DUMP_OQ 
			if (ot.bbNode == node) {
				printf("%d T(%s) = %s query\n", teacher->getOutputQueryCount(),
					FSMmodel::getInSequenceAsString(suffix).c_str(), FSMmodel::getOutSequenceAsString(output).c_str());
			}
			else {
				printf("%d T(%s, %s) = %s query\n", teacher->getOutputQueryCount(),
					FSMmodel::getInSequenceAsString(node->accessSequence).c_str(),
					FSMmodel::getInSequenceAsString(suffix).c_str(), FSMmodel::getOutSequenceAsString(output).c_str());
			}
#endif // DUMP_OQ
		}
		else {
			transitionOutput = (ot.bbNode == node) ? teacher->outputQuery(input) :
				teacher->resetAndOutputQueryOnSuffix(node->accessSequence, input);
#if DUMP_OQ 
			if (ot.bbNode == node) {
				printf("%d T(%d) = %d query\n", teacher->getOutputQueryCount(), input, transitionOutput);
			}
			else {
				printf("%d T(%s, %s) = %s query\n", teacher->getOutputQueryCount(),
					FSMmodel::getInSequenceAsString(node->accessSequence).c_str(), input, transitionOutput);
			}
#endif // DUMP_OQ
			if (ot.conjecture->getType() == TYPE_DFSM) {
				stateOutput = teacher->outputQuery(STOUT_INPUT);
#if DUMP_OQ 
				printf("%d T(S) = %d query\n", teacher->getOutputQueryCount(), stateOutput);
#endif // DUMP_OQ
			}
			else if (!ot.conjecture->isOutputTransition()) {// Moore, DFA
				stateOutput = transitionOutput;
				//transitionOutput = DEFAULT_OUTPUT;
			}
		}
		checkNumberOfOutputs(teacher, ot.conjecture);
		auto leaf = make_shared<spy_node_t>(node, input, transitionOutput, stateOutput, ot.conjecture->getNumberOfInputs());// teacher->getNextPossibleInputs());
		if (ot.conjecture->getNumberOfInputs() != teacher->getNumberOfInputs()) {
			// update all requested spyOTree
			ot.conjecture->incNumberOfInputs(teacher->getNumberOfInputs() - ot.conjecture->getNumberOfInputs());
		}
		node->next[input] = leaf;
		ot.bbNode = leaf;
		auto domIt = leaf->refStates.end();
		// init domain
		for (state_t state = 0; state < ot.conjecture->getNumberOfStates(); state++) {
			if (ot.conjecture->isOutputState()) {
				if (ot.stateNodes[state]->convergent.front()->stateOutput == stateOutput) {
					domIt = leaf->refStates.emplace_hint(domIt, state);
				}
			} else {
				domIt = leaf->refStates.emplace_hint(domIt, state);
			}
		}
		if ((node->assumedState != NULL_STATE) && (ot.numberOfExtraStates > 0)) {
			leaf->assumedState = ot.conjecture->getNextState(node->assumedState, input);
		}
	}

	static bool moveAndCheck(shared_ptr<spy_node_t>& currNode, shared_ptr<requested_query_node_t>& rq, state_t state,
			const sequence_in_t& testedSeq,	SPYObservationTree& ot, const unique_ptr<Teacher>& teacher) {
		auto& nextInput = testedSeq.back();
		if (currNode->next[nextInput]) {
			currNode = currNode->next[nextInput];
			return false;
		}
		query(currNode, nextInput, ot, teacher);
		if ((ot.repairingInconsistency == 0) && (currNode->state != NULL_STATE) && (ot.stateNodes[currNode->state]->next[nextInput])) {
			mergeConvergent(ot.stateNodes[currNode->state]->next[nextInput]->convergent.front()->state, currNode->next[nextInput], ot);
		}
		currNode = currNode->next[nextInput];
		if (ot.repairingInconsistency == 0) {
			processRelatedTransitions(rq, state, testedSeq, ot);
			processConfirmedTransitions(ot);
		} else if (rq->next.empty()) {
			ot.repairingInconsistency--;
			decreaseSeqCount(ot.stateNodes[state]->requestedQueries, testedSeq);
		}
		return checkPrevious(currNode, ot);
	}

	static bool areDistinguished(const list<list<shared_ptr<spy_node_t>>>& nodes) {
		for (auto it1 = nodes.begin(); it1 != nodes.end(); ++it1) {
			auto it2 = it1;
			for (++it2; it2 != nodes.end(); ++it2) {
				for (auto& n1 : *it1) {
					for (auto& n2 : *it2) {
						if (areNodesDifferent(n1, n2)) return true;
					}
				}
			}
		}
		return false;
	}

	static void chooseADS(const shared_ptr<ads_cv_t>& ads, const SPYObservationTree& ot, frac_t& bestVal, frac_t& currVal,
		seq_len_t& totalLen, seq_len_t currLength = 1, state_t undistinguishedStates = 0, double prob = 1) {
		auto numStates = state_t(ads->nodes.size()) + undistinguishedStates;
		frac_t localBest(1);
		seq_len_t subtreeLen, minLen = seq_len_t(-1);
		for (input_t i = 0; i < ot.conjecture->getNumberOfInputs(); i++) {
			undistinguishedStates = numStates - state_t(ads->nodes.size());
			map<output_t, shared_ptr<ads_cv_t>> next;
			for (auto& cn : ads->nodes) {
				bool isUndistinguished = true;
				for (auto& node : cn) {
					if (node->next[i]) {
						auto it = next.find(node->next[i]->incomingOutput);
						if (it == next.end()) {
							next.emplace(node->next[i]->incomingOutput, make_shared<ads_cv_t>(node->next[i]));
						}
						else if (isUndistinguished) {
							it->second->nodes.emplace_back(list<shared_ptr<spy_node_t>>({ node->next[i] }));
						}
						else {
							it->second->nodes.back().emplace_back(node->next[i]);
						}
						isUndistinguished = false;
					}
				}
				if (isUndistinguished) {
					undistinguishedStates++;
				}
			}
			if (next.empty()) continue;
			auto adsVal = currVal;
			subtreeLen = 0;
			for (auto p : next) {
				if ((p.second->nodes.size() == 1) || (!areDistinguished(p.second->nodes))) {
					adsVal += frac_t(p.second->nodes.size() + undistinguishedStates - 1) / (prob * next.size() * (numStates - 1));
					subtreeLen += (currLength * (p.second->nodes.size() + undistinguishedStates));
				}
				else if (ot.conjecture->getType() == TYPE_DFSM) {
					for (auto& cn : p.second->nodes) {
						bool isFirstNode = true;
						for (auto& node : cn) {
							auto it = p.second->next.find(node->stateOutput);
							if (it == p.second->next.end()) {
								p.second->next.emplace(node->stateOutput, make_shared<ads_cv_t>(node));
							}
							else if (isFirstNode) {
								it->second->nodes.emplace_back(list<shared_ptr<spy_node_t>>({ node }));
								isFirstNode = false;
							}
							else {
								it->second->nodes.back().emplace_back(node);
							}
						}
					}
					for (auto& sp : p.second->next) {
						if ((sp.second->nodes.size() == 1) || (!areDistinguished(sp.second->nodes))) {
							adsVal += frac_t(sp.second->nodes.size() + undistinguishedStates - 1) /
								(prob * next.size() * p.second->next.size() * (numStates - 1));
							subtreeLen += (currLength * (sp.second->nodes.size() + undistinguishedStates));
						}
						else {
							chooseADS(sp.second, ot, bestVal, adsVal, subtreeLen, currLength + 1,
								undistinguishedStates, prob * next.size() * p.second->next.size());
						}
					}
				}
				else {
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

	static bool queryRequiredSequence(state_t state, sequence_in_t seq, SPYObservationTree& ot, const unique_ptr<Teacher>& teacher) {
		auto rq = ot.stateNodes[state]->requestedQueries;
		auto currNode = ot.stateNodes[state]->convergent.front();// shortest access sequence
		sequence_in_t testedSeq;
		auto lastBranch = rq;
		auto lastBranchInput = seq.front();
		shared_ptr<ads_cv_t> ads;
		for (auto nextInput : seq) {
			if (rq->next.size() > 1) {
				lastBranch = rq;
				lastBranchInput = nextInput;
			}
			rq = rq->next[nextInput];

			testedSeq.emplace_back(nextInput);
			if (moveAndCheck(currNode, rq, state, testedSeq, ot, teacher)) {
				return true;
			}
		}
		if ((ot.numberOfExtraStates == 0) && (currNode->state == NULL_STATE) && rq->next.empty()) {// identify 
			auto domain = getSuccessorDomain(ot.stateNodes[state]->convergent.front(), seq.front(), ot);
			if (domain.size() < 2) {
				if (checkPrevious(currNode, ot)) {
					return true;
				}
				/*
				auto input = seq.front();
				auto& node = ot.stateNodes[state]->convergent.front();
				if (domain.empty()) {// inconsistency found
					for (const auto& n : ot.stateNodes[state]->convergent) {
						if (n->next[input] && (node != n) && ((node->next[input]->incomingOutput != n->next[input]->incomingOutput)
							|| areNodesDifferent(node->next[input], n->next[input]))) {
							node->assumedState = WRONG_STATE;
							storeInconsistentNode(node, n, ot);
							break;
						}
					}
					if (node->assumedState != WRONG_STATE) {
						node->assumedState = WRONG_STATE;
						storeInconsistentNode(node->next[input], node, ot);
					}
				}
				else {// identified
					auto nextState = *(domain.begin());
					ot.unprocessedConfirmedTransition.emplace_back(node->state, input);
					ot.conjecture->setTransition(node->state, input, nextState,
						(ot.conjecture->isOutputTransition() ? node->next[input]->incomingOutput : DEFAULT_OUTPUT));

					return true;
				}
				*/
			}
				// choose separating sequence to query
			else	if (domain.size() == 2) {
					auto idx = FSMsequence::getStatePairIdx(*(domain.begin()), *(domain.rbegin()));
					//seq = ot.separatingSequences[idx];
					auto nRq = rq;
					appendRequestedQuery(nRq, ot.separatingSequences[idx], true);
				} else {
					// choose ads
					ads = make_shared<ads_cv_t>();
					for (auto& domState : domain) {
						ads->nodes.push_back(ot.stateNodes[domState]->convergent);
					}
					seq_len_t totalLen = 0;
					frac_t bestVal(1), currVal(0);
					chooseADS(ads, ot, bestVal, currVal, totalLen);
					auto nn = make_shared<requested_query_node_t>();
					nn->seqCount++; 
					rq->next.emplace(ads->input, nn);
					lastBranch = rq;
					lastBranchInput = ads->input;
				}
			//}
			
		}
		while (!rq->next.empty()) {
			bool updateLastBranch = (rq->next.size() > 1);
			if (updateLastBranch) {
				lastBranch = rq;
			}
			input_t nextInput(STOUT_INPUT);
			for (const auto& inIt : rq->next) {
				if (inIt.second->seqCount > 0) {// verifying tested transition
					nextInput = inIt.first;
					rq = inIt.second;
					break;
				}
			}
			if (nextInput == STOUT_INPUT) {// verifying other transition
				nextInput = rq->next.begin()->first;
				rq = rq->next.begin()->second;
			}
			if (updateLastBranch) {
				lastBranchInput = nextInput;
			}

			testedSeq.emplace_back(nextInput);
			if (moveAndCheck(currNode, rq, state, testedSeq, ot, teacher)) {
				return true;
			}
			if (ads) {
				auto it = ads->next.find(currNode->incomingOutput);
				if ((it != ads->next.end()) && (!it->second->next.empty())) {
					ads = it->second;
					if ((ot.conjecture->getType() == TYPE_DFSM) && (ads->input == STOUT_INPUT)) {
						it = ads->next.find(currNode->stateOutput);
						if ((it != ads->next.end()) && (!it->second->next.empty())) {//(it->second->input == STOUT_INPUT)) {
							ads = it->second;
						}
					}
					if (ads->input != STOUT_INPUT) {
						auto nn = make_shared<requested_query_node_t>();
						nn->seqCount++;
						rq->next.emplace(ads->input, nn);
					}
				}
			}
		}
		lastBranch->next.erase(lastBranchInput);// no harm even if it is not used (after merging)
		
		// continue querying?
		
		return false;
	}

	unique_ptr<DFSM> SPYlearner(const unique_ptr<Teacher>& teacher, state_t maxExtraStates,
		function<bool(const unique_ptr<DFSM>& conjecture)> provideTentativeModel, bool isEQallowed) {
		if (!teacher->isBlackBoxResettable()) {
			ERROR_MESSAGE("FSMlearning::SPYlearner - the Black Box needs to be resettable");
			return nullptr;
		}

		/// Observation Tree
		SPYObservationTree ot;
		auto numInputs = teacher->getNumberOfInputs();
		ot.conjecture = FSMmodel::createFSM(teacher->getBlackBoxModelType(), 1, numInputs, teacher->getNumberOfOutputs());
		teacher->resetBlackBox();
		auto node = make_shared<spy_node_t>(numInputs); // root
		node->refStates.emplace(0);
		if (ot.conjecture->isOutputState()) {
			node->stateOutput = teacher->outputQuery(STOUT_INPUT);
			checkNumberOfOutputs(teacher, ot.conjecture);
			ot.conjecture->setOutput(0, node->stateOutput);
		}
		ot.bbNode = node;
		ot.numberOfExtraStates = 0;
		ot.repairingInconsistency = 0;
		auto cn = make_shared<convergent_node_t>(node);
		ot.stateNodes.emplace_back(cn);
		ot.stateIdentifier.emplace_back(sequence_set_t());
		generateRequestedQueries(ot);
		node->assumedState = node->state = 0;
		
		sequence_in_t ce;
		sequence_out_t bbOutput;
		ot.testedState = 0;
		ot.testedInput = 0;

		bool unlearned = true;
		while (unlearned) {
			while (ot.unconfirmedTransitions > 0) {
				// choose transition to verify
				//testedState = 0;
				cn = ot.stateNodes[ot.testedState];
				while (cn->requestedQueries->seqCount == 0) {
					cn = ot.stateNodes[++ot.testedState];
				}
				for (const auto& inIt : cn->requestedQueries->next) {
					if (inIt.second->seqCount > 0) {
						ot.testedInput = inIt.first;
						break;
					}
				}

				// verify transition
				auto rqIt = cn->requestedQueries->next.begin();
				do {
					if (queryRequiredSequence(ot.testedState, sequence_in_t({ ot.testedInput }), ot, teacher)) {
						cn = ot.stateNodes[ot.testedState];
					}
					rqIt = cn->requestedQueries->next.find(ot.testedInput);
				} while ((rqIt != cn->requestedQueries->next.end()) && (rqIt->second->seqCount > 0));
				auto reqIt = ot.queriesFromNextState.find(ot.testedState * numInputs + ot.testedInput);
				if (reqIt != ot.queriesFromNextState.end()) {
					auto nextState = ot.conjecture->getNextState(ot.testedState, ot.testedInput);
					while ((ot.numberOfExtraStates > 0) && !reqIt->second.empty()) {
						queryRequiredSequence(nextState, reqIt->second.front(), ot, teacher);
					}
					if (ot.numberOfExtraStates > 0)
						ot.queriesFromNextState.erase(reqIt);
				}
				
				if (provideTentativeModel) {
					unlearned = provideTentativeModel(ot.conjecture);
					if (!unlearned) break;
				}
			}
			if (!unlearned) break;
			ot.numberOfExtraStates++;
			if (ot.numberOfExtraStates > maxExtraStates) {
				if (isEQallowed) {
					/*
					if (!ce.empty()) {
						auto out = ot.conjecture->getOutputAlongPath(0, ce);
						if (bbOutput != out) {
							//updateWithCE(ce, ot, teacher);
							ot.numberOfExtraStates--;
							continue;
						}
						bbOutput.clear();
					}
					ce = teacher->equivalenceQuery(ot.conjecture);
					if (!ce.empty()) {
						ot.numberOfExtraStates--;
						auto currNode = ot.stateNodes[0];// root
						for (auto& input : ce) {
							if (input == STOUT_INPUT) {
								bbOutput.emplace_back(currNode->stateOutput);
								continue;
							}
							auto idx = getNextIdx(currNode, input);
							if (!currNode->next[idx]) {
								query(currNode, idx, ot, teacher);
							}
							currNode = currNode->next[idx];
							bbOutput.emplace_back(currNode->incomingOutput);
						}
						checkAndQueryNext(currNode, ot, teacher);
						if (ot.uncheckedNodes.front() != ot.stateNodes[0]) {// a new state was not found
							updateWithCE(ce, ot, teacher);
						}
						continue;
					}
					*/
				}
				unlearned = false;
				//break;
			} else {
				//update
				generateRequestedQueries(ot);
				// update RQ by subtree of state nodes
				ot.testedState = 0;
				for (auto it = ot.stateNodes.begin(); it != ot.stateNodes.end(); ++it, ot.testedState++) {
					if (!mergeConvergent(ot.testedState, (*it)->convergent.front(), ot, true)) {
						throw "";
					}
				}
				processConfirmedTransitions(ot);
				for (auto it = ot.queriesFromNextState.begin(); it != ot.queriesFromNextState.end();) {
					if (it->second.empty()) {
						it = ot.queriesFromNextState.erase(it);
					}
					else ++it;
				}
				ot.testedState = 0;
			}
		}
		return move(ot.conjecture);
	}
}
