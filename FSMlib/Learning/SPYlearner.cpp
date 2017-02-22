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
	struct convergent_node_t;

	struct spy_node_t {
		sequence_in_t accessSequence;
		output_t incomingOutput;
		output_t stateOutput;
		state_t state;
		state_t assumedState;
		seq_len_t maxSuffixLen;
		weak_ptr<spy_node_t> parent;
		vector<shared_ptr<spy_node_t>> next;
		
		input_t lastQueriedInput;
		set<state_t> refStates;
		weak_ptr<convergent_node_t> convergentNode;

		spy_node_t(input_t numberOfInputs) :
			incomingOutput(DEFAULT_OUTPUT), state(NULL_STATE), stateOutput(DEFAULT_OUTPUT),
			next(numberOfInputs), lastQueriedInput(STOUT_INPUT), assumedState(NULL_STATE), maxSuffixLen(0)
			{
		}

		spy_node_t(const shared_ptr<spy_node_t>& parent, input_t input, 
			output_t transitionOutput, output_t stateOutput, input_t numInputs) : 
			accessSequence(parent->accessSequence), incomingOutput(transitionOutput), state(NULL_STATE),
			stateOutput(stateOutput), next(numInputs), parent(parent), lastQueriedInput(STOUT_INPUT),
			assumedState(NULL_STATE), maxSuffixLen(0) 
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
		vector<shared_ptr<convergent_node_t>> next;
		set<convergent_node_t*> domain;// or consistent cn in case of state nodes
		shared_ptr<requested_query_node_t> requestedQueries;

		convergent_node_t(const shared_ptr<spy_node_t>& node) {
			convergent.emplace_back(node);
			next.resize(node->next.size());
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
		sequence_in_t inconsistentSequence;
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
		ot.inconsistentNodes.emplace_front(diffNode);
		ot.inconsistentNodes.emplace_front(node);
		/*
		auto state = diffNode->assumedState;
		if (state == WRONG_STATE) state = diffNode->state;
		if ((diffNode == ot.stateNodes[state]->convergent.front()) && 
				(diffNode->assumedState != WRONG_STATE) && (node->assumedState != WRONG_STATE)) {
			diffNode->assumedState = WRONG_STATE;
		}
		const auto& refNode = ot.stateNodes[state]->convergent.front();// the sep seq will be quiried from there
		* /
		if (ot.inconsistentNodes.empty() || (node->accessSequence.size() < ot.inconsistentNodes.front()->accessSequence.size())) {
			ot.inconsistentNodes.emplace_front(diffNode);
			ot.inconsistentNodes.emplace_front(node);
		} else {
			ot.inconsistentNodes.emplace_back(node);
			ot.inconsistentNodes.emplace_back(diffNode);
		}
		*/
	}

	static void storeInconsistentNode(const shared_ptr<spy_node_t>& node, SPYObservationTree& ot, bool inconsistent = true) {
		ot.inconsistentNodes.emplace_front(node);
		/*
		if (inconsistent && (ot.inconsistentNodes.empty() || 
				(node->accessSequence.size() < ot.inconsistentNodes.front()->accessSequence.size()))) {
			ot.inconsistentNodes.emplace_front(node);
		}
		else {
			ot.inconsistentNodes.emplace_back(node);
		}*/
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
	
	static bool areNodesDifferentUnder(const shared_ptr<spy_node_t>& n1, const shared_ptr<spy_node_t>& n2, long long len) {
		if ((n1->stateOutput != n2->stateOutput)) return true;
		if ((n1->lastQueriedInput == STOUT_INPUT) || (static_cast<long long>(n2->maxSuffixLen) < len)) return false;
		auto& idx = n1->lastQueriedInput;
		return ((n2->next[idx]) && ((n1->next[idx]->incomingOutput != n2->next[idx]->incomingOutput)
			|| areNodesDifferentUnder(n1->next[idx], n2->next[idx], len - 1)));
 	}

	static bool isIntersectionEmpty(const set<convergent_node_t*>& domain1, const set<convergent_node_t*>& domain2) {
		if (domain1.size() < domain2.size()) {
			for (auto& cn : domain1) {
				if (domain2.count(cn)) return false;
			}
		}
		else {
			for (auto& cn : domain2) {
				if (domain1.count(cn)) return false;
			}
		}
		return true;
	}
	
	static bool areNodeAndConvergentDifferentUnder(const shared_ptr<spy_node_t>& node, convergent_node_t* cn) {
		if ((node->stateOutput != cn->convergent.front()->stateOutput)) return true;
		if ((node->lastQueriedInput == STOUT_INPUT) || (!cn->next[node->lastQueriedInput])) return false;
		auto& idx = node->lastQueriedInput;
		auto it = cn->convergent.begin();
		while (!(*it)->next[idx]) {
			++it;
		}
		return ((node->next[idx]->incomingOutput != (*it)->next[idx]->incomingOutput)
			|| areNodeAndConvergentDifferentUnder(node->next[idx], cn->next[idx].get()));
	}

	static bool areConvergentNodesDistinguished(const shared_ptr<convergent_node_t>& cn1, const shared_ptr<convergent_node_t>& cn2) {
		if (cn1 == cn2) return false;
		if (cn1->convergent.front()->stateOutput != cn2->convergent.front()->stateOutput) return true;
		if ((cn1->requestedQueries && (!cn1->domain.count(cn2.get()))) 
				|| (cn2->requestedQueries && (!cn2->domain.count(cn1.get())))) 
			return true;
		for (input_t i = 0; i < cn1->next.size(); i++) {
			if ((cn1->next[i]) && (cn2->next[i])) {
				auto it1 = cn1->convergent.begin();
				while (!(*it1)->next[i]) {
					++it1;
				}
				auto it2 = cn2->convergent.begin();
				while (!(*it2)->next[i]) {
					++it2;
				}
				if (((*it1)->next[i]->incomingOutput != (*it2)->next[i]->incomingOutput)
					|| areConvergentNodesDistinguished(cn1->next[i], cn2->next[i]))
					return true;
			}
		}
		return false;
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

	static void generateConvergentSubtree(const shared_ptr<convergent_node_t>& cn, SPYObservationTree& ot) {
		const auto& node = cn->convergent.front();
		node->convergentNode = cn;
		for (auto& state : node->refStates) {
			cn->domain.emplace(ot.stateNodes[state].get());
			ot.stateNodes[state]->domain.emplace(cn.get());
		}
		for (input_t input = 0; input < cn->next.size(); input++) {
			if (node->next[input]) {
				if ((node->next[input]->state != NULL_STATE) &&
						(node->next[input] == node->next[input]->convergentNode.lock()->convergent.front())) {
					cn->next[input] = node->next[input]->convergentNode.lock();
					continue;
				}
				cn->next[input] = make_shared<convergent_node_t>(node->next[input]);
				generateConvergentSubtree(cn->next[input], ot);
			}
		}
	}

	static void generateRequestedQueries(SPYObservationTree& ot) {
		size_t tranId = 0;
		vector<state_t> nextStates(ot.conjecture->getNumberOfStates() * ot.conjecture->getNumberOfInputs());
		ot.unconfirmedTransitions = 0;
		ot.queriesFromNextState.clear();
		for (auto& cn : ot.stateNodes) {
			cn->domain.clear();
			cn->next.clear();
			cn->next.resize(ot.conjecture->getNumberOfInputs());
			// clear convergent
			auto stateNode = cn->convergent.front();
			auto state = stateNode->state;
			for (auto& n : cn->convergent) {
				n->state = NULL_STATE;
			}
			stateNode->assumedState = stateNode->state = state;
			cn->convergent.clear();
			cn->convergent.emplace_back(stateNode);

			// init requested queries
			cn->requestedQueries->next.clear();
			cn->requestedQueries->seqCount = 0;
		}
		for (state_t state = 0; state < ot.conjecture->getNumberOfStates(); state++) {
			auto& cn = ot.stateNodes[state];
			auto& stateNode = cn->convergent.front();
			// update QueriesFromNextState, RelatedTransitions and SeqCounts
			for (input_t i = 0; i < ot.conjecture->getNumberOfInputs(); i++, tranId++) {
				auto& succ = stateNode->next[i];
				if (!succ || (succ->state == NULL_STATE)) {// unconfirmed transition
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
						if (succ) {
							cn->next[i] = make_shared<convergent_node_t>(succ);
							generateConvergentSubtree(cn->next[i], ot);
						}
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

	/*
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
		auto it = rq->next.find(node->lastQueriedInput);
		if (it != rq->next.end() && checkRequestedQueries(node, node->lastQueriedInput, ot)) {
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
	*/
	

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
			}
			else {
				domIt = leaf->refStates.emplace_hint(domIt, state);
			}
		}
	}

	static void updateOTreeWithNewState(const shared_ptr<spy_node_t>& node, SPYObservationTree& ot) {
		queue<shared_ptr<spy_node_t>> nodes;
		nodes.emplace(ot.stateNodes[0]->convergent.front());//root
		while (!nodes.empty()) {
			auto otNode = move(nodes.front());
			nodes.pop();
			otNode->assumedState = NULL_STATE;
			if ((node == otNode) || !areNodesDifferent(node, otNode)) {
				otNode->refStates.emplace(node->state);
			}
			if ((otNode->refStates.size() <= 1) && ((otNode->state == NULL_STATE) 
					|| (ot.stateNodes[otNode->state]->convergent.front() != otNode))) {
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

	static sequence_in_t getQueriedSeparatingSequenceFromSN(const shared_ptr<spy_node_t>& n1, const shared_ptr<convergent_node_t>& cn2) {
		if (n1->stateOutput != cn2->convergent.front()->stateOutput) {
			return sequence_in_t();
		}
		queue<pair<shared_ptr<spy_node_t>, shared_ptr<convergent_node_t>>> fifo;
		fifo.emplace(n1, cn2);
		while (!fifo.empty()) {
			auto& p = fifo.front();
			for (input_t i = 0; i < p.first->next.size(); i++) {
				if (p.first->next[i] && p.second->next[i]) {
					auto nIt2 = p.second->convergent.begin();
					while (!(*nIt2)->next[i]) ++nIt2;
					const auto& n2 = (*nIt2)->next[i];
					if ((p.first->next[i]->incomingOutput != n2->incomingOutput)
						|| (p.first->next[i]->stateOutput != n2->stateOutput)) {
						return getAccessSequence(n1, p.first->next[i]);
					}
					fifo.emplace(p.first->next[i], p.second->next[i]);
				}
			}
			fifo.pop();
		}
		return sequence_in_t();
	}

	static sequence_in_t getQueriedSeparatingSequenceOfCN(const shared_ptr<convergent_node_t>& cn1, const shared_ptr<convergent_node_t>& cn2) {
		if (cn1->convergent.front()->stateOutput != cn2->convergent.front()->stateOutput) {
			return sequence_in_t();
		}
		queue<pair<shared_ptr<convergent_node_t>, shared_ptr<convergent_node_t>>> fifo;
		queue<sequence_in_t> seqFifo;
		fifo.emplace(cn1, cn2);
		seqFifo.emplace(sequence_in_t());
		while (!fifo.empty()) {
			auto& p = fifo.front();
			for (input_t i = 0; i < p.first->next.size(); i++) {
				if (p.first->next[i] && p.second->next[i]) {
					auto nIt1 = p.first->convergent.begin();
					while (!(*nIt1)->next[i]) ++nIt1;
					const auto& n1 = (*nIt1)->next[i];
					auto nIt2 = p.second->convergent.begin();
					while (!(*nIt2)->next[i]) ++nIt2;
					const auto& n2 = (*nIt2)->next[i];
					if ((n1->incomingOutput != n2->incomingOutput) || (n1->stateOutput != n2->stateOutput)) {
						seqFifo.front().emplace_back(i);
						return move(seqFifo.front());
					}
					fifo.emplace(p.first->next[i], p.second->next[i]);
					auto seq(seqFifo.front());
					seq.emplace_back(i);
					seqFifo.emplace(move(seq));
				}
			}
			fifo.pop();
			seqFifo.pop();
		}
		return sequence_in_t();
	}

	static sequence_in_t getQueriedSeparatingSequenceForSuccessor(state_t diffState, state_t parentState, 
			const sequence_in_t& transferSeq, SPYObservationTree& ot) {
		sequence_in_t sepSeq;
		list<shared_ptr<spy_node_t>> diffCN, cn;
		queue<shared_ptr<spy_node_t>> nodes;
		nodes.emplace(ot.stateNodes[0]->convergent.front());//root
		while (!nodes.empty()) {
			auto node = move(nodes.front());
			nodes.pop();
			if (node->assumedState == diffState) {
				for (const auto& n : cn) {
					sepSeq = getQueriedSeparatingSequence(node, n);
					if (!sepSeq.empty()) {
						return sepSeq;
					}
				}
				diffCN.emplace_back(node);
			} else if (node->assumedState == parentState) {
				auto n = node;
				for (auto& input : transferSeq) {
					n = n->next[input];
					if (!n) break;
				}
				if (n) {
					for (const auto& diffN : diffCN) {
						sepSeq = getQueriedSeparatingSequence(n, diffN);
						if (!sepSeq.empty()) {
							return sepSeq;
						}
					}
					cn.emplace_back(n);
				}
			}
			for (const auto& nn : node->next) {
				if (nn) nodes.emplace(nn);
			}
		}
		return sepSeq;
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

	
	static bool mergeConvergent(state_t state, const shared_ptr<spy_node_t>& spyNode, SPYObservationTree& ot, bool forceMerge = false) {

		if (!spyNode->refStates.count(state)) {// discrepancy found
			spyNode->assumedState = state;
			storeInconsistentNode(spyNode, ot);
			return false;
		}
		if ((spyNode->state != NULL_STATE) && !forceMerge) {
			// already merged + hack for refNode
			return true;
		}

		spyNode->assumedState = spyNode->state = state;
		const auto& cn = ot.stateNodes[state];
		spyNode->convergentNode = cn;
		// updating requestedQueries 
		sequence_in_t seq;
		updateRequestedQueries(cn->requestedQueries, spyNode, state, seq, ot);

		for (input_t i = 0; i < ot.conjecture->getNumberOfInputs(); i++) {
			if ((spyNode->next[i]) && (cn->next[i])) {
				if (!mergeConvergent(cn->next[i]->convergent.front()->state, spyNode->next[i], ot)) {
					cn->convergent.emplace_back(spyNode);
					return false;
				}
			}
		}

		if (spyNode->accessSequence.size() < cn->convergent.front()->accessSequence.size()) {
			//change refStates
			//cn->convergent.emplace_front(spyNode);
			cn->convergent.emplace_back(spyNode);
		}
		else if (spyNode != cn->convergent.front()) {
			cn->convergent.emplace_back(spyNode);
		}
		return true;
	}

	static bool processConfirmedTransitions(SPYObservationTree& ot) {
		while (!ot.unprocessedConfirmedTransition.empty()) {
			auto state = ot.unprocessedConfirmedTransition.front().first;
			auto input = ot.unprocessedConfirmedTransition.front().second;
			auto nextState = ot.conjecture->getNextState(state, input);
			auto& fromCN = ot.stateNodes[state];// ->next[input];
			ot.stateNodes[state]->next[input] = ot.stateNodes[nextState];
			for (auto& fcn : fromCN->convergent) {
				if (fcn->next[input]) {
					if (!mergeConvergent(nextState, fcn->next[input], ot))  {
						ot.unprocessedConfirmedTransition.clear();
						return false;
					}
				}
			}
			ot.unprocessedConfirmedTransition.pop_front();
			ot.unconfirmedTransitions--;
		}
		return true;
	}

	static bool makeStateNode(shared_ptr<spy_node_t> node, SPYObservationTree& ot, const unique_ptr<Teacher>& teacher);
	
	static bool checkNode(const shared_ptr<spy_node_t>& node, SPYObservationTree& ot) {
		if (node->refStates.empty()) {// new state
			storeIdentifiedNode(node, ot);
			return false;
		}
		else if ((node->assumedState != NULL_STATE) && !node->refStates.count(node->assumedState)) {// inconsistent node
			storeInconsistentNode(node, ot);
			ot.unprocessedConfirmedTransition.clear();
			return false;
		}
		return true;
	}

	static bool checkAgainstAllNodes(const shared_ptr<spy_node_t>& node, const long long& suffixLen, SPYObservationTree& ot) {
		bool isConsistent = true;
		stack<shared_ptr<spy_node_t>> nodes;
		nodes.emplace(ot.stateNodes[0]->convergent.front());//root
		while (!nodes.empty()) {
			auto otNode = move(nodes.top());
			nodes.pop();
			if ((otNode != node) && (otNode->refStates.count(node->state))) {
				if (areNodesDifferentUnder(node, otNode, suffixLen)) {
					otNode->refStates.erase(node->state);
					isConsistent &= checkNode(otNode, ot);
				}
			}
			for (const auto& nn : otNode->next) {
				if (nn && (static_cast<long long>(nn->maxSuffixLen) >= suffixLen)) nodes.emplace(nn);
			}
		}
		return isConsistent;
	}

	static bool checkPrevious(shared_ptr<spy_node_t> node, SPYObservationTree& ot, long long suffixAdded = 0) {
		bool isConsistent = true;
		seq_len_t suffixLen = 0;
		input_t input(STOUT_INPUT);
		do {
			node->lastQueriedInput = input;
			if (suffixLen > node->maxSuffixLen) node->maxSuffixLen = suffixLen;
			if ((node->state == NULL_STATE) || (node != ot.stateNodes[node->state]->convergent.front())) {
				for (auto snIt = node->refStates.begin(); snIt != node->refStates.end();) {
					if (areNodesDifferentUnder(node, ot.stateNodes[*snIt]->convergent.front(), suffixAdded)) {
						snIt = node->refStates.erase(snIt);
					}
					else ++snIt;
				}
				isConsistent &= checkNode(node, ot);
			}
			else {// state node
				isConsistent &= checkAgainstAllNodes(node, suffixAdded, ot);
			}
			if (!node->accessSequence.empty()) input = node->accessSequence.back();
			node = node->parent.lock();
			suffixLen++; suffixAdded++;
		} while (node);
		return isConsistent;
	}

	static bool moveAndCheck(shared_ptr<spy_node_t>& currNode, shared_ptr<requested_query_node_t>& rq, state_t state,
		const sequence_in_t& testedSeq, SPYObservationTree& ot, const unique_ptr<Teacher>& teacher) {
		auto& nextInput = testedSeq.back();
		if (currNode->next[nextInput]) {
			currNode = currNode->next[nextInput];
			return true;
		}
		query(currNode, nextInput, ot, teacher);
		currNode->next[nextInput]->assumedState = ot.conjecture->getNextState(currNode->assumedState, nextInput);
		if ((currNode->state != NULL_STATE) && (ot.stateNodes[currNode->state]->next[nextInput])) {
			mergeConvergent(ot.stateNodes[currNode->state]->next[nextInput]->convergent.front()->state, currNode->next[nextInput], ot);
		}
		currNode = currNode->next[nextInput];
		processRelatedTransitions(rq, state, testedSeq, ot);
		bool isConsistent = checkPrevious(currNode, ot);
		if (!ot.identifiedNodes.empty()) {// new state
			isConsistent &= makeStateNode(move(ot.identifiedNodes.front()), ot, teacher);
		}
		else {
			isConsistent &= processConfirmedTransitions(ot);
		}
		return isConsistent;
	}

	static bool queryRequiredSequence(state_t state, sequence_in_t seq, SPYObservationTree& ot, const unique_ptr<Teacher>& teacher) {
		auto rq = ot.stateNodes[state]->requestedQueries;
		auto currNode = ot.stateNodes[state]->convergent.front();// shortest access sequence
		sequence_in_t testedSeq;
		auto lastBranch = rq;
		auto lastBranchInput = seq.front();
		for (auto nextInput : seq) {
			if (rq->next.size() > 1) {
				lastBranch = rq;
				lastBranchInput = nextInput;
			}
			rq = rq->next[nextInput];
			testedSeq.emplace_back(nextInput);
			if (!moveAndCheck(currNode, rq, state, testedSeq, ot, teacher)) {
				return true;
			}
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
			if (!moveAndCheck(currNode, rq, state, testedSeq, ot, teacher)) {
				return true;
			}
		}
		lastBranch->next.erase(lastBranchInput);// no harm even if it is not used (after merging)

		// continue querying?

		return false;
	}


	static bool querySequenceAndCheck(shared_ptr<spy_node_t> currNode, const sequence_in_t& seq,
		SPYObservationTree& ot, const unique_ptr<Teacher>& teacher, bool allowIdentification = true) {
		//seq_len_t suffixLen = seq.size();
		long long suffixAdded = 0;
		/*
		if (currNode != ot.stateNodes[0]->convergent.front()) {
			suffixLen += currNode->accessSequence.size();
			auto n = ot.stateNodes[0]->convergent.front();
			for (auto& input : currNode->accessSequence) {
				if (n->maxSuffixLen < suffixLen) {
					n->maxSuffixLen = suffixLen;
				}
				suffixLen--;
				n = n->next[input];
			}
		}*/
		for (auto& input : seq) {
			/*
			if (currNode->maxSuffixLen < suffixLen) {
				currNode->maxSuffixLen = suffixLen;
			}
			suffixLen--;*/
			if (!currNode->next[input]) {
				query(currNode, input, ot, teacher);
				suffixAdded--;
			}
			currNode = currNode->next[input];
		}
		bool isConsistent = checkPrevious(move(currNode), ot, suffixAdded);
		if (allowIdentification) {
			if (!ot.identifiedNodes.empty()) {// new state
				isConsistent &= makeStateNode(move(ot.identifiedNodes.front()), ot, teacher);
			}
			else {
				isConsistent &= processConfirmedTransitions(ot);
			}
		}
		return isConsistent;
	}


	static bool mergeConvergentNoES(shared_ptr<convergent_node_t>& fromCN, const shared_ptr<convergent_node_t>& toCN, SPYObservationTree& ot) {
		if (toCN->requestedQueries) {// a state node
			if (fromCN->domain.find(toCN.get()) == fromCN->domain.end()) {
				storeInconsistentNode(fromCN->convergent.front(), toCN->convergent.front(), ot);
				return false;
			}
			for (auto& consistent : fromCN->domain) {
				consistent->domain.erase(fromCN.get());
			}
			//fromCN->domain.clear();
			for (auto toIt = toCN->domain.begin(); toIt != toCN->domain.end();) {
				if (areConvergentNodesDistinguished(fromCN, (*toIt)->convergent.front()->convergentNode.lock())) {
					(*toIt)->domain.erase(toCN.get());
					if ((*toIt)->domain.empty()) {
						auto& n = fromCN->convergent.front();
						n->convergentNode = fromCN;
						//n->assumedState = WRONG_STATE;
						toCN->convergent.remove(n);
						storeInconsistentNode((*toIt)->convergent.front(), n, ot);
						return false;
					}
					else if ((*toIt)->domain.size() == 1) {
						storeIdentifiedNode((*toIt)->convergent.front(), ot);
					}
					toIt = toCN->domain.erase(toIt);
				}
				else {
					++toIt;
				}
			}
			auto& state = toCN->convergent.front()->state;
			for (auto& node : fromCN->convergent) {
				node->assumedState = node->state = state;
				node->convergentNode = toCN;
				//if (node->accessSequence.size() < toCN->convergent.front()->accessSequence.size()) swap ref state nodes
				// TODO
				toCN->convergent.emplace_back(node);
			}
		}
		else {
			bool reduced = false;
			// intersection of domains
			auto toIt = toCN->domain.begin();
			auto fromIt = fromCN->domain.begin();
			while ((toIt != toCN->domain.end()) && (fromIt != fromCN->domain.end())) {
				if (*toIt < *fromIt) {
					(*toIt)->domain.erase(toCN.get());
					toIt = toCN->domain.erase(toIt);
					reduced = true;
				}
				else {
					if (!(*fromIt < *toIt)) {
						++toIt;
					}
					(*fromIt)->domain.erase(fromCN.get());
					++fromIt;
				}
			}
			while (toIt != toCN->domain.end()) {
				(*toIt)->domain.erase(toCN.get());
				toIt = toCN->domain.erase(toIt);
				reduced = true;
			}
			while (fromIt != fromCN->domain.end()) {
				(*fromIt)->domain.erase(fromCN.get());
				++fromIt;
			}
			//fromCN->domain.clear();
			for (auto& node : fromCN->convergent) {
				node->convergentNode = toCN;
				if (node->accessSequence.size() < toCN->convergent.front()->accessSequence.size()) {
					toCN->convergent.emplace_front(node);
				}
				else {
					toCN->convergent.emplace_back(node);
				}
			}
			if (toCN->domain.empty()) {
				auto& n = fromCN->convergent.front();
				n->convergentNode = fromCN;
				toCN->convergent.remove(n);
				storeInconsistentNode(n, n, ot);
				return false;
			}
			if (reduced && (toCN->domain.size() == 1)) {
				storeIdentifiedNode(toCN->convergent.front(), ot);
			}
		}
		auto tmpCN = fromCN;
		fromCN = toCN;
		// merge successors
		for (input_t i = 0; i < ot.conjecture->getNumberOfInputs(); i++) {
			if (tmpCN->next[i]) {
				if (toCN->next[i]) {
					if (tmpCN->next[i] == toCN->next[i]) continue;
					if (tmpCN->next[i]->requestedQueries) {
						if (toCN->next[i]->requestedQueries) {// a different state node
							storeInconsistentNode(tmpCN->next[i]->convergent.front(), toCN->next[i]->convergent.front(), ot);
							ot.inconsistentSequence.emplace_front(i);
							fromCN = tmpCN;
							return false;
						}
						if (!mergeConvergentNoES(toCN->next[i], tmpCN->next[i], ot)) {
							ot.inconsistentSequence.emplace_front(i);
							fromCN = tmpCN;
							return false;
						}
						if (toCN->requestedQueries) {// state node
							auto it = toCN->requestedQueries->next.find(i);
							if (it != toCN->requestedQueries->next.end()) {
								auto nIt = toCN->convergent.begin();
								while (!(*nIt)->next[i]) ++nIt;
								auto& node = (*nIt)->next[i];
								ot.conjecture->setTransition((*nIt)->state, i, node->state,
									(ot.conjecture->isOutputTransition() ? node->incomingOutput : DEFAULT_OUTPUT));
								ot.unconfirmedTransitions--;
								toCN->requestedQueries->seqCount--;
								toCN->requestedQueries->next.erase(it);
							}
						}
					}
					else {
						if (!mergeConvergentNoES(tmpCN->next[i], toCN->next[i], ot)) {
							ot.inconsistentSequence.emplace_front(i);
							fromCN = tmpCN;
							return false;
						}
					}
				}
				else {
					toCN->next[i].swap(tmpCN->next[i]);
					if (toCN->requestedQueries && toCN->next[i]->requestedQueries) {// state nodes
						auto it = toCN->requestedQueries->next.find(i);
						if (it != toCN->requestedQueries->next.end()) {
							auto nIt = toCN->convergent.begin();
							while (!(*nIt)->next[i]) ++nIt;
							auto& node = (*nIt)->next[i];
							ot.conjecture->setTransition((*nIt)->state, i, node->state,
								(ot.conjecture->isOutputTransition() ? node->incomingOutput : DEFAULT_OUTPUT));
							ot.unconfirmedTransitions--;
							toCN->requestedQueries->seqCount--;
							toCN->requestedQueries->next.erase(it);
						}
					}
				}
			}
		}
		return true;
	}

	static bool checkPredecessors(shared_ptr<spy_node_t> node, shared_ptr<convergent_node_t> parentCN, 
			const shared_ptr<convergent_node_t>& refCN, SPYObservationTree& ot) {
		auto input = node->accessSequence.back();
		bool reduced = false;
		for (auto it = parentCN->domain.begin(); it != parentCN->domain.end();) {
			if ((*it)->next[input] && 
				(((*it)->next[input]->requestedQueries && 
					((refCN && ((*it)->next[input] != refCN)) || 
					(!refCN && (!(*it)->next[input]->domain.count(parentCN->next[input].get())))))
				|| 
				(!(*it)->next[input]->requestedQueries && 
					((refCN && !(*it)->next[input]->domain.count(refCN.get())) || 
					(!refCN && isIntersectionEmpty(parentCN->next[input]->domain, (*it)->next[input]->domain)))))) {
				(*it)->domain.erase(parentCN.get());
				it = parentCN->domain.erase(it);
				reduced = true;
			}
			else ++it;
		}
		while (reduced) {
			if (parentCN->domain.empty()) {
				auto& n = parentCN->convergent.front();
				//n->assumedState = WRONG_STATE;
				storeInconsistentNode(n, ot);//node, 
				ot.identifiedNodes.clear();
				return false;
			}
			if (parentCN->domain.size() == 1) {
				storeIdentifiedNode(parentCN->convergent.front(), ot);
				break;
			}
			else {
				node = parentCN->convergent.front();
				parentCN = node->parent.lock()->convergentNode.lock();
				reduced = false;
				if (!parentCN->requestedQueries) {// not a state node
					input = node->accessSequence.back();
					for (auto it = parentCN->domain.begin(); it != parentCN->domain.end();) {
						if ((*it)->next[input] && (((*it)->next[input]->requestedQueries &&
							(!(*it)->next[input]->domain.count(parentCN->next[input].get())))
							|| (!(*it)->next[input]->requestedQueries &&
							isIntersectionEmpty(parentCN->next[input]->domain, (*it)->next[input]->domain)))) {
							(*it)->domain.erase(parentCN.get());
							it = parentCN->domain.erase(it);
							reduced = true;
						}
						else ++it;
					}
				}
			}
		}
		return true;
	}

	static bool processIdentified(SPYObservationTree& ot) {
		while (!ot.identifiedNodes.empty()) {
			auto node = move(ot.identifiedNodes.front());
			ot.identifiedNodes.pop_front();
			if (node->state == NULL_STATE) {
				auto parentCN = node->parent.lock()->convergentNode.lock();
				auto input = node->accessSequence.back();
				auto& refCN = (*(node->convergentNode.lock()->domain.begin()))->convergent.front()->convergentNode.lock();
				if (!mergeConvergentNoES(parentCN->next[input], refCN, ot)) {
					refCN->convergent.remove(node);
					node->state = NULL_STATE;
					node->convergentNode = parentCN->next[input];
					storeInconsistentNode(node, refCN->convergent.front(), ot);
					ot.identifiedNodes.clear();
					return false;
				}
				if (parentCN->requestedQueries) {// state node
					auto it = parentCN->requestedQueries->next.find(input);
					if (it != parentCN->requestedQueries->next.end()) {
						ot.conjecture->setTransition(parentCN->convergent.front()->state, input, node->state,
							(ot.conjecture->isOutputTransition() ? node->incomingOutput : DEFAULT_OUTPUT));
						ot.unconfirmedTransitions--;
						parentCN->requestedQueries->seqCount--;
						parentCN->requestedQueries->next.erase(it);
					}
					else {
						ot.identifiedNodes.clear();
						return false;
					}
				} else {

					if (!checkPredecessors(node, parentCN, refCN, ot)) {
						return false;
					}
				}
			}
			else {
				node->state = node->assumedState;
			}
		}
		return true;
	}

	static bool makeStateNode(shared_ptr<spy_node_t> node, SPYObservationTree& ot, const unique_ptr<Teacher>& teacher) {
		auto parent = node->parent.lock();
		if ((parent->state == NULL_STATE) || (ot.stateNodes[parent->state]->convergent.front() != parent)) {
			ot.identifiedNodes.clear();
			auto state = parent->assumedState;
			if (state == NULL_STATE) {
				if (parent->refStates.size() > 1) {
					return false;
				}
				state = *(parent->refStates.begin());
			}
			else if (!parent->refStates.count(state)) {
				return false;
			}
			auto newSN = ot.stateNodes[state]->convergent.front();
			auto& input = node->accessSequence.back();
			if (!newSN->next[input]) {
				query(newSN, input, ot, teacher);
				newSN = newSN->next[input];
				checkPrevious(newSN, ot);
			}
			else {
				newSN = newSN->next[input];
			}
			while (!newSN->refStates.empty() && !parent->refStates.empty()) {
				auto state = *(newSN->refStates.begin());
				sequence_in_t sepSeq;
				if ((node->assumedState == NULL_STATE) || (node->assumedState == state) ||
					!isSeparatingSequenceQueried(node, state, sepSeq, ot)) {
					sepSeq = getQueriedSeparatingSequence(node, ot.stateNodes[state]->convergent.front());
				}
				querySequenceAndCheck(newSN, sepSeq, ot, teacher, false);
			}
			if (newSN->refStates.empty()) {
				node = newSN;
			} else {
				return makeStateNode(parent, ot, teacher);
			}
		}

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
		ot.stateNodes.back()->requestedQueries = make_shared<requested_query_node_t>();
		auto& cn = node->convergentNode.lock();
		if (cn) {
			cn->convergent.remove(node);
		}
		node->convergentNode = ot.stateNodes.back();

		// update OTree with new state and fill ot.identifiedNodes with nodes with |domain|<= 1
		ot.identifiedNodes.clear();
		updateOTreeWithNewState(node, ot);

		ot.numberOfExtraStates = 0;
		if (!ot.identifiedNodes.empty() && (ot.identifiedNodes.front()->refStates.empty())) {// new state
			return makeStateNode(move(ot.identifiedNodes.front()), ot, teacher);
		}
		ot.testedState = 0;
		generateRequestedQueries(ot);

		ot.inconsistentSequence.clear();
		ot.inconsistentNodes.clear();
		return processIdentified(ot);
	}

	static bool checkNodeNoES(const shared_ptr<spy_node_t>& node, SPYObservationTree& ot) {
		if (node->refStates.empty()) {// new state
			storeIdentifiedNode(node, ot);
			return false;
		}
		else if ((node->assumedState != NULL_STATE) && !node->refStates.count(node->assumedState)) {// inconsistent node
			storeInconsistentNode(node, ot);
			return false;
		}
		else if (node->state == NULL_STATE) {
			if (node->convergentNode.lock()->domain.empty()) {
				storeInconsistentNode(node, ot);//node, 
				return false;
			} else if (node->convergentNode.lock()->domain.size() == 1) {
				storeIdentifiedNode(node, ot);//->convergentNode.lock()->convergent.front()
			}
		}
		return true;
	}

	static bool checkAgainstAllNodesNoES(const shared_ptr<spy_node_t>& node, const seq_len_t& suffixLen, SPYObservationTree& ot) {
		bool isConsistent = true;
		stack<shared_ptr<spy_node_t>> nodes;
		nodes.emplace(ot.stateNodes[0]->convergent.front());//root
		while (!nodes.empty()) {
			auto otNode = move(nodes.top());
			nodes.pop();
			if ((otNode != node) && (otNode->refStates.count(node->state))) {
				if (areNodesDifferentUnder(node, otNode, suffixLen)) {
					otNode->refStates.erase(node->state);
					if (otNode->convergentNode.lock()->domain.erase(ot.stateNodes[node->state].get())) {
						ot.stateNodes[node->state]->domain.erase(otNode->convergentNode.lock().get());
						
						auto parentCN = otNode->parent.lock()->convergentNode.lock();
						if (!parentCN->requestedQueries) {
							isConsistent &= checkPredecessors(otNode, parentCN, nullptr, ot);
						}
					}
					isConsistent &= checkNodeNoES(otNode, ot);
				}
			}
			for (const auto& nn : otNode->next) {
				if (nn && (nn->maxSuffixLen >= suffixLen)) nodes.emplace(nn);
			}
		}
		return isConsistent;
	}

	static void reduceDomainNoES(const shared_ptr<spy_node_t>& node, const seq_len_t& suffixLen, bool checkCN, SPYObservationTree& ot) {
		const auto& cn = node->convergentNode.lock();
		for (auto snIt = node->refStates.begin(); snIt != node->refStates.end();) {
			if (areNodesDifferentUnder(node, ot.stateNodes[*snIt]->convergent.front(), suffixLen)) {
				if (cn->domain.erase(ot.stateNodes[*snIt].get())) {
					ot.stateNodes[*snIt]->domain.erase(cn.get());
				}
				snIt = node->refStates.erase(snIt);
			}
			else {
				if (checkCN && (node->state == NULL_STATE)) {
					auto cnIt = cn->domain.find(ot.stateNodes[*snIt].get());
					if (cnIt != cn->domain.end()) {
						if (areNodeAndConvergentDifferentUnder(node, ot.stateNodes[*snIt].get())) {
							cn->domain.erase(cnIt);
							ot.stateNodes[*snIt]->domain.erase(cn.get());
						}
					}
				}
				++snIt;
			}
		}
		if (checkCN && (node->state != NULL_STATE)) {// convergent state node
			for (auto cnIt = cn->domain.begin(); cnIt != cn->domain.end();) {
				if (areNodeAndConvergentDifferentUnder(node, *cnIt)) {
					(*cnIt)->domain.erase(cn.get());
					cnIt = cn->domain.erase(cnIt);
				}
				else ++cnIt;
			}
		}
	}

	static bool checkPreviousNoES(shared_ptr<spy_node_t> node, SPYObservationTree& ot, const unique_ptr<Teacher>& teacher) {
		bool isConsistent = ot.inconsistentNodes.empty();
		seq_len_t suffixLen = 0;
		input_t input(STOUT_INPUT);
		do {
			node->lastQueriedInput = input;
			if (suffixLen > node->maxSuffixLen) node->maxSuffixLen = suffixLen;
			if ((node->state == NULL_STATE) || (node != ot.stateNodes[node->state]->convergent.front())) {
				reduceDomainNoES(node, suffixLen, isConsistent, ot);
				isConsistent &= checkNodeNoES(node, ot);
			} else {// reference state node
				isConsistent &= checkAgainstAllNodesNoES(node, suffixLen, ot);
			}
			if (!node->accessSequence.empty()) input = node->accessSequence.back();
			node = node->parent.lock();
			suffixLen++;
		} while (node);

		if (!ot.identifiedNodes.empty()) {
			if (ot.identifiedNodes.front()->refStates.empty()) {// new state
				isConsistent &= makeStateNode(move(ot.identifiedNodes.front()), ot, teacher);
			} else if (isConsistent) {// no inconsistency
				isConsistent &= processIdentified(ot);
			} else {
				ot.identifiedNodes.clear();
			}
		}
		return isConsistent;
	}
	
	static bool moveAndCheckNoES(shared_ptr<spy_node_t>& currNode, input_t input, SPYObservationTree& ot, const unique_ptr<Teacher>& teacher) {
		if (currNode->next[input]) {
			currNode = currNode->next[input];
			return true;
		}
		query(currNode, input, ot, teacher);
		auto cn = currNode->convergentNode.lock();
		if (cn->next[input]) {
			auto it = cn->convergent.begin();
			while ((*it == currNode) || (!(*it)->next[input])) {
				++it;
			}
			auto& refNode = (*it)->next[input];
			auto& currNext = currNode->next[input];
			if ((refNode->incomingOutput != currNext->incomingOutput) ||
				(refNode->stateOutput != currNext->stateOutput)) {
				currNext->assumedState = WRONG_STATE;
				//storeInconsistentNode(currNext, ot);//refNode
				/*
				if ((cn->convergent.front() == currNode) && (currNode->state == NULL_STATE)) {
					cn->convergent.pop_front();
					cn->convergent.push_back(currNode);
				}*/
			}
			currNode = currNext;
			auto& nextCN = cn->next[input];
			if (nextCN->requestedQueries) {// state node
				currNode->state = nextCN->convergent.front()->state;
				if (currNode->assumedState != WRONG_STATE) {
					currNode->assumedState = currNode->state;
				}
				//cn->next[input]->convergent.emplace_front(currNode);
				nextCN->convergent.emplace_back(currNode);
			}
			else {/*
				for (auto state : currNode->refStates) {
					nextCN->domain.emplace(ot.stateNodes[state].get());
					ot.stateNodes[state]->domain.emplace(nextCN.get());
				}*/
				if (nextCN->convergent.front()->accessSequence.size() > currNode->accessSequence.size()) {
					nextCN->convergent.emplace_front(currNode);
				}
				else {
					nextCN->convergent.emplace_back(currNode);
				}
			}
		} else {
			currNode = currNode->next[input];
			auto nextCN = make_shared<convergent_node_t>(currNode);
			for (auto state : currNode->refStates) {
				nextCN->domain.emplace(ot.stateNodes[state].get());
				ot.stateNodes[state]->domain.emplace(nextCN.get());
			}
			cn->next[input] = move(nextCN);
		}
		currNode->convergentNode = cn->next[input];
		return checkPreviousNoES(currNode, ot, teacher);
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

	static bool identifyNextState(SPYObservationTree& ot, const unique_ptr<Teacher>& teacher) {
		auto currNode = ot.stateNodes[ot.testedState]->convergent.front();// shortest access sequence
		
		if (!moveAndCheckNoES(currNode, ot.testedInput, ot, teacher)) {
			return true;
		}
		if (currNode->state != NULL_STATE) {// identified
			// optimization - apply the same input again
			if (!moveAndCheckNoES(currNode, ot.testedInput, ot, teacher)) {
				return true;
			}
			return false;
		}
		auto cn = currNode->convergentNode.lock();
		if (cn->domain.size() < 2) {
			return true;
		}
		if (cn->domain.size() == 2) {// query sepSeq
			auto idx = FSMsequence::getStatePairIdx(
				(*(cn->domain.begin()))->convergent.front()->state, 
				(*(cn->domain.rbegin()))->convergent.front()->state);
			auto& sepSeq = ot.separatingSequences[idx];
			for (auto input : sepSeq) {
				if (!moveAndCheckNoES(currNode, input, ot, teacher)) {
					return true;
				}
			}
		} else {
			// choose ads
			auto ads = make_shared<ads_cv_t>();
			for (auto& sn : cn->domain) {
				ads->nodes.push_back(sn->convergent);
			}
			seq_len_t totalLen = 0;
			frac_t bestVal(1), currVal(0);
			chooseADS(ads, ot, bestVal, currVal, totalLen);
			while (ads->input != STOUT_INPUT) {
				if (!moveAndCheckNoES(currNode, ads->input, ot, teacher)) {
					return true;
				}
				auto it = ads->next.find(currNode->incomingOutput);
				if (it != ads->next.end()) {// && (!it->second->next.empty())) {
					ads = it->second;
					if ((ot.conjecture->getType() == TYPE_DFSM) && (ads->input == STOUT_INPUT)) {
						it = ads->next.find(currNode->stateOutput);
						if (it != ads->next.end()) {//&& (!it->second->next.empty())) {//(it->second->input == STOUT_INPUT)) {
							ads = it->second;
						}
						else {
							break;
						}
					}
				}
				else {
					break;
				}
			}
		}
		
		// continue querying?

		return false;
	}

	
	static shared_ptr<spy_node_t> queryIfNotQueried(shared_ptr<spy_node_t> node, sequence_in_t seq, SPYObservationTree& ot, const unique_ptr<Teacher>& teacher) {
		while (!seq.empty()) {
			if (!node->next[seq.front()]) {
				if (!querySequenceAndCheck(node, seq, ot, teacher)) {
					return nullptr;
				}
				return ot.bbNode;
			}
			node = node->next[seq.front()];
			seq.pop_front();
		}
		return move(node);
	}
	
	static void processInconsistent(SPYObservationTree& ot, const unique_ptr<Teacher>& teacher) {
		auto n1 = move(ot.inconsistentNodes.front());
		ot.inconsistentNodes.pop_front();
		if ((n1->assumedState != NULL_STATE) && (!n1->refStates.count(n1->assumedState))) {
			sequence_in_t sepSeq;
			sepSeq.emplace_back(n1->accessSequence.back());
			auto n2 = n1->parent.lock();
			while (n2->assumedState == NULL_STATE) {
				sepSeq.emplace_front(n2->accessSequence.back());
				n2 = n2->parent.lock();
			}
			if ((n2->state != NULL_STATE) && (n2 == ot.stateNodes[n2->state]->convergent.front())) {
				auto cn2 = n2->convergentNode.lock();
				auto transferSeq = move(sepSeq);
				auto domain = n1->refStates;
				for (auto state : domain) {
					sepSeq.clear();
					for (auto n : cn2->convergent) {
						for (auto& input : transferSeq) {
							n = n->next[input];
							if (!n) break;
						}
						if (n) {
							sepSeq = getQueriedSeparatingSequence(n, ot.stateNodes[state]->convergent.front());
							if (!sepSeq.empty()) {
								break;
							}
						}
					}
					if (sepSeq.empty()) {
						if (ot.numberOfExtraStates == 0) {
							for (auto n : cn2->convergent) {
								for (auto& input : transferSeq) {
									n = n->next[input];
									if (!n) break;
								}
								if (n) {
									sepSeq = getQueriedSeparatingSequenceFromSN(n, ot.stateNodes[state]);
									if (!sepSeq.empty()) {
										break;
									}
								}
							}
						} else {
							sepSeq = getQueriedSeparatingSequenceForSuccessor(state, n2->state, transferSeq, ot);
						}
						if (sepSeq.empty()) {
							return;
						}
						if (!queryIfNotQueried(ot.stateNodes[state]->convergent.front(), sepSeq, ot, teacher)) {
							return;
						}
					}
					if (!queryIfNotQueried(n1, move(sepSeq), ot, teacher)) {
						return;
					}
				} 
			} else {
				if (n1->assumedState != WRONG_STATE) {
					if ((ot.numberOfExtraStates == 0) && isPrefix(n1->accessSequence, ot.bbNode->accessSequence)) {
						sepSeq.splice(sepSeq.end(), getQueriedSeparatingSequenceFromSN(n1, ot.stateNodes[n1->assumedState]));
					} else {
						sepSeq.splice(sepSeq.end(), getQueriedSeparatingSequence(n1, ot.stateNodes[n1->assumedState]->convergent.front()));
					}
				}
				if (!querySequenceAndCheck(ot.stateNodes[n2->assumedState]->convergent.front(), sepSeq, ot, teacher)) {
					return;
				}
			}
		}
		else {
			auto& cn1 = n1->convergentNode.lock();
			if (cn1->domain.empty()) {
				auto n2 = n1->parent.lock();
				sequence_in_t sepSeq;
				sepSeq.emplace_back(n1->accessSequence.back());
				while (n2->assumedState == NULL_STATE) {
					sepSeq.emplace_front(n2->accessSequence.back());
					n2 = n2->parent.lock();
				}
				if (n2 != ot.stateNodes[n2->state]->convergent.front()) {
					n2 = ot.stateNodes[n2->state]->convergent.front();
					n1 = queryIfNotQueried(n2, move(sepSeq), ot, teacher);
					if (!n1) {
						return;
					}
				}
				auto& cn1 = n1->convergentNode.lock();
				auto domain = n1->refStates;
				for (auto state : domain) {
					sepSeq = getQueriedSeparatingSequenceOfCN(cn1, ot.stateNodes[state]);
					if (!queryIfNotQueried(n1, sepSeq, ot, teacher)) {
						return;
					}
					if (!queryIfNotQueried(ot.stateNodes[state]->convergent.front(), move(sepSeq), ot, teacher)) {
						return;
					}
				}
			}
			else {
				auto n2 = move(ot.inconsistentNodes.front());
				ot.inconsistentNodes.pop_front();
				auto fn1 = move(ot.inconsistentNodes.front());
				ot.inconsistentNodes.pop_front();
				auto fn2 = move(ot.inconsistentNodes.front());
				ot.inconsistentNodes.pop_front();
				n2 = queryIfNotQueried(n2, ot.inconsistentSequence, ot, teacher);
				if (!n2) {
					return;
				}
				if (fn1 == fn2) {// empty domain
					auto& cn1 = fn1->convergentNode.lock();
					auto domain = n2->refStates;
					for (auto state : domain) {
						auto sepSeq = getQueriedSeparatingSequenceOfCN(cn1, ot.stateNodes[state]);
						if (!queryIfNotQueried(n2, sepSeq, ot, teacher)) {
							return;
						}
						if (!queryIfNotQueried(ot.stateNodes[state]->convergent.front(), move(sepSeq), ot, teacher)) {
							return;
						}
					}
				}
				else {
					auto sepSeq = getQueriedSeparatingSequenceOfCN(fn1->convergentNode.lock(), fn2->convergentNode.lock());
					if (!queryIfNotQueried(n2, sepSeq, ot, teacher)) {
						return;
					}
					ot.inconsistentSequence.splice(ot.inconsistentSequence.end(), move(sepSeq));
					if (!queryIfNotQueried(n1, move(ot.inconsistentSequence), ot, teacher)) {
						return;
					}

				}			
			}
		}
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
		node->state = 0;
		if (ot.conjecture->isOutputState()) {
			node->stateOutput = teacher->outputQuery(STOUT_INPUT);
			checkNumberOfOutputs(teacher, ot.conjecture);
			ot.conjecture->setOutput(0, node->stateOutput);
		}
		ot.bbNode = node;
		ot.numberOfExtraStates = 0;
		auto cn = make_shared<convergent_node_t>(node);
		node->convergentNode = cn;
		cn->requestedQueries = make_shared<requested_query_node_t>();
		ot.stateNodes.emplace_back(cn);
		ot.stateIdentifier.emplace_back(sequence_set_t());
		generateRequestedQueries(ot);
		
		ot.testedState = 0;
		ot.testedInput = 0;

		bool unlearned = true;
		while (unlearned) {
			while ((ot.unconfirmedTransitions > 0) || (!ot.inconsistentNodes.empty())) {
				if (!ot.inconsistentNodes.empty()) {
					processInconsistent(ot, teacher);
				}
				else {

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
					if (ot.numberOfExtraStates == 0) {
						identifyNextState(ot, teacher);
					} else {
						// verify transition
						auto rqIt = cn->requestedQueries->next.begin();
						do {
							if (queryRequiredSequence(ot.testedState, sequence_in_t({ ot.testedInput }), ot, teacher)) {
								break;
							}
							rqIt = cn->requestedQueries->next.find(ot.testedInput);
						} while ((rqIt != cn->requestedQueries->next.end()) && (rqIt->second->seqCount > 0));
						auto reqIt = ot.queriesFromNextState.find(ot.testedState * numInputs + ot.testedInput);
						if (reqIt != ot.queriesFromNextState.end()) {
							auto nextState = ot.conjecture->getNextState(ot.testedState, ot.testedInput);
							while ((ot.numberOfExtraStates > 0) && !reqIt->second.empty()) {
								if (queryRequiredSequence(nextState, reqIt->second.front(), ot, teacher)) {
									break;
								}
							}
							if (ot.numberOfExtraStates > 0)
								ot.queriesFromNextState.erase(reqIt);
						}
					}
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
					*/
					auto ce = teacher->equivalenceQuery(ot.conjecture);
					if (!ce.empty()) {
						ot.numberOfExtraStates--;
						if (ot.conjecture->isOutputState()) {
							for (auto it = ce.begin(); it != ce.end();) {
								if (*it == STOUT_INPUT) {
									it = ce.erase(it);
								}
								else ++it;
							}
						}
						if (ot.numberOfExtraStates == 0) {
							auto currNode = ot.stateNodes[0]->convergent.front();
							for (auto input : ce) {
								moveAndCheckNoES(currNode, input, ot, teacher);
							}
						} else {
							querySequenceAndCheck(ot.stateNodes[0]->convergent.front(), ce, ot, teacher);
						}
						continue;
					}
				}
				unlearned = false;
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
		// clean so convergent_node_t can be destroyed
		for (auto& sn : ot.stateNodes) {
			sn->next.clear();
		}
		return move(ot.conjecture);
	}
}
