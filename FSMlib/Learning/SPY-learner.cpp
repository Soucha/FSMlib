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

#define CHECK_PREDECESSORS 1

#define DUMP_OQ 0

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

	inline bool CNcompare(const convergent_node_t* ls, const convergent_node_t* rs);

	struct CNcomp {
		bool operator() (const convergent_node_t* ls, const convergent_node_t* rs) const {
			return CNcompare(ls, rs);
		}
	};
	/*struct CNcomp {
	bool operator() (const convergent_node_t* ls, const convergent_node_t* rs) const {
	const auto& las = ls->convergent.front()->accessSequence;
	const auto& ras = rs->convergent.front()->accessSequence;
	if (las.size() != ras.size()) return las.size() > ras.size();
	return las < ras;
	}
	//};*/

	typedef set<convergent_node_t*, CNcomp> cn_set_t;

	struct convergent_node_t {
		list<shared_ptr<spy_node_t>> convergent;
		vector<shared_ptr<convergent_node_t>> next;
		cn_set_t domain;// or consistent cn in case of state nodes
		shared_ptr<requested_query_node_t> requestedQueries;

		convergent_node_t(const shared_ptr<spy_node_t>& node) {
			convergent.emplace_back(node);
			next.resize(node->next.size());
		}
	};

	inline bool CNcompare(const convergent_node_t* ls, const convergent_node_t* rs) {
		const auto& las = ls->convergent.front()->accessSequence;
		const auto& ras = rs->convergent.front()->accessSequence;
		if (las.size() != ras.size()) return las.size() < ras.size();
		return las < ras;
	}

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
#if CHECK_PREDECESSORS
		cn_set_t nodesWithChangedDomain;
#endif
#if DUMP_OQ
		unique_ptr<DFSM> observationTree;
#endif
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

	static bool isIntersectionEmpty(const cn_set_t& domain1, const cn_set_t& domain2) {
		if (domain1.empty() || domain2.empty()) return false;
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

	static bool isIntersectionEmpty(const set<state_t>& domain1, const set<state_t>& domain2) {
		if (domain1.empty() || domain2.empty()) return false;
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
		if (node->stateOutput != cn->convergent.front()->stateOutput) return true;
#if CHECK_PREDECESSORS
		auto& cn1 = node->convergentNode.lock();
		if (cn->requestedQueries || cn1->requestedQueries)  {
			if (cn->requestedQueries && cn1->requestedQueries) {
				if (cn1.get() != cn) return true;
			}
			else if (!cn1->domain.count(cn)) return true;
		} else if (isIntersectionEmpty(cn1->domain, cn->domain)) return true;
#endif
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
		if (cn1->requestedQueries || cn2->requestedQueries)  {
			if (cn2->requestedQueries) return !cn1->domain.count(cn2.get());
			if (!cn1->domain.count(cn2.get())) return true;
		}
#if CHECK_PREDECESSORS
		else if (isIntersectionEmpty(cn1->domain, cn2->domain)) return true;
#endif
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
				printf("%d T(%s, %d) = %d query\n", teacher->getOutputQueryCount(),
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
#if DUMP_OQ 
		auto otreeState = ot.observationTree->addState(stateOutput);
		auto otreeStartState = ot.observationTree->getEndPathState(0, node->accessSequence);
		ot.observationTree->setTransition(otreeStartState, input, otreeState, (ot.observationTree->isOutputTransition() ? transitionOutput : DEFAULT_OUTPUT));
#endif // DUMP_OQ
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
			if (node && ((node == otNode) || !areNodesDifferent(node, otNode))) {
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
		sequence_in_t retVal;
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
#if CHECK_PREDECESSORS
			if (retVal.empty() && isIntersectionEmpty(p.first->refStates, p.second->refStates)) {
				retVal = getAccessSequence(n1, p.first);
				retVal.emplace_front(STOUT_INPUT);
			}
#endif
			fifo.pop();
		}
		return retVal;
	}

	static sequence_in_t getQueriedSeparatingSequenceFromSN(const shared_ptr<spy_node_t>& n1, const shared_ptr<convergent_node_t>& cn2) {
		if (n1->stateOutput != cn2->convergent.front()->stateOutput) {
			return sequence_in_t();
		}
		queue<pair<shared_ptr<spy_node_t>, shared_ptr<convergent_node_t>>> fifo;
		sequence_in_t retVal;
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
#if CHECK_PREDECESSORS
			if (retVal.empty() && !p.second->requestedQueries && !p.first->convergentNode.lock()->requestedQueries
				&& isIntersectionEmpty(p.first->convergentNode.lock()->domain, p.second->domain)) {
				retVal = getAccessSequence(n1, p.first);
				retVal.emplace_front(STOUT_INPUT);
			}
#endif
			fifo.pop();
		}
		return retVal;
	}

	static bool isCNdifferent(const shared_ptr<convergent_node_t>& cn1, const shared_ptr<convergent_node_t>& cn2) {
		// cn1 has empty domain
		cn_set_t domain(cn2->domain);
		for (auto& node : cn1->convergent) {
			for (auto it = domain.begin(); it != domain.end();) {
				if (!node->refStates.count((*it)->convergent.front()->state)){
					it = domain.erase(it);
				}
				else {
					++it;
				}
			}
			if (domain.empty()) return true;
		}
		return false;
	}

	static sequence_in_t getQueriedSeparatingSequenceOfCN(const shared_ptr<convergent_node_t>& cn1, const shared_ptr<convergent_node_t>& cn2) {
		if (cn1->convergent.front()->stateOutput != cn2->convergent.front()->stateOutput) {
			return sequence_in_t();
		}
		queue<pair<shared_ptr<convergent_node_t>, shared_ptr<convergent_node_t>>> fifo;
		queue<sequence_in_t> seqFifo;
		sequence_in_t retVal;
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
					if (p.first->next[i] != p.second->next[i]) {
						fifo.emplace(p.first->next[i], p.second->next[i]);
						auto seq(seqFifo.front());
						seq.emplace_back(i);
						seqFifo.emplace(move(seq));
					}
				}
			}
#if CHECK_PREDECESSORS
			if (retVal.empty() && !seqFifo.front().empty() && ((((cn1 != p.first) || (cn2 != p.second)) &&
				((p.first->requestedQueries && !p.second->requestedQueries 
					&& !p.second->domain.empty() && !p.second->domain.count(p.first.get())) ||
				(!p.first->requestedQueries && p.second->requestedQueries 
					&& !p.first->domain.empty() && !p.first->domain.count(p.second.get())))) ||
				(!p.first->requestedQueries && !p.second->requestedQueries 
				&& ((cn1 != p.first) || !p.second->domain.count(cn2.get())) && ((cn1 != p.second) || !p.first->domain.count(cn2.get()))
				&& (isIntersectionEmpty(p.first->domain, p.second->domain) ||
				(p.second->domain.empty() && (((cn1 == p.second) && !p.first->domain.count(cn2.get())) || 
					(isCNdifferent(p.second, p.first)))))))) {
				retVal = move(seqFifo.front());
				retVal.emplace_front(STOUT_INPUT);
			}
#endif
			fifo.pop();
			seqFifo.pop();
		}
		return retVal;
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
			// TODO is it needed?
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
			//TODO change refStates
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
			auto& fromCN = ot.stateNodes[state];
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

	static void checkRQofPrevious(shared_ptr<spy_node_t> node, SPYObservationTree& ot) {
		sequence_in_t seq;
		while (!node->accessSequence.empty()) {
			seq.push_front(node->accessSequence.back());
			node = node->parent.lock();
			if (node->state != NULL_STATE) {
				auto rq = ot.stateNodes[node->state]->requestedQueries;
				for (auto input : seq) {
					auto rqIt = rq->next.find(input);
					if (rqIt == rq->next.end()) {
						rq = nullptr;
						break;
					}
					rq = rqIt->second;
				}
				if (rq) {
					processRelatedTransitions(rq, node->state, seq, ot);
				}
			}
		}
	}

	static bool moveAndCheck(shared_ptr<spy_node_t>& currNode, shared_ptr<requested_query_node_t>& rq,
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
		//processRelatedTransitions(rq, ot.testedState, testedSeq, ot);
		checkRQofPrevious(currNode, ot);
		bool isConsistent = checkPrevious(currNode, ot);
		if (!ot.identifiedNodes.empty()) {// new state
			isConsistent &= makeStateNode(move(ot.identifiedNodes.front()), ot, teacher);
		}
		else {
			isConsistent &= processConfirmedTransitions(ot);
		}
		return isConsistent;
	}

	static bool queryRequiredSequence(shared_ptr<spy_node_t>& currNode, shared_ptr<requested_query_node_t> rq,
			sequence_in_t seq, SPYObservationTree& ot, const unique_ptr<Teacher>& teacher) {
		//auto rq = ot.stateNodes[state]->requestedQueries;
		//auto currNode = ot.stateNodes[state]->convergent.front();// shortest access sequence
		sequence_in_t testedSeq;
		auto lastBranch = rq;
		auto lastBranchInput = seq.empty() ? rq->next.begin()->first : seq.front();
		for (auto nextInput : seq) {
			if (rq->next.size() > 1) {
				lastBranch = rq;
				lastBranchInput = nextInput;
			}
			rq = rq->next.at(nextInput);
			testedSeq.emplace_back(nextInput);
			if (!moveAndCheck(currNode, rq, testedSeq, ot, teacher)) {
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
			if (!moveAndCheck(currNode, rq, testedSeq, ot, teacher)) {
				return true;
			}
		}
		lastBranch->next.erase(lastBranchInput);// no harm even if it is not used (after merging)
		return false;
	}

#if CHECK_PREDECESSORS
	static bool checkPredecessors(shared_ptr<spy_node_t> node, shared_ptr<convergent_node_t> parentCN, SPYObservationTree& ot) {
		bool reduced;
		do {
			reduced = false;
			auto input = node->accessSequence.back();
			for (auto it = parentCN->domain.begin(); it != parentCN->domain.end();) {
				if ((*it)->next[input] && (((*it)->next[input]->requestedQueries &&
					(!(*it)->next[input]->domain.count(parentCN->next[input].get())))
					|| (!(*it)->next[input]->requestedQueries &&
					isIntersectionEmpty(parentCN->next[input]->domain, (*it)->next[input]->domain)))) {

					(*it)->domain.erase(parentCN.get());
					if (parentCN->requestedQueries) {// a state node
						if ((*it)->domain.empty()) {
							storeInconsistentNode((*it)->convergent.front(), ot);
							it = parentCN->domain.erase(it);
							ot.identifiedNodes.clear();
							return false;
						}
						if ((*it)->domain.size() == 1) {
							storeIdentifiedNode((*it)->convergent.front(), ot);
						}
						ot.nodesWithChangedDomain.insert(*it);
					}
					else {
						reduced = true;
					}
					it = parentCN->domain.erase(it);
				}
				else ++it;
			}
			if (reduced) {
				if (parentCN->domain.empty()) {
					storeInconsistentNode(parentCN->convergent.front(), ot);
					ot.identifiedNodes.clear();
					return false;
				}
				if (parentCN->domain.size() == 1) {
					storeIdentifiedNode(parentCN->convergent.front(), ot);
					//break;
				}
				node = parentCN->convergent.front();
				parentCN = node->parent.lock()->convergentNode.lock();
			}
		} while (reduced);
		return true;
	}
#endif

	static bool processChangedNodes(SPYObservationTree& ot) {
		while (!ot.nodesWithChangedDomain.empty()) {
			auto it = ot.nodesWithChangedDomain.end();
			--it;
			auto node = (*it)->convergent.front();
			ot.nodesWithChangedDomain.erase(it);
			if (!checkPredecessors(node, node->parent.lock()->convergentNode.lock(), ot)) {
				ot.nodesWithChangedDomain.clear();
				return false;
			}
		}
		return true;
	}
	
	static bool reduceDomainsBySuccessors(SPYObservationTree& ot) {
		for (const auto& cn : ot.stateNodes) {
			for (auto it = cn->domain.begin(); it != cn->domain.end();) {
				if (areConvergentNodesDistinguished(cn, (*it)->convergent.front()->convergentNode.lock())) {
					(*it)->domain.erase(cn.get());
					if ((*it)->domain.empty()) {
						storeInconsistentNode((*it)->convergent.front(), ot);
						it = cn->domain.erase(it);
						ot.nodesWithChangedDomain.clear();
						return false;
					}
					if ((*it)->domain.size() == 1) {
						storeIdentifiedNode((*it)->convergent.front(), ot);
					}
					ot.nodesWithChangedDomain.insert(*it);
					it = cn->domain.erase(it);
				} else {
					++it;
				}
			}
		}
		return processChangedNodes(ot);
	}

	static bool mergeConvergentNoES(shared_ptr<convergent_node_t>& fromCN, const shared_ptr<convergent_node_t>& toCN, SPYObservationTree& ot) {
		if (toCN->requestedQueries) {// a state node
			if (fromCN->domain.find(toCN.get()) == fromCN->domain.end()) {
				ot.inconsistentSequence = getQueriedSeparatingSequenceOfCN(fromCN, toCN);
				if (ot.inconsistentSequence.empty()) {
					throw "";
				}
				if (ot.inconsistentSequence.front() == STOUT_INPUT) {
					ot.inconsistentSequence.pop_front();
					ot.inconsistentSequence.push_back(STOUT_INPUT);
				}
				storeInconsistentNode(fromCN->convergent.front(), toCN->convergent.front(), ot);
				return false;
			}
			for (auto toIt = toCN->domain.begin(); toIt != toCN->domain.end();) {
				if (areConvergentNodesDistinguished(fromCN, (*toIt)->convergent.front()->convergentNode.lock())) {
					(*toIt)->domain.erase(toCN.get());
					if ((*toIt)->domain.empty()) {
						//auto& n = fromCN->convergent.front();
						//n->convergentNode = fromCN;
						//n->assumedState = WRONG_STATE;
						//toCN->convergent.remove(n);
						storeInconsistentNode((*toIt)->convergent.front(), ot);
						//return false;
					}
					if ((*toIt)->domain.size() == 1) {
						storeIdentifiedNode((*toIt)->convergent.front(), ot);
					}
#if CHECK_PREDECESSORS
					if (ot.inconsistentNodes.empty()) {
						ot.nodesWithChangedDomain.insert(*toIt);
					}
#endif	
					toIt = toCN->domain.erase(toIt);
				}
				else {
					++toIt;
				}
			}
			for (auto& consistent : fromCN->domain) {
				consistent->domain.erase(fromCN.get());
			}
			//fromCN->domain.clear();
			auto& state = toCN->convergent.front()->state;
			for (auto& node : fromCN->convergent) {
				node->assumedState = node->state = state;
				node->convergentNode = toCN;
				//if (node->accessSequence.size() < toCN->convergent.front()->accessSequence.size()) swap ref state nodes
				// TODO swap refSN
				toCN->convergent.emplace_back(node);
			}
		}
		else {
			bool reduced = false;
			// intersection of domains
			auto toIt = toCN->domain.begin();
			auto fromIt = fromCN->domain.begin();
			while ((toIt != toCN->domain.end()) && (fromIt != fromCN->domain.end())) {
				if (CNcompare(*toIt, *fromIt)) {
					(*toIt)->domain.erase(toCN.get());
					toIt = toCN->domain.erase(toIt);
					reduced = true;
				}
				else {
					if (!(CNcompare(*fromIt, *toIt))) {
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
					// TODO does the first CN need to be the one with the shortest access seq?
					//toCN->convergent.emplace_front(node);
					toCN->convergent.emplace_back(node);
				}
				else {
					toCN->convergent.emplace_back(node);
				}
			}
			if (toCN->domain.empty()) {
				auto& n = fromCN->convergent.front();
				//n->convergentNode = fromCN;
				//toCN->convergent.remove(n);
				storeInconsistentNode(n, ot);
				//return false;
			} else
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
							ot.inconsistentSequence = getQueriedSeparatingSequenceOfCN(tmpCN->next[i], toCN->next[i]);
							if (ot.inconsistentSequence.empty()) {
								throw "";
							}
							if (ot.inconsistentSequence.front() == STOUT_INPUT) {
								ot.inconsistentSequence.pop_front();
								ot.inconsistentSequence.push_back(STOUT_INPUT);
							}
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
							while (!(*nIt)->next[i] || ((*nIt)->next[i]->state == NULL_STATE)) ++nIt;
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
#if CHECK_PREDECESSORS
		ot.nodesWithChangedDomain.erase(tmpCN.get());
#endif	
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
					node->assumedState = node->state = NULL_STATE;
					node->convergentNode = parentCN->next[input];
					storeInconsistentNode(node, refCN->convergent.front(), ot);
					ot.identifiedNodes.clear();
					ot.nodesWithChangedDomain.clear();
					return false;
				}
				if (!ot.inconsistentNodes.empty()
#if CHECK_PREDECESSORS
						|| !processChangedNodes(ot)
#endif	
					) {
					ot.nodesWithChangedDomain.clear();
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
				}
#if CHECK_PREDECESSORS
				else {
					bool reduced = false;
					for (auto it = parentCN->domain.begin(); it != parentCN->domain.end();) {
						if ((*it)->next[input] &&
							(((*it)->next[input]->requestedQueries && ((*it)->next[input] != refCN)) ||
							(!(*it)->next[input]->requestedQueries && !(*it)->next[input]->domain.count(refCN.get())))) {

							(*it)->domain.erase(parentCN.get());
							it = parentCN->domain.erase(it);
							reduced = true;
						}
						else ++it;
					}
					if (reduced) {
						if (parentCN->domain.empty()) {
							storeInconsistentNode(parentCN->convergent.front(), ot);
							ot.identifiedNodes.clear();
							return false;
						}
						if (parentCN->domain.size() == 1) {
							storeIdentifiedNode(parentCN->convergent.front(), ot);
						}
						ot.nodesWithChangedDomain.insert(parentCN.get());
						if (!processChangedNodes(ot)) {
							return false;
						}
					}
				}
#endif
			}
			/*
			else {
				node->state = node->assumedState;
			}*/
		}
		return true;
	}

	
	static bool checkNodeNoES(const shared_ptr<spy_node_t>& node, SPYObservationTree& ot) {
		if (node->refStates.empty()) {// new state
			storeIdentifiedNode(node, ot);
			return false;
		}
		else if ((node->assumedState != NULL_STATE) && !node->refStates.count(node->assumedState)) {// inconsistent node
			storeInconsistentNode(node, ot);
			//node->convergentNode.lock()->convergent.remove(node);
			//node->state = NULL_STATE;
			return false;
		}
		else if (node->state == NULL_STATE) {
			if (node->convergentNode.lock()->domain.empty()) {
				storeInconsistentNode(node, ot);//node, 
				return false;
			} else if (node->convergentNode.lock()->domain.size() == 1) {
				storeIdentifiedNode(node, ot);
			}
		}
		return true;
	}

	static void checkAgainstAllNodesNoES(const shared_ptr<spy_node_t>& node, const seq_len_t& suffixLen, 
			bool& isConsistent, SPYObservationTree& ot) {
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
#if CHECK_PREDECESSORS
						if (isConsistent) {
							ot.nodesWithChangedDomain.insert(otNode->convergentNode.lock().get());
							isConsistent &= processChangedNodes(ot);
							//isConsistent &= checkPredecessors(otNode, otNode->parent.lock()->convergentNode.lock(), ot);
						}
#endif
					}
					isConsistent &= checkNodeNoES(otNode, ot);
				}
			}
			for (const auto& nn : otNode->next) {
				if (nn && (nn->maxSuffixLen >= suffixLen)) nodes.emplace(nn);
			}
		}
	}

	static void reduceDomainNoES(const shared_ptr<spy_node_t>& node, const seq_len_t& suffixLen, bool& checkCN, SPYObservationTree& ot) {
		const auto& cn = node->convergentNode.lock();
		for (auto snIt = node->refStates.begin(); snIt != node->refStates.end();) {
			if (areNodesDifferentUnder(node, ot.stateNodes[*snIt]->convergent.front(), suffixLen)) {
				if (cn->domain.erase(ot.stateNodes[*snIt].get())) {
					ot.stateNodes[*snIt]->domain.erase(cn.get());
				}
				snIt = node->refStates.erase(snIt);
			}
			else {
				if (checkCN) {
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
	}

	static void reduceDomainStateNodeNoES(const shared_ptr<spy_node_t>& node, const seq_len_t& suffixLen, bool& isConsistent, SPYObservationTree& ot) {
		const auto& cn = node->convergentNode.lock();
		if (node != ot.stateNodes[node->state]->convergent.front()) {
			for (auto snIt = node->refStates.begin(); snIt != node->refStates.end();) {
				if (areNodesDifferentUnder(node, ot.stateNodes[*snIt]->convergent.front(), suffixLen)) {
					snIt = node->refStates.erase(snIt);
				}
				else {
					++snIt;
				}
			}
			isConsistent &= checkNodeNoES(node, ot);
		}
		else {
			checkAgainstAllNodesNoES(node, suffixLen, isConsistent, ot);
		}
		if (isConsistent) {
			for (auto cnIt = cn->domain.begin(); cnIt != cn->domain.end();) {
				if (areNodeAndConvergentDifferentUnder(node, *cnIt)) {
					(*cnIt)->domain.erase(cn.get());
					isConsistent &= checkNodeNoES((*cnIt)->convergent.front(), ot);
#if CHECK_PREDECESSORS
					if (isConsistent) {
						ot.nodesWithChangedDomain.insert(*cnIt);
					}
#endif
					cnIt = cn->domain.erase(cnIt);
				}
				else ++cnIt;
			}
#if CHECK_PREDECESSORS
			if (isConsistent) {
				isConsistent &= processChangedNodes(ot);
			}
			else {
				ot.nodesWithChangedDomain.clear();
			}
#endif
		}
	}

	static bool checkPreviousNoES(shared_ptr<spy_node_t> node, SPYObservationTree& ot, const unique_ptr<Teacher>& teacher) {
		bool isConsistent = ot.inconsistentNodes.empty() && ot.inconsistentSequence.empty();
		seq_len_t suffixLen = 0;
		input_t input(STOUT_INPUT);
		do {
			node->lastQueriedInput = input;
			if (suffixLen > node->maxSuffixLen) node->maxSuffixLen = suffixLen;
			if (node->state == NULL_STATE) {
				reduceDomainNoES(node, suffixLen, isConsistent, ot);
				isConsistent &= checkNodeNoES(node, ot);
			} else {// state node
				reduceDomainStateNodeNoES(node, suffixLen, isConsistent, ot);
			}
			if (!node->accessSequence.empty()) input = node->accessSequence.back();
			node = node->parent.lock();
			suffixLen++;
		} while (node);

		if (!ot.identifiedNodes.empty()) {
			if (ot.identifiedNodes.front()->refStates.empty() && (ot.testedState != NULL_STATE)) {// new state
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
			}
			currNode = currNext;
			auto& nextCN = cn->next[input];
			if (nextCN->requestedQueries) {// state node
				currNode->state = nextCN->convergent.front()->state;
				if (currNode->assumedState != WRONG_STATE) {
					currNode->assumedState = currNode->state;
				}
				//cn->next[input]->convergent.emplace_front(currNode); 
				// TODO check refSN with the shortest access sequence
				nextCN->convergent.emplace_back(currNode);
			}
			else {
				if (nextCN->convergent.front()->accessSequence.size() > currNode->accessSequence.size()) {
					//nextCN->convergent.emplace_front(currNode); 
					// TODO CN with the shortest access seq
					nextCN->convergent.emplace_back(currNode);
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
				auto it = next.end();
				for (auto& node : cn) {
					if (node->next[i]) {
						if (it == next.end()) {
							it = next.find(node->next[i]->incomingOutput);
							if (it == next.end()) {
								it = next.emplace(node->next[i]->incomingOutput, make_shared<ads_cv_t>(node->next[i])).first;
							}
							else {
								it->second->nodes.emplace_back(list<shared_ptr<spy_node_t>>({ node->next[i] }));
							}
						}
						else {
							it->second->nodes.back().emplace_back(node->next[i]);
						}
					}
				}
				if (it == next.end()) {
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
							auto it = p.second->next.find(cn.front()->stateOutput);
							if (it == p.second->next.end()) {
								it = p.second->next.emplace(cn.front()->stateOutput, make_shared<ads_cv_t>()).first;
							}
							it->second->nodes.emplace_back(list<shared_ptr<spy_node_t>>(cn));
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

	static bool identifyByADS(shared_ptr<spy_node_t>& currNode, shared_ptr<ads_cv_t> ads, 
			SPYObservationTree& ot, const unique_ptr<Teacher>& teacher) {
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
		return false;
	}

	static bool identifyNextState(shared_ptr<spy_node_t>& currNode, SPYObservationTree& ot, const unique_ptr<Teacher>& teacher) {
		//auto currNode = ot.stateNodes[ot.testedState]->convergent.front();// shortest access sequence
		
		if (!moveAndCheckNoES(currNode, ot.testedInput, ot, teacher)) {
			return true;
		}
		if (currNode->state != NULL_STATE) {// identified
			/*/ optimization - apply the same input again
			if (!moveAndCheckNoES(currNode, ot.testedInput, ot, teacher)) {
				return true;
			}
			// TODO return or continue?*/
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
			if (identifyByADS(currNode, ads, ot, teacher)) {
				return true;
			}
		}
		return false;
	}

	static bool querySequenceAndCheck(shared_ptr<spy_node_t> currNode, const sequence_in_t& seq,
		SPYObservationTree& ot, const unique_ptr<Teacher>& teacher, bool allowIdentification = true) {
		bool isConsistent = true;
		if (ot.numberOfExtraStates == 0) {
			ot.inconsistentSequence.push_front(STOUT_INPUT);
			auto numStates = ot.stateNodes.size();
			auto numInconsistent = ot.inconsistentNodes.size();
			for (auto& input : seq) {
				moveAndCheckNoES(currNode, input, ot, teacher);
			}
			if (!ot.inconsistentSequence.empty())
				ot.inconsistentSequence.pop_front();
			isConsistent = (numStates == ot.stateNodes.size()) && (numInconsistent == ot.inconsistentNodes.size());
		}
		else {
			long long suffixAdded = 0;
			for (auto& input : seq) {
				if (!currNode->next[input]) {
					query(currNode, input, ot, teacher);
					if (allowIdentification) {
						currNode->next[input]->assumedState = ot.conjecture->getNextState(currNode->assumedState, input);
						if ((currNode->state != NULL_STATE) && (ot.stateNodes[currNode->state]->next[input])) {
							mergeConvergent(ot.stateNodes[currNode->state]->next[input]->convergent.front()->state, currNode->next[input], ot);
						}
					}
					suffixAdded--;
				}
				currNode = currNode->next[input];
			}
			isConsistent = checkPrevious(move(currNode), ot, suffixAdded);
			if (allowIdentification) {
				if (!ot.identifiedNodes.empty()) {// new state
					isConsistent &= makeStateNode(move(ot.identifiedNodes.front()), ot, teacher);
				}
				else {
					isConsistent &= processConfirmedTransitions(ot);
				}
			}
		}
		return isConsistent;
	}

	static bool moveNewStateNode(shared_ptr<spy_node_t>& node, SPYObservationTree& ot, const unique_ptr<Teacher>& teacher) {
		ot.identifiedNodes.clear();
		auto parent = node->parent.lock();
		auto state = parent->refStates.count(parent->assumedState) ? parent->assumedState : NULL_STATE;
		bool stateNotKnown = (state == NULL_STATE);
		if (ot.numberOfExtraStates == 0) {
			ot.testedState = NULL_STATE;
		}
		//if (stateNotKnown || ) {
			auto& input = node->accessSequence.back();
			do {
				if (stateNotKnown) state = *(parent->refStates.begin());
				auto newSN = ot.stateNodes[state]->convergent.front();
				if (!newSN->next[input]) {
					querySequenceAndCheck(newSN, sequence_in_t({ input }), ot, teacher, false);
				}
				newSN = newSN->next[input];
				while (!newSN->refStates.empty() && parent->refStates.count(state)) {
					auto newSNstate = *(newSN->refStates.begin());
					sequence_in_t sepSeq;
					if ((node->assumedState == NULL_STATE) || (node->assumedState == newSNstate) ||
						!isSeparatingSequenceQueried(node, newSNstate, sepSeq, ot)) {
						sepSeq = getQueriedSeparatingSequence(node, ot.stateNodes[newSNstate]->convergent.front());
					}
					querySequenceAndCheck(newSN, sepSeq, ot, teacher, false);
				}
				if (newSN->refStates.empty()) {
					node = newSN;
					break;
				}
				else if (parent->refStates.empty()) {
					node = parent;
					break;
				}
				else if (!stateNotKnown) {// parent is inconsistent
					if (ot.numberOfExtraStates == 0) {
						ot.testedState = 0;
					}
					storeInconsistentNode(parent, ot);
					return false;
				}
			} while (!parent->refStates.empty());
			return true;
		//}
		// parent is inconsistent
		//return false;
	}

	static bool makeStateNode(shared_ptr<spy_node_t> node, SPYObservationTree& ot, const unique_ptr<Teacher>& teacher) {
		auto parent = node->parent.lock();
		if ((parent->state == NULL_STATE) || (ot.stateNodes[parent->state]->convergent.front() != parent)) {
			if (parent->refStates.empty()) {
				return makeStateNode(parent, ot, teacher);
			}
			if (!moveNewStateNode(node, ot, teacher)) {
				return false;// inconsistency
			}
			if (node == parent) {
				return makeStateNode(parent, ot, teacher);
			}
		}
		if (node->assumedState == WRONG_STATE) node->assumedState = NULL_STATE;
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
		auto cn = node->convergentNode.lock();
		node->convergentNode = ot.stateNodes.back();
		if (cn) {
			cn->convergent.remove(node);
			if (cn->convergent.size() == 0) {
				ot.stateNodes.back()->next = move(cn->next);
				auto& parentCN = node->parent.lock()->convergentNode.lock();
				parentCN->next[node->accessSequence.back()] = ot.stateNodes.back();
			}
		}
		
		// update OTree with new state and fill ot.identifiedNodes with nodes with |domain|<= 1
		ot.identifiedNodes.clear();
		updateOTreeWithNewState(node, ot);

		if (!ot.identifiedNodes.empty() && (ot.identifiedNodes.front()->refStates.empty())) {// new state
			return makeStateNode(move(ot.identifiedNodes.front()), ot, teacher);
		}
		ot.numberOfExtraStates = 0;
		ot.testedState = 0;
		generateRequestedQueries(ot);

		ot.inconsistentSequence.clear();
		ot.inconsistentNodes.clear();
#if CHECK_PREDECESSORS
		if (!reduceDomainsBySuccessors(ot)) {
			return false;
		}
#endif
		return processIdentified(ot);
	}

	static shared_ptr<spy_node_t> queryIfNotQueried(shared_ptr<spy_node_t> node, sequence_in_t seq, 
			SPYObservationTree& ot, const unique_ptr<Teacher>& teacher) {
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
	
	static bool proveSepSeqOfEmptyDomainIntersection(shared_ptr<spy_node_t> n1, list<shared_ptr<spy_node_t>>& nodes2,
			sequence_in_t& sepSeq, SPYObservationTree& ot, const unique_ptr<Teacher>& teacher) {
		n1 = queryIfNotQueried(n1, sepSeq, ot, teacher);
		if (!n1) {
			return true;
		}
		bool found = false;
		auto d1 = n1->refStates;
		for (auto& s : d1) {
			if (n1->refStates.count(s)) {
				for (const auto& n2 : nodes2) {
					if (!n2->refStates.count(s)) {
						auto seq = getQueriedSeparatingSequence(n2, ot.stateNodes[s]->convergent.front());
						if (!queryIfNotQueried(n1, seq, ot, teacher)) {
							return true;
						}
						if (n1->refStates.count(s) || areNodesDifferentUnder(n1, n2, seq.size())) {
							sepSeq.splice(sepSeq.end(), move(seq));
							found = true;
						}
						break;
					}
				}
				if (found) break;
			}
		}
		if (!found) {
			if (ot.numberOfExtraStates == 0) {
				auto seq = getQueriedSeparatingSequenceOfCN(nodes2.front()->convergentNode.lock(), n1->convergentNode.lock());
				if (!seq.empty()) {
					if (seq.front() == STOUT_INPUT) {
						seq.pop_front();// STOUT
						auto cn2 = nodes2.front()->convergentNode.lock();
						for (auto& input : seq) {
							cn2 = cn2->next[input];
						}
						if (proveSepSeqOfEmptyDomainIntersection(n1, cn2->convergent, seq, ot, teacher)) {
							return true;
						}
					}
					else {
						if (!queryIfNotQueried(n1, seq, ot, teacher)) {
							return true;
						}
						if (!queryIfNotQueried(nodes2.front(), seq, ot, teacher)) {
							return true;
						}
					}
					if (!queryIfNotQueried(n1, seq, ot, teacher)) {
						return true;
					}
					if (areNodeAndConvergentDifferentUnder(n1, nodes2.front()->convergentNode.lock().get())) {
						sepSeq.splice(sepSeq.end(), move(seq));
						found = true;
					}
				}
			}
			if (!found) {
				d1 = n1->refStates;
				for (auto& s : d1) {
					if (n1->refStates.count(s)) {
						if (ot.numberOfExtraStates == 0) {
							auto seq = getQueriedSeparatingSequenceOfCN(nodes2.front()->convergentNode.lock(), ot.stateNodes[s]);
							if (!seq.empty()) {
								if (seq.front() == STOUT_INPUT) {
									seq.pop_front();// STOUT
									auto cn2 = nodes2.front()->convergentNode.lock();
									for (auto& input : seq) {
										cn2 = cn2->next[input];
									}
									if (proveSepSeqOfEmptyDomainIntersection(ot.stateNodes[s]->convergent.front(),
										cn2->convergent, seq, ot, teacher)) {
										return true;
									}
								}
								else {
									if (!queryIfNotQueried(ot.stateNodes[s]->convergent.front(), seq, ot, teacher)) {
										return true;
									}
									if (!queryIfNotQueried(nodes2.front(), seq, ot, teacher)) {
										return true;
									}
								}
								if (!queryIfNotQueried(n1, seq, ot, teacher)) {
									return true;
								}
								if (n1->refStates.count(s) || areNodeAndConvergentDifferentUnder(n1, nodes2.front()->convergentNode.lock().get())) {
									sepSeq.splice(sepSeq.end(), move(seq));
									found = true;
								}
							}
							else {
								auto seq = getQueriedSeparatingSequenceOfCN(n1->convergentNode.lock(), ot.stateNodes[s]);
								if (seq.empty()) {
									continue;
								}
								if (seq.front() == STOUT_INPUT) {
									seq.pop_front();// STOUT
									auto cn1 = n1->convergentNode.lock();
									for (auto& input : seq) {
										cn1 = cn1->next[input];
									}
									if (proveSepSeqOfEmptyDomainIntersection(ot.stateNodes[s]->convergent.front(),
										cn1->convergent, seq, ot, teacher)) {
										return true;
									}
								}
								else {
									if (!queryIfNotQueried(ot.stateNodes[s]->convergent.front(), seq, ot, teacher)) {
										return true;
									}
									if (!queryIfNotQueried(n1, seq, ot, teacher)) {
										return true;
									}
								}
								if (!queryIfNotQueried(nodes2.front(), seq, ot, teacher)) {
									return true;
								}
								if (nodes2.front()->refStates.count(s) || areNodesDifferentUnder(nodes2.front(), n1, seq.size())) {
									sepSeq.splice(sepSeq.end(), move(seq));
									found = true;
								}
							}
						}
						else {
							for (const auto& n2 : nodes2) {
								auto seq = getQueriedSeparatingSequence(n2, ot.stateNodes[s]->convergent.front());
								if (seq.empty()) continue;
								if (seq.front() != STOUT_INPUT) {
									throw;
								}
								seq.pop_front();// STOUT
								auto next2 = queryIfNotQueried(n2, seq, ot, teacher);
								if (proveSepSeqOfEmptyDomainIntersection(ot.stateNodes[s]->convergent.front(),
									list<shared_ptr<spy_node_t>>({ next2 }), seq, ot, teacher)) {
									return true;
								}
								if (!queryIfNotQueried(n1, seq, ot, teacher)) {
									return true;
								}
								if (n1->refStates.count(s) || areNodesDifferentUnder(n1, n2, seq.size())) {
									sepSeq.splice(sepSeq.end(), move(seq));
									found = true;
								}
								break;
							}
							if (!found) {
								auto seq = getQueriedSeparatingSequence(n1, ot.stateNodes[s]->convergent.front());
								if (seq.empty()) {
									continue;
								}
								if (seq.front() != STOUT_INPUT) {
									throw;
								}
								seq.pop_front();// STOUT
								auto next1 = queryIfNotQueried(n1, seq, ot, teacher);
								if (proveSepSeqOfEmptyDomainIntersection(ot.stateNodes[s]->convergent.front(),
									list<shared_ptr<spy_node_t>>({ next1 }), seq, ot, teacher)) {
									return true;
								}
								if (!queryIfNotQueried(nodes2.front(), seq, ot, teacher)) {
									return true;
								}
								if (nodes2.front()->refStates.count(s) || areNodesDifferentUnder(nodes2.front(), n1, seq.size())) {
									sepSeq.splice(sepSeq.end(), move(seq));
									found = true;
								}
							}
						}
						if (found) break;
					}
				}
				if (!found) {
					return false;
				}
			}
		}
		return false;
	}

	static bool proveSepSeqOfEmptyDomainIntersection(const shared_ptr<spy_node_t>& n1, shared_ptr<convergent_node_t> cn2,
		sequence_in_t& sepSeq, SPYObservationTree& ot, const unique_ptr<Teacher>& teacher) {
		for (auto& input : sepSeq) {
			cn2 = cn2->next[input];
		}
		return proveSepSeqOfEmptyDomainIntersection(n1, cn2->convergent, sepSeq, ot, teacher);
	}

	static bool eliminateSeparatedStatesFromDomain(const shared_ptr<spy_node_t>& node, const shared_ptr<convergent_node_t>& cn,
			SPYObservationTree& ot, const unique_ptr<Teacher>& teacher) {
		auto domain = node->refStates;
		for (auto state : domain) {
			if (node->refStates.count(state)) {
				auto sepSeq = getQueriedSeparatingSequenceOfCN(cn, ot.stateNodes[state]);
				if (sepSeq.empty()) {
					return false;
				}
				if (sepSeq.front() == STOUT_INPUT) {
					sepSeq.pop_front();
					if (proveSepSeqOfEmptyDomainIntersection(ot.stateNodes[state]->convergent.front(), cn, sepSeq, ot, teacher)) {
						return true;
					}
				}
				if (!queryIfNotQueried(node, sepSeq, ot, teacher)) {
					return true;
				}
				if (!queryIfNotQueried(ot.stateNodes[state]->convergent.front(), move(sepSeq), ot, teacher)) {
					return true;
				}
			}
		}
		return false;
	}

	static sequence_in_t getQueriedSeparatingSequenceForSuccessor(state_t diffState, state_t parentState,
		const sequence_in_t& transferSeq, SPYObservationTree& ot, const unique_ptr<Teacher>& teacher) {
		sequence_in_t sepSeq;
#if CHECK_PREDECESSORS
		sequence_in_t seqToDistDomains;
		shared_ptr<spy_node_t> distDomN1, distDomN2;
#endif
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
#if CHECK_PREDECESSORS
						if (sepSeq.front() == STOUT_INPUT) {
							if (seqToDistDomains.empty() || (seqToDistDomains.size() > sepSeq.size())) {
								seqToDistDomains = move(sepSeq);
								distDomN1 = node;
								distDomN2 = n;
							}
						}
						else
#endif
							return sepSeq;
					}
				}
				diffCN.emplace_back(node);
			}
			if (node->assumedState == parentState) {
				auto n = node;
				for (auto& input : transferSeq) {
					n = n->next[input];
					if (!n) break;
				}
				if (n) {
					for (const auto& diffN : diffCN) {
						sepSeq = getQueriedSeparatingSequence(n, diffN);
						if (!sepSeq.empty()) {
#if CHECK_PREDECESSORS
							if (sepSeq.front() == STOUT_INPUT) {
								if (seqToDistDomains.empty() || (seqToDistDomains.size() > sepSeq.size())) {
									seqToDistDomains = move(sepSeq);
									distDomN1 = n;
									distDomN2 = diffN;
								}
							}
							else
#endif
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
#if CHECK_PREDECESSORS
		if (!seqToDistDomains.empty()) {
			sepSeq = move(seqToDistDomains);
			sepSeq.pop_front();
			distDomN2 = queryIfNotQueried(distDomN2, sepSeq, ot, teacher);
			if (proveSepSeqOfEmptyDomainIntersection(distDomN1, list<shared_ptr<spy_node_t>>({distDomN2}), sepSeq, ot, teacher)) {
				return seqToDistDomains;
			}
		}
#endif
		return sepSeq;
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
			if (!n2->refStates.count(n2->assumedState)) return;
			if ((n2->state != NULL_STATE) && (n2 == ot.stateNodes[n2->state]->convergent.front())) {
				auto cn2 = n2->convergentNode.lock();
				auto transferSeq = move(sepSeq);
				auto domain = n1->refStates;
				for (auto state : domain) {
					if (!n1->refStates.count(state)) continue;
					sepSeq.clear();
					sequence_in_t seqToDistDomains;
					if (ot.numberOfExtraStates == 0) {
						for (auto n : cn2->convergent) {
							for (auto& input : transferSeq) {
								n = n->next[input];
								if (!n) break;
							}
							if (n) {
								sepSeq = getQueriedSeparatingSequenceFromSN(n, ot.stateNodes[state]);
								if (!sepSeq.empty()) {
#if CHECK_PREDECESSORS
									if (sepSeq.front() == STOUT_INPUT) {
										if (seqToDistDomains.empty() || (seqToDistDomains.size() > sepSeq.size())) {
											seqToDistDomains = move(sepSeq);
										}
										else {
											sepSeq.clear();
										}
									} else
#endif
									break;
								}
							}
						}
#if CHECK_PREDECESSORS
						if (sepSeq.empty() && !seqToDistDomains.empty()) {
							sepSeq = move(seqToDistDomains);
							sepSeq.pop_front();
							if (proveSepSeqOfEmptyDomainIntersection(ot.stateNodes[state]->convergent.front(),
									n1->convergentNode.lock(), sepSeq, ot, teacher)) {
								return;
							}
						}
#endif
						if (sepSeq.empty()) {
							return;
						}
						if (!queryIfNotQueried(ot.stateNodes[state]->convergent.front(), sepSeq, ot, teacher)) {
							return;
						}
					} else {
						shared_ptr<spy_node_t> distN2;
						for (auto n : cn2->convergent) {
							for (auto& input : transferSeq) {
								n = n->next[input];
								if (!n) break;
							}
							if (n) {
								sepSeq = getQueriedSeparatingSequence(n, ot.stateNodes[state]->convergent.front());
								if (!sepSeq.empty()) {
#if CHECK_PREDECESSORS
									if (sepSeq.front() == STOUT_INPUT) {
										if (seqToDistDomains.empty() || (seqToDistDomains.size() > sepSeq.size())) {
											seqToDistDomains = move(sepSeq);
											distN2 = n;
										}
										else {
											sepSeq.clear();
										}
									} else
#endif
									break;
								}
							}
						}
#if CHECK_PREDECESSORS
						if (sepSeq.empty() && !seqToDistDomains.empty()) {
							sepSeq = move(seqToDistDomains);
							sepSeq.pop_front();
							distN2 = queryIfNotQueried(distN2, sepSeq, ot, teacher);
							if (proveSepSeqOfEmptyDomainIntersection(ot.stateNodes[state]->convergent.front(),
									list<shared_ptr<spy_node_t>>({distN2}), sepSeq, ot, teacher)) {
								return;
							}
						}
#endif
						if (sepSeq.empty()) {
							sepSeq = getQueriedSeparatingSequenceForSuccessor(state, n2->state, transferSeq, ot, teacher);
							if (sepSeq.empty()) {// already resolved
								return;
							}
							if (!queryIfNotQueried(ot.stateNodes[state]->convergent.front(), sepSeq, ot, teacher)) {
								return;
							}
						}
					}
					if (!queryIfNotQueried(n1, move(sepSeq), ot, teacher)) {
						return;
					}
					return;
				} 
			} else {
				if (n1->assumedState != WRONG_STATE) {
					sequence_in_t sepSuffix;
					if ((ot.numberOfExtraStates == 0) && isPrefix(n1->accessSequence, ot.bbNode->accessSequence)) {
						sepSuffix = getQueriedSeparatingSequenceFromSN(n1, ot.stateNodes[n1->assumedState]);
					} else {
						sepSuffix = getQueriedSeparatingSequence(n1, ot.stateNodes[n1->assumedState]->convergent.front());
					}
					if (sepSuffix.empty() || (sepSuffix.front() == STOUT_INPUT)) {
						return;
					}
					sepSeq.splice(sepSeq.end(), move(sepSuffix));
				}
				if (!querySequenceAndCheck(ot.stateNodes[n2->assumedState]->convergent.front(), sepSeq, ot, teacher)) {
					return;
				}
				/*
				do {
					sepSeq.push_front(n2->accessSequence.back());
					n2 = n2->parent.lock();
				} while (n2->assumedState == NULL_STATE);
				if (!querySequenceAndCheck(ot.stateNodes[n2->assumedState]->convergent.front(), sepSeq, ot, teacher)) {
					return;
				}*/
				return;
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
				if (!n2->refStates.count(n2->assumedState)) return;
				if (n2 != ot.stateNodes[n2->state]->convergent.front()) {
					n2 = ot.stateNodes[n2->state]->convergent.front();
					n1 = queryIfNotQueried(n2, move(sepSeq), ot, teacher);
					if (!n1) {
						return;
					}
				}
				if (eliminateSeparatedStatesFromDomain(n1, n1->convergentNode.lock(), ot, teacher)) {
					return;
				}
				return;
			}
			else {
				auto n2 = move(ot.inconsistentNodes.front());
				ot.inconsistentNodes.pop_front();
				auto fn1 = move(ot.inconsistentNodes.front());
				ot.inconsistentNodes.pop_front();
				auto fn2 = move(ot.inconsistentNodes.front());
				ot.inconsistentNodes.pop_front();
				if (ot.inconsistentSequence.back() == STOUT_INPUT) {
					ot.inconsistentSequence.pop_back();
					if (proveSepSeqOfEmptyDomainIntersection(n2, n1->convergentNode.lock(), ot.inconsistentSequence, ot, teacher)) {
						return;
					}
				}
				n2 = queryIfNotQueried(n2, ot.inconsistentSequence, ot, teacher);
				if (!n2) {
					return;
				}/*
				if (fn1 == fn2) {// empty domain
					if (eliminateSeparatedStatesFromDomain(n2, fn1->convergentNode.lock(), ot, teacher)) {
						return;
					}
					return;
				}
				else {
					auto sepSeq = getQueriedSeparatingSequenceOfCN(fn1->convergentNode.lock(), fn2->convergentNode.lock());
					if (sepSeq.empty()) {
						return;
					}
#if CHECK_PREDECESSORS
					if (sepSeq.front() == STOUT_INPUT) {
						sepSeq.pop_front();
						if (proveSepSeqOfEmptyDomainIntersection(n2, fn1->convergentNode.lock(), sepSeq, ot, teacher)) {
							return;
						}
					}
#endif
					if (!queryIfNotQueried(n2, sepSeq, ot, teacher)) {
						return;
					}
					ot.inconsistentSequence.splice(ot.inconsistentSequence.end(), move(sepSeq));*/
					if (!queryIfNotQueried(n1, move(ot.inconsistentSequence), ot, teacher)) {
						return;
					}
					return;
				//}			
			}
		}
	}
	
	static shared_ptr<ads_cv_t> getADSwithFixedPrefix(shared_ptr<spy_node_t> node, SPYObservationTree& ot) {
		auto ads = make_shared<ads_cv_t>();
		auto& cn = node->convergentNode.lock();
		for (auto& sn : cn->domain) {
			ads->nodes.push_back(sn->convergent);
		}
		auto currAds = ads;
		while (node->lastQueriedInput != STOUT_INPUT) {
			currAds->input = node->lastQueriedInput;
			node = node->next[node->lastQueriedInput];
			auto nextADS = make_shared<ads_cv_t>();
			for (auto& currCN : currAds->nodes) {
				list<shared_ptr<spy_node_t>> nextNodes;
				for (auto& nn : currCN) {
					if (nn->next[currAds->input]) {
						nextNodes.emplace_back(nn->next[currAds->input]);
					}
				}
				if (!nextNodes.empty()) {
					nextADS->nodes.emplace_back(move(nextNodes));
				}
			}
			if (nextADS->nodes.size() <= 1) return nullptr;
			currAds->next.emplace(node->incomingOutput, nextADS);
			currAds = nextADS;
		}
		if (!areDistinguished(currAds->nodes)) {
			return nullptr;
		}
		seq_len_t totalLen = 0;
		frac_t bestVal(1), currVal(0);
		chooseADS(currAds, ot, bestVal, currVal, totalLen);
		return ads;
	}

	static seq_len_t chooseTransitionToVerify(SPYObservationTree& ot) {
		seq_len_t minLen = seq_len_t(-1);
		const auto numInputs = ot.conjecture->getNumberOfInputs();
		auto reqIt = ot.queriesFromNextState.begin();
		size_t tranId = 0;
		for (state_t state = 0; state < ot.stateNodes.size(); state++) {
			const auto& sn = ot.stateNodes[state];
			const auto& refSN = sn->convergent.front();
			for (input_t input = 0; input < numInputs; input++, tranId++) {
				seq_len_t len = minLen;
				auto inIt = sn->requestedQueries->next.find(input);
				bool isRq = ((inIt != sn->requestedQueries->next.end()) && (inIt->second->seqCount > 0));
				if ((reqIt != ot.queriesFromNextState.end()) && (tranId == reqIt->first)) {
					if (reqIt->second.empty()) {
						reqIt = ot.queriesFromNextState.erase(reqIt);
						if (isRq) {
							len = inIt->second->seqCount * refSN->accessSequence.size();
						}
					}
					else {
						auto nextState = ot.conjecture->getNextState(refSN->state, input);
						len = reqIt->second.size() * ot.stateNodes[nextState]->convergent.front()->accessSequence.size();
						if (isRq) {
							len =+ inIt->second->seqCount * refSN->accessSequence.size();
						}
						++reqIt;
					}
				}
				else if (isRq) {
					len = inIt->second->seqCount * refSN->accessSequence.size();
				}
				if (minLen > len) {
					minLen = len;
					ot.testedState = state;
					ot.testedInput = input;
				}
			}
		}
		return minLen;
	}

	static bool tryExtendQueriedPath(shared_ptr<spy_node_t>& node, SPYObservationTree& ot, const unique_ptr<Teacher>& teacher) {
		node = ot.bbNode;// should be the same
		sequence_in_t seq;
		while (node->state == NULL_STATE) {// find the lowest state node
			seq.emplace_front(node->accessSequence.back());
			node = node->parent.lock();
		}
		ot.testedInput = STOUT_INPUT;
		auto cn = ot.stateNodes[node->state];
		if (seq.size() > 0) {
			if (ot.numberOfExtraStates == 0) {
				auto inIt = cn->requestedQueries->next.find(seq.front());
				if (inIt != cn->requestedQueries->next.end()) {
					auto ads = getADSwithFixedPrefix(node->next[seq.front()], ot);
					if (ads) {
						ot.testedState = node->state;
						ot.testedInput = seq.front();
						node = node->next[seq.front()];
						identifyByADS(node, ads, ot, teacher);
						return true;// next time tryExtend is called again
					}
				}
				// find unidentified transition closest to the root
				seq_len_t minLen(ot.stateNodes.size());
				for (const auto& sn : ot.stateNodes) {
					if ((sn->requestedQueries->seqCount > 0) && (minLen > sn->convergent.front()->accessSequence.size())) {
						node = sn->convergent.front();
						minLen = node->accessSequence.size();
						ot.testedState = node->state;
						cn = sn;
					}
				}
			}
			else {
				do {
					cn = ot.stateNodes[node->state];
					auto rq = cn->requestedQueries;
					for (auto input : seq) {
						auto rqIt = rq->next.find(input);
						if (rqIt == rq->next.end()) {
							rq = nullptr;
							break;
						}
						rq = rqIt->second;
					}
					if (rq) {
						ot.testedState = node->state;
						ot.testedInput = seq.front();
						queryRequiredSequence(node, cn->requestedQueries, move(seq), ot, teacher);
						return true;// next time tryExtend is called again
					}
					if (!node->accessSequence.empty())
						seq.emplace_front(node->accessSequence.back());
					node = node->parent.lock();
				} while (node);
					
				chooseTransitionToVerify(ot);
				node = ot.stateNodes[ot.testedState]->convergent.front();
			}
		}
		else if (!cn->requestedQueries->next.empty()) {//the state node is leaf with required outcoming sequences
			ot.testedState = node->state;
		}
		else {//the state node is leaf = if (node->lastQueriedInput == STOUT_INPUT)
			// find unidentified transition closest to the root
			seq_len_t minLen(ot.stateNodes.size());
			if (ot.numberOfExtraStates == 0) {
				for (const auto& sn : ot.stateNodes) {
					if ((sn->requestedQueries->seqCount > 0) && (minLen > sn->convergent.front()->accessSequence.size())) {
						minLen = sn->convergent.front()->accessSequence.size();
						ot.testedState = sn->convergent.front()->state;
					}
				}
			}
			else {
				minLen = chooseTransitionToVerify(ot);
			}
			// extend or reset?
			bool extend = false;
			if (minLen > 1) {
				// find closest unidentified transition from the node
				queue<pair<sequence_in_t, shared_ptr<convergent_node_t>>> fifo;
				seq.clear();// should be empty
				fifo.emplace(seq, move(cn));
				while (!fifo.empty()) {
					auto p = move(fifo.front());
					fifo.pop();
					for (input_t i = 0; i < p.second->next.size(); i++) {
						if (p.second->next[i] && p.second->next[i]->requestedQueries) {
							seq = p.first;
							seq.emplace_back(i);
							if (!p.second->next[i]->requestedQueries->next.empty()) {
								if (!querySequenceAndCheck(node, seq, ot, teacher)) {
									// inconsistency found
									return true;
								}
								node = ot.bbNode;
								cn = node->convergentNode.lock();
								ot.testedState = node->state;
								ot.testedInput = STOUT_INPUT;
								extend = true;
								break;
							}
							if (seq.size() + 1 < min(minLen, p.second->next[i]->convergent.front()->accessSequence.size())) {
								fifo.emplace(move(seq), p.second->next[i]);
							}
						}
					}
					if (extend) {
						break;
					}
				}
			}
			if (!extend) {
				cn = ot.stateNodes[ot.testedState];
				node = cn->convergent.front();
			}
		}
		if (ot.testedInput == STOUT_INPUT) {
			if (ot.numberOfExtraStates == 0) {
				if (!node->accessSequence.empty() && cn->requestedQueries->next.count(node->accessSequence.back())) {
					ot.testedInput = node->accessSequence.back();
				}
				else {
					ot.testedInput = cn->requestedQueries->next.begin()->first;
				}
			}
			else {
				for (const auto& inIt : cn->requestedQueries->next) {
					if (inIt.second->seqCount > 0) {
						ot.testedInput = inIt.first;
						break;
					}
				}
			}
		}
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
		node->state = 0;
		if (ot.conjecture->isOutputState()) {
			node->stateOutput = teacher->outputQuery(STOUT_INPUT);
			checkNumberOfOutputs(teacher, ot.conjecture);
			ot.conjecture->setOutput(0, node->stateOutput);
		}
#if DUMP_OQ
		ot.observationTree = FSMmodel::createFSM(teacher->getBlackBoxModelType(), 1, numInputs, teacher->getNumberOfOutputs());
		if (ot.observationTree->isOutputState()) {
			ot.observationTree->setOutput(0, node->stateOutput);
		}
#endif
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
		bool checkPossibleExtension = false;
		bool unlearned = true;
		while (unlearned) {
			while ((ot.unconfirmedTransitions > 0) || (!ot.inconsistentNodes.empty())) {
				if (!ot.inconsistentNodes.empty()) {
					auto numStates = ot.stateNodes.size();
					processInconsistent(ot, teacher);
					if (ot.inconsistentNodes.empty() && (numStates == ot.stateNodes.size())) {
						ot.identifiedNodes.clear();
						updateOTreeWithNewState(nullptr, ot);

						ot.numberOfExtraStates = 0;
						if (!ot.identifiedNodes.empty() && (ot.identifiedNodes.front()->refStates.empty())) {// new state
							makeStateNode(move(ot.identifiedNodes.front()), ot, teacher);
						}
						else {
							ot.testedState = 0;
							generateRequestedQueries(ot);

							ot.inconsistentSequence.clear();
							ot.inconsistentNodes.clear();
#if CHECK_PREDECESSORS
							if (!reduceDomainsBySuccessors(ot)) {
								continue;
							}
#endif
							processIdentified(ot);
							if (ot.inconsistentNodes.empty() && (numStates == ot.stateNodes.size())) {
								throw;
							}
						}
					}
				}
				else {
					if (checkPossibleExtension) {
						if (tryExtendQueriedPath(node, ot, teacher)) {
							// inconsistency
							continue;
						}
					}
					else {
						node = ot.stateNodes[ot.testedState]->convergent.front();
					}
					if (ot.numberOfExtraStates == 0) {
						checkPossibleExtension = (identifyNextState(node, ot, teacher)
							|| (ot.stateNodes[ot.testedState]->next[ot.testedInput]->requestedQueries));
					} else {
						cn = ot.stateNodes[ot.testedState];
						if (ot.testedInput == STOUT_INPUT) {
							queryRequiredSequence(node, cn->requestedQueries, sequence_in_t(), ot, teacher);
						}
						else {
							auto rqIt = cn->requestedQueries->next.find(ot.testedInput);
							if ((rqIt != cn->requestedQueries->next.end()) && (rqIt->second->seqCount > 0)) {
								queryRequiredSequence(node, cn->requestedQueries, sequence_in_t({ ot.testedInput }), ot, teacher);
								/*
								checkPossibleExtension = (queryRequiredSequence(node, cn->requestedQueries,
								sequence_in_t({ ot.testedInput }), ot, teacher) ||
								((reqIt == ot.queriesFromNextState.end()) &&
								(!cn->requestedQueries->next.count(ot.testedInput) || (rqIt->second->seqCount == 0))));*/
							}
							else {
								auto reqIt = ot.queriesFromNextState.find(ot.testedState * numInputs + ot.testedInput);
								if ((reqIt != ot.queriesFromNextState.end()) && (reqIt->second.empty())) {
									ot.queriesFromNextState.erase(reqIt);
									reqIt = ot.queriesFromNextState.end();
								}
								ot.testedState = ot.conjecture->getNextState(ot.testedState, ot.testedInput);
								node = ot.stateNodes[ot.testedState]->convergent.front();
								queryRequiredSequence(node, ot.stateNodes[ot.testedState]->requestedQueries,
									reqIt->second.front(), ot, teacher);
							}
							/*
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
							if (queryRequiredSequence(node, ot.stateNodes[ot.testedState]->requestedQueries,
							sequence_in_t({ ot.testedInput }), ot, teacher)) {
							break;
							}
							rqIt = cn->requestedQueries->next.find(ot.testedInput);
							} while ((rqIt != cn->requestedQueries->next.end()) && (rqIt->second->seqCount > 0));
							if (ot.inconsistentNodes.empty()) {
							auto reqIt = ot.queriesFromNextState.find(ot.testedState * numInputs + ot.testedInput);
							if (reqIt != ot.queriesFromNextState.end()) {
							//auto nextState
							ot.testedState = ot.conjecture->getNextState(ot.testedState, ot.testedInput);
							while ((ot.numberOfExtraStates > 0) && !reqIt->second.empty()) {
							if (queryRequiredSequence(node, ot.stateNodes[ot.testedState]->requestedQueries,
							reqIt->second.front(), ot, teacher)) {
							break;
							}
							}
							if ((ot.numberOfExtraStates > 0) && (ot.inconsistentNodes.empty()))
							ot.queriesFromNextState.erase(reqIt);
							}
							}*/
						}
					}
				}
				if (provideTentativeModel) {
#if DUMP_OQ
					unlearned = provideTentativeModel(ot.observationTree);
#else
					unlearned = provideTentativeModel(ot.conjecture);
#endif // DUMP_OQ
					if (!unlearned) break;
				}
			}
			if (!unlearned) break;

			checkPossibleExtension = true;

			ot.numberOfExtraStates++;
			if (ot.numberOfExtraStates > maxExtraStates) {
				if (isEQallowed) {
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
						querySequenceAndCheck(ot.stateNodes[0]->convergent.front(), ce, ot, teacher);
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
