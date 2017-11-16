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

using namespace FSMtesting;

namespace FSMlearning {

#define CHECK_PREDECESSORS 0

#define DUMP_OQ 0

	struct requested_query_node_t {
		size_t seqCount = 0;
		list<size_t> relatedTransition;
		map<input_t, shared_ptr<requested_query_node_t>> next;
	};

	// custom hash can be a standalone function object:
	struct MyHash {
		size_t operator()(pair<ConvergentNode*, ConvergentNode*> const& s) const {
			size_t h1 = std::hash<ConvergentNode*>{}(s.first);
			size_t h2 = std::hash<ConvergentNode*>{}(s.second);
			return h1 ^ (h2 << 1); // or use boost::hash_combine (see Discussion)
		}
	};

	typedef unordered_set<pair<ConvergentNode*, ConvergentNode*>, MyHash> cn_pair_set_t;

	typedef double frac_t;

	struct ads_cv_t {
		list<list<shared_ptr<OTreeNode>>> nodes;
		input_t input;
		map<output_t, shared_ptr<ads_cv_t>> next;

		ads_cv_t() : input(STOUT_INPUT) {}
		ads_cv_t(const shared_ptr<OTreeNode>& node) : nodes({ list<shared_ptr<OTreeNode>>({ node }) }), input(STOUT_INPUT) {}
	};

	struct SPYLearningInfo {
		OTree ot;
		vector<sequence_set_t> stateIdentifier;
		vector<sequence_in_t> separatingSequences;
		shared_ptr<OTreeNode> bbNode;
		
		state_t numberOfExtraStates;
		unique_ptr<DFSM> conjecture;

		size_t unconfirmedTransitions;
		list<pair<state_t, input_t>> unprocessedConfirmedTransition;
		vector<shared_ptr<requested_query_node_t>> requestedQueries;
		map<size_t, list<sequence_in_t>> queriesFromNextState;
		//map<sequence_in_t, list<set<state_t>>> partitionBySepSeq;
		list<shared_ptr<OTreeNode>> identifiedNodes;
		list<shared_ptr<OTreeNode>> inconsistentNodes;
		sequence_in_t inconsistentSequence; 
		state_t testedState;
		input_t testedInput;
		
		enum OTreeInconsistency { NO_INCONSISTENCY, WRONG_MERGE, EMPTY_CN_DOMAIN, INCONSISTENT_DOMAIN, NEW_STATE_REVEALED }
			inconsistency = NO_INCONSISTENCY;

#if CHECK_PREDECESSORS
		set<ConvergentNode*, CNcomp> nodesWithChangedDomain;
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

	static sequence_in_t getAccessSequence(const shared_ptr<OTreeNode>& parent, const shared_ptr<OTreeNode>& child) {
		auto it = child->accessSequence.begin();
		std::advance(it, parent->accessSequence.size());
		return sequence_in_t(it, child->accessSequence.end());
	}

	static void storeInconsistentNode(const shared_ptr<OTreeNode>& node, SPYLearningInfo& li, SPYLearningInfo::OTreeInconsistency inconsistencyType) {
		if (find(li.inconsistentNodes.begin(), li.inconsistentNodes.end(), node) != li.inconsistentNodes.end()) return;
		if (inconsistencyType == SPYLearningInfo::INCONSISTENT_DOMAIN) {
			li.inconsistency = inconsistencyType;
			li.inconsistentNodes.emplace_front(node);
		}
		else {
			if (li.inconsistentNodes.empty()) {
				li.inconsistency = inconsistencyType;
			}
			li.inconsistentNodes.emplace_back(node);
		}
	}

	static void storeIdentifiedNode(const shared_ptr<OTreeNode>& node, SPYLearningInfo& li) {
		if (li.identifiedNodes.empty()) {
			li.identifiedNodes.emplace_back(node);
		} else if (node->domain.empty()) {
			if (!li.identifiedNodes.front()->domain.empty() ||
					(li.identifiedNodes.front()->accessSequence.size() > node->accessSequence.size())) {
				li.identifiedNodes.clear();
				li.identifiedNodes.emplace_back(node);
			}
		} else if (!li.identifiedNodes.front()->domain.empty()) {
			if (node->accessSequence.size() < li.identifiedNodes.front()->accessSequence.size()) {
				li.identifiedNodes.emplace_front(node); 
			} else {
				li.identifiedNodes.emplace_back(node);
			}
		}
	}
	
	static bool areNodesDifferentUnder(const shared_ptr<OTreeNode>& n1, const shared_ptr<OTreeNode>& n2, long long len) {
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
	
	static bool areNodeAndConvergentDifferentUnder(const shared_ptr<OTreeNode>& node, ConvergentNode* cn) {
		if (node->stateOutput != cn->convergent.front()->stateOutput) return true;
#if CHECK_PREDECESSORS
		auto cn1 = node->convergentNode.lock();
		if (cn->isRN || cn1->isRN)  {
			if (cn->isRN && cn1->isRN) {
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

	static bool areConvergentNodesDistinguished(const shared_ptr<ConvergentNode>& cn1, const shared_ptr<ConvergentNode>& cn2,
		cn_pair_set_t& closed) {
		if (cn1 == cn2) return false;
		if (cn1->convergent.front()->stateOutput != cn2->convergent.front()->stateOutput) return true;
		if (cn1->isRN || cn2->isRN)  {
			if ((cn1->isRN && cn2->isRN) || // different RNs are distinguished
				!cn1->domain.count(cn2.get())) return true;
			if (cn1->isRN) {
				if (!closed.emplace(cn1.get(), cn2.get()).second) return false;
			} else {
				if (!closed.emplace(cn2.get(), cn1.get()).second) return false;
			}
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
					|| areConvergentNodesDistinguished(cn1->next[i], cn2->next[i], closed))
					return true;
			}
		}
		return false;
	}

	static bool areNodesDifferent(const shared_ptr<OTreeNode>& n1, const shared_ptr<OTreeNode>& n2) {
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

	static void appendStateIdentifier(const shared_ptr<requested_query_node_t>& rq, state_t state, seq_len_t len, SPYLearningInfo& li) {
		if (len > 0) {
			for (input_t i = 0; i < li.conjecture->getNumberOfInputs(); i++) {
				auto nn = make_shared<requested_query_node_t>();
				appendStateIdentifier(nn, li.conjecture->getNextState(state, i), len - 1, li);
				rq->next.emplace(i, move(nn));
			}
		}
		for (auto& seq : li.stateIdentifier[state]) {
			auto actRq = rq;
			appendRequestedQuery(actRq, seq);
		}
	}

	static void updateQueriesFromNextState(const shared_ptr<requested_query_node_t>& rq, 
			sequence_in_t& seq, list<sequence_in_t>& queries, SPYLearningInfo& li) {
		if (rq->next.empty()) {
			rq->relatedTransition.emplace_back(-1);
			rq->seqCount = 1;
			queries.emplace_back(seq);
		} else {
			rq->seqCount = 0;
			for (auto& p : rq->next) {
				seq.emplace_back(p.first);
				updateQueriesFromNextState(p.second, seq, queries, li);
				seq.pop_back();
				rq->seqCount += p.second->seqCount;
			}
		}
	}

	static void _generateConvergentSubtree(const shared_ptr<ConvergentNode>& cn, SPYLearningInfo& li,
			const shared_ptr<ConvergentNode>& origCN = nullptr) {
		const auto& node = cn->convergent.front();
		if (origCN) origCN->convergent.remove(node);
		node->convergentNode = cn;
		if (!cn->isRN) {
			for (auto& state : node->domain) {
				cn->domain.emplace(li.ot.rn[state].get());
				li.ot.rn[state]->domain.emplace(cn.get());
			}
		}
		for (input_t input = 0; input < cn->next.size(); input++) {
			if (node->next[input]) {
				if (origCN && origCN->next[input]) {
					if (origCN->next[input]->convergent.size() == 1) {
						cn->next[input].swap(origCN->next[input]);
					}
					else {
						if (node->next[input]->observationStatus != OTreeNode::QUERIED_SN) {// not a state node
							if (li.numberOfExtraStates == 0) node->next[input]->state = NULL_STATE;
							node->next[input]->observationStatus = OTreeNode::QUERIED_NOT_IN_RN;
							cn->next[input] = make_shared<ConvergentNode>(node->next[input]);
							_generateConvergentSubtree(cn->next[input], li, origCN->next[input]);
						}
						else {
							cn->next[input] = node->next[input]->convergentNode.lock();
						}
					}
				}
				else {
					if (node->next[input]->observationStatus != OTreeNode::QUERIED_SN) {// not a state node
						if (li.numberOfExtraStates == 0) node->next[input]->state = NULL_STATE;
						node->next[input]->observationStatus = OTreeNode::QUERIED_NOT_IN_RN;
						cn->next[input] = make_shared<ConvergentNode>(node->next[input]);
						_generateConvergentSubtree(cn->next[input], li);
					}
					else {
						throw;
					}

				}
			}
		}
	}

	static void generateConvergentSubtree(const shared_ptr<ConvergentNode>& cn, SPYLearningInfo& li) {
		const auto& node = cn->convergent.front();
		node->convergentNode = cn;
		if (!cn->isRN) {
			for (auto& state : node->domain) {
				cn->domain.emplace(li.ot.rn[state].get());
				li.ot.rn[state]->domain.emplace(cn.get());
			}
		}
		for (input_t input = 0; input < cn->next.size(); input++) {
			if (node->next[input]) {
				//if (node->next[input]->observationStatus != OTreeNode::QUERIED_SN) {// not a state node
					if (li.numberOfExtraStates == 0) node->next[input]->state = NULL_STATE;
					node->next[input]->observationStatus = OTreeNode::QUERIED_NOT_IN_RN;
					cn->next[input] = make_shared<ConvergentNode>(node->next[input]);
				//}
				generateConvergentSubtree(cn->next[input], li);
			}
		}
	}

	static void separateConvergentSubtree(const shared_ptr<ConvergentNode>& cn,
		const shared_ptr<OTreeNode>& node, SPYLearningInfo& li) {
		//const shared_ptr<ConvergentNode>& origCN = nullptr
		auto origCN = node->convergentNode.lock();
		if (origCN) {
			origCN->convergent.remove(node);
			if (origCN->convergent.empty()) {
				for (auto& rn : origCN->domain) {
					rn->domain.erase(origCN.get());
				}
			}
		}
		node->convergentNode = cn;
		if (!cn->isRN) {
			for (auto& state : node->domain) {
				cn->domain.emplace(li.ot.rn[state].get());
				li.ot.rn[state]->domain.emplace(cn.get());
			}
		}
		for (input_t input = 0; input < cn->next.size(); input++) {
			if (node->next[input]) {
				if (origCN && origCN->next[input] &&
					!origCN->next[input]->isRN && (origCN->next[input]->convergent.size() == 1)) {
					cn->next[input].swap(origCN->next[input]);
				}
				else {
					if (node->next[input]->observationStatus != OTreeNode::QUERIED_SN) {// not a state node
						if (li.numberOfExtraStates == 0) node->next[input]->state = NULL_STATE;
						node->next[input]->observationStatus = OTreeNode::QUERIED_NOT_IN_RN;
						cn->next[input] = make_shared<ConvergentNode>(node->next[input]);
						separateConvergentSubtree(cn->next[input], node->next[input], li);// , origCN ? origCN->next[input] : nullptr);
					}
					else {
						cn->next[input] = node->next[input]->convergentNode.lock();
					}
					if (origCN && origCN->next[input]) {
						bool hasSucc = false;
						for (auto& n : origCN->convergent) {
							if (n->next[input]) {
								hasSucc = true;
								break;
							}
						}
						if (!hasSucc) {
							origCN->next[input].reset();
						}
					}
				}
			}
		}
	}

	static void generateRequestedQueries(SPYLearningInfo& li) {
		size_t tranId = 0;
		vector<state_t> nextStates(li.conjecture->getNumberOfStates() * li.conjecture->getNumberOfInputs());
		li.unconfirmedTransitions = 0;
		li.unprocessedConfirmedTransition.clear();
		li.queriesFromNextState.clear();
		for (auto& cn : li.ot.rn) {
			cn->domain.clear();
			// clear convergent if no ES
			auto stateNode = cn->convergent.front();
			auto state = stateNode->state;
			for (auto& n : cn->convergent) {
				if (li.numberOfExtraStates == 0) n->state = NULL_STATE;
				n->observationStatus = OTreeNode::QUERIED_NOT_IN_RN;
			}
			stateNode->observationStatus = OTreeNode::QUERIED_SN;
			cn->state = stateNode->state = state;
			if (li.numberOfExtraStates == 0) {
				cn->convergent.clear();
				cn->convergent.emplace_back(stateNode);
				cn->next.clear();
				cn->next.resize(li.conjecture->getNumberOfInputs());
			}

			// init requested queries
			li.requestedQueries[state]->next.clear();
			li.requestedQueries[state]->seqCount = 0;
		}
		for (state_t state = 0; state < li.conjecture->getNumberOfStates(); state++) {
			auto& cn = li.ot.rn[state];
			auto& stateNode = cn->convergent.front();
			// update QueriesFromNextState, RelatedTransitions and SeqCounts
			for (input_t i = 0; i < li.conjecture->getNumberOfInputs(); i++, tranId++) {
				auto& succ = stateNode->next[i];
				if (!succ || (succ->observationStatus == OTreeNode::QUERIED_NOT_IN_RN)) {// unconfirmed transition
					auto rq = make_shared<requested_query_node_t>();
					if (li.numberOfExtraStates > 0) {
						appendStateIdentifier(rq, li.conjecture->getNextState(state, i), li.numberOfExtraStates, li);

						list<sequence_in_t> queries;
						sequence_in_t seq;
						updateQueriesFromNextState(rq, seq, queries, li);
						queries.sort();
						li.queriesFromNextState.emplace(tranId, move(queries));
						nextStates[tranId] = li.conjecture->getNextState(state, i);
					} else {
						rq->seqCount = 1;
						if (succ) {
							cn->next[i] = make_shared<ConvergentNode>(succ);
							generateConvergentSubtree(cn->next[i], li);
						}
					}
 					li.requestedQueries[state]->seqCount += rq->seqCount;
					li.requestedQueries[state]->next.emplace(i, move(rq));
					li.unconfirmedTransitions++;
				} else {
					cn->next[i] = li.ot.rn[succ->state];
					if (li.numberOfExtraStates == 0) {
						li.conjecture->setTransition(state, i, succ->state,
							(li.conjecture->isOutputTransition() ? succ->incomingOutput : DEFAULT_OUTPUT));
					}
				}
			}
		}
		for (auto& p : li.queriesFromNextState) {
			for (auto& seq : p.second) {
				auto rq = li.requestedQueries[nextStates[p.first]];
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

	static void checkTransition(state_t state, input_t input, SPYLearningInfo& li) {
		auto& ssRq = li.requestedQueries[state];
		auto it = ssRq->next.find(input);
		if ((it == ssRq->next.end()) || (it->second->seqCount == 0)) {// all seqs applied
			li.unprocessedConfirmedTransition.emplace_back(state, input);
		}
	}

	static state_t getNextStateIfVerified(state_t state, input_t input, SPYLearningInfo& li) {
		auto& ssRq = li.requestedQueries[state];
		auto it = ssRq->next.find(input);
		if ((it == ssRq->next.end()) || (it->second->seqCount == 0)) {// all seqs applied
			auto qIt = li.queriesFromNextState.find(state * li.conjecture->getNumberOfInputs() + input);
			if ((qIt == li.queriesFromNextState.end()) || (qIt->second.empty())) {
				return li.conjecture->getNextState(state, input);
			}
		}
		return NULL_STATE;
	}

	static void processRelatedTransitions(const shared_ptr<requested_query_node_t>& rq, state_t testedState, 
			const sequence_in_t& testedSeq, SPYLearningInfo& li) {
		if (rq->relatedTransition.empty()) return;

		auto numInputs = li.conjecture->getNumberOfInputs();
		for (auto& relTran : rq->relatedTransition) {
			if (relTran == size_t(-1)) {
				decreaseSeqCount(li.requestedQueries[testedState], testedSeq);
				// check tran if verified
				auto qIt = li.queriesFromNextState.find(testedState * numInputs + testedSeq.front());
				if ((qIt == li.queriesFromNextState.end()) || (qIt->second.empty())) {
					checkTransition(testedState, testedSeq.front(), li);
				}
			} else {
				auto& queries = li.queriesFromNextState[relTran];
				queries.erase(lower_bound(queries.begin(), queries.end(), testedSeq));
				if (queries.empty()) {
					// check tran if verified
					state_t startState(state_t(relTran / numInputs));
					input_t input = relTran % numInputs;
					checkTransition(startState, input, li);
				}
			}
		}
		rq->relatedTransition.clear();
	}

	static bool updateRequestedQueries(const shared_ptr<requested_query_node_t>& rq, const shared_ptr<OTreeNode>& spyNode,
		const state_t& state, sequence_in_t& testedSeq, SPYLearningInfo& li) {

		processRelatedTransitions(rq, state, testedSeq, li);
		if (rq->next.empty()) return true;

		for (auto it = rq->next.begin(); it != rq->next.end();) {
			if (spyNode->next[it->first]) {
				testedSeq.emplace_back(it->first);
				if (updateRequestedQueries(it->second, spyNode->next[it->first], state, testedSeq, li)) {
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
	

	static void query(const shared_ptr<OTreeNode>& node, input_t input, SPYLearningInfo& li, const unique_ptr<Teacher>& teacher) {
		state_t transitionOutput, stateOutput = DEFAULT_OUTPUT;
		if (!teacher->isProvidedOnlyMQ() && (li.conjecture->getType() == TYPE_DFSM)) {
			sequence_in_t suffix({ input, STOUT_INPUT });
			auto output = (li.bbNode == node) ? teacher->outputQuery(suffix) :
				teacher->resetAndOutputQueryOnSuffix(node->accessSequence, suffix);
			transitionOutput = output.front();
			stateOutput = output.back();
#if DUMP_OQ 
			if (li.bbNode == node) {
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
			transitionOutput = (li.bbNode == node) ? teacher->outputQuery(input) :
				teacher->resetAndOutputQueryOnSuffix(node->accessSequence, input);
#if DUMP_OQ 
			if (li.bbNode == node) {
				printf("%d T(%d) = %d query\n", teacher->getOutputQueryCount(), input, transitionOutput);
			}
			else {
				printf("%d T(%s, %d) = %d query\n", teacher->getOutputQueryCount(),
					FSMmodel::getInSequenceAsString(node->accessSequence).c_str(), input, transitionOutput);
			}
#endif // DUMP_OQ
			if (li.conjecture->getType() == TYPE_DFSM) {
				stateOutput = teacher->outputQuery(STOUT_INPUT);
#if DUMP_OQ 
				printf("%d T(S) = %d query\n", teacher->getOutputQueryCount(), stateOutput);
#endif // DUMP_OQ
			}
			else if (!li.conjecture->isOutputTransition()) {// Moore, DFA
				stateOutput = transitionOutput;
				transitionOutput = DEFAULT_OUTPUT;
			}
		}
#if DUMP_OQ 
		auto otreeState = li.observationTree->addState(stateOutput);
		auto otreeStartState = li.observationTree->getEndPathState(0, node->accessSequence);
		li.observationTree->setTransition(otreeStartState, input, otreeState, (li.observationTree->isOutputTransition() ? transitionOutput : DEFAULT_OUTPUT));
#endif // DUMP_OQ
		checkNumberOfOutputs(teacher, li.conjecture);
		auto leaf = make_shared<OTreeNode>(node, input, transitionOutput, stateOutput,//li.numberOfExtraStates > 0 ? li.conjecture->getNextState(node->state, input) : 
			(li.numberOfExtraStates > 0) ? li.conjecture->getNextState(node->state, input) : NULL_STATE, li.conjecture->getNumberOfInputs());// teacher->getNextPossibleInputs());
		leaf->observationStatus = OTreeNode::QUERIED_NOT_IN_RN;
		if (li.conjecture->getNumberOfInputs() != teacher->getNumberOfInputs()) {
			// update all requested spyOTree
			li.conjecture->incNumberOfInputs(teacher->getNumberOfInputs() - li.conjecture->getNumberOfInputs());
		}
		node->next[input] = leaf;
		li.bbNode = leaf;
		if (li.numberOfExtraStates > 0) {
			li.ot.rn[leaf->state]->convergent.emplace_back(leaf);
			leaf->convergentNode = li.ot.rn[leaf->state];
		}
		auto domIt = leaf->domain.end();
		// init domain
		for (state_t state = 0; state < li.conjecture->getNumberOfStates(); state++) {
			if (li.conjecture->isOutputState()) {
				if (li.ot.rn[state]->convergent.front()->stateOutput == stateOutput) {
					domIt = leaf->domain.emplace_hint(domIt, state);
				}
			}
			else {
				domIt = leaf->domain.emplace_hint(domIt, state);
			}
		}
	}

	static void updateOTreeWithNewState(const shared_ptr<OTreeNode>& node, SPYLearningInfo& li) {
		queue<shared_ptr<OTreeNode>> nodes;
		nodes.emplace(li.ot.rn[0]->convergent.front());//root
		while (!nodes.empty()) {
			auto otNode = move(nodes.front());
			nodes.pop();
			if (node && ((node == otNode) || !areNodesDifferent(node, otNode))) {
				otNode->domain.emplace(node->state);
			}
			if (otNode->observationStatus != OTreeNode::QUERIED_SN) {
				otNode->state = NULL_STATE;
				if (otNode->domain.size() <= 1) {
					storeIdentifiedNode(otNode, li);
				}
			}
			for (const auto& nn : otNode->next) {
				if (nn) nodes.emplace(nn);
			}
		}	
	}

	static bool isSeparatingSequenceQueried(shared_ptr<OTreeNode> node, state_t state, sequence_in_t& sepSeq, SPYLearningInfo& li) {
		auto otherNode = li.ot.rn[state]->convergent.front();
		if (node->stateOutput != otherNode->stateOutput) return true; 
		
		auto& seq = li.separatingSequences[FSMsequence::getStatePairIdx(state, node->state)];
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

	static sequence_in_t getQueriedSeparatingSequence(const shared_ptr<OTreeNode>& n1, const shared_ptr<OTreeNode>& n2) {
		if (n1->stateOutput != n2->stateOutput) {
			return sequence_in_t();
		}
		sequence_in_t retVal;
		queue<pair<shared_ptr<OTreeNode>, shared_ptr<OTreeNode>>> fifo;
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
			if (retVal.empty() && isIntersectionEmpty(p.first->domain, p.second->domain)) {
				retVal = getAccessSequence(n1, p.first);
				retVal.emplace_front(STOUT_INPUT);
			}
#endif
			fifo.pop();
		}
		return retVal;
	}

	static bool isCNdifferent(const shared_ptr<ConvergentNode>& cn1, const shared_ptr<ConvergentNode>& cn2,
		const shared_ptr<ConvergentNode>& cnPredecessor1, const shared_ptr<ConvergentNode>& cnPredecessor2, cn_pair_set_t& closed) {
		if (closed.count(make_pair(cn1.get(), cn2.get())) || closed.count(make_pair(cn2.get(), cn1.get()))) return false;
		if ((cn1->state == WRONG_STATE) || (cn2->state == WRONG_STATE)) {
			return false;
		}
		if (cn1->isRN || cn2->isRN) {
			if (cn1->isRN && cn2->isRN) {
				return (cn1 != cn2);
			}
			//if ((cn1 != cnPredecessor1) || (cn2 != cnPredecessor2)) {
			if (cn1->isRN) {
				if (!cn2->domain.empty() && !cn2->domain.count(cn1.get())) return true;
			}
			else {
				if (!cn1->domain.empty() && !cn1->domain.count(cn2.get())) return true;
			}
			//}
		}
		else {// if (((cn1 != cnPredecessor1) || !cn2->domain.count(cnPredecessor2.get()))
			//&& ((cn1 != cnPredecessor2) || !cn1->domain.count(cnPredecessor2.get()))) {// !cn1->isRN && !cn2->isRN
			if (cn2->domain.empty() || isIntersectionEmpty(cn1->domain, cn2->domain)) {//cn1->domain.empty() || 
				return true;
			}
			if (cn2->domain.empty()) {
				//if ((cnPredecessor1 == cn2) && !cn1->domain.count(cnPredecessor2.get())) return true;
				auto domain = cn1->domain;
				for (auto& node : cn2->convergent) {
					for (auto it = domain.begin(); it != domain.end();) {
						if (!node->domain.count((*it)->state)){
							it = domain.erase(it);
						}
						else {
							++it;
						}
					}
					if (domain.empty()) return true;
				}/*
				 for (auto& cn : domain) {
				 cn_pair_set_t closed;
				 if (!areConvergentNodesDistinguished(cn2, cn->convergent.front()->convergentNode.lock(), closed)) {
				 return false;
				 }
				 }
				 return true;*/
			}
		}
		return false;
	}

	static sequence_in_t getQueriedSeparatingSequenceFromSN(const shared_ptr<OTreeNode>& n1, const shared_ptr<ConvergentNode>& cn2,
		cn_pair_set_t& closed) {
		if (n1->stateOutput != cn2->convergent.front()->stateOutput) {
			return sequence_in_t();
		}
#if CHECK_PREDECESSORS
		closed.emplace(n1->convergentNode.lock().get(), cn2.get());
#endif
		queue<pair<shared_ptr<OTreeNode>, shared_ptr<ConvergentNode>>> fifo;
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
			if (retVal.empty() && isCNdifferent(p.first->convergentNode.lock(), p.second, n1->convergentNode.lock(), cn2, closed)) {
				retVal = getAccessSequence(n1, p.first);
				retVal.emplace_front(STOUT_INPUT);
			}
#endif
			fifo.pop();
		}
		return retVal;
	}

	static sequence_in_t getQueriedSeparatingSequenceOfCN(const shared_ptr<ConvergentNode>& cn1, const shared_ptr<ConvergentNode>& cn2,
		cn_pair_set_t& closed) {
		if (cn1->convergent.front()->stateOutput != cn2->convergent.front()->stateOutput) {
			return sequence_in_t();
		}
		closed.emplace(cn1.get(), cn2.get());
		queue<pair<shared_ptr<ConvergentNode>, shared_ptr<ConvergentNode>>> fifo;
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
					if ((p.first->next[i] != p.second->next[i]) &&
						(!closed.count(make_pair(p.first->next[i].get(), p.second->next[i].get())))) {
						fifo.emplace(p.first->next[i], p.second->next[i]);
						auto seq(seqFifo.front());
						seq.emplace_back(i);
						seqFifo.emplace(move(seq));
					}
				}
			}
#if CHECK_PREDECESSORS
			if (retVal.empty() && !seqFifo.front().empty() && isCNdifferent(p.first, p.second, cn1, cn2, closed)) {
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


	static bool mergeConvergent(state_t state, const shared_ptr<OTreeNode>& spyNode, SPYLearningInfo& li, bool forceMerge = false) {

		if (!spyNode->domain.count(state)) {// discrepancy found
			//spyNode->assumedState = state;
			storeInconsistentNode(spyNode, li, SPYLearningInfo::WRONG_MERGE);
			return false;
		}
		if (spyNode->observationStatus != OTreeNode::QUERIED_NOT_IN_RN) {
			if (!forceMerge) return true;
		}
		else {
			spyNode->observationStatus = OTreeNode::QUERIED_IN_RN;
		}
		//if ((spyNode->state != NULL_STATE) && !forceMerge) {
			// already merged + hack for refNode 
			// TODO is it needed?
			//return true;
		//}

		spyNode->state = state;
		const auto& cn = li.ot.rn[state];
		spyNode->convergentNode = cn;
		// updating requestedQueries 
		sequence_in_t seq;
		updateRequestedQueries(li.requestedQueries[state], spyNode, state, seq, li);

		for (input_t i = 0; i < li.conjecture->getNumberOfInputs(); i++) {
			if (spyNode->next[i]) {
				auto ns = getNextStateIfVerified(state, i, li);
				if (ns != NULL_STATE) {
					if (!mergeConvergent(ns, spyNode->next[i], li)) {
						//cn->convergent.emplace_back(spyNode);
						return false;
					}
				}
			}
		}
		/*
		if (spyNode->accessSequence.size() < cn->convergent.front()->accessSequence.size()) {
			//TODO change domain
			//cn->convergent.emplace_front(spyNode);
			cn->convergent.emplace_back(spyNode);
		}
		else if (spyNode != cn->convergent.front()) {
			cn->convergent.emplace_back(spyNode);
		}*/
		return true;
	}

	static void processConfirmedTransitions(SPYLearningInfo& li) {
		while (!li.unprocessedConfirmedTransition.empty()) {
			auto state = li.unprocessedConfirmedTransition.front().first;
			auto input = li.unprocessedConfirmedTransition.front().second;
			auto nextState = li.conjecture->getNextState(state, input);
			auto& fromCN = li.ot.rn[state];
			li.ot.rn[state]->next[input] = li.ot.rn[nextState];
			for (auto& fcn : fromCN->convergent) {
				if ((fcn->observationStatus != OTreeNode::QUERIED_NOT_IN_RN) && (fcn->next[input])) {
					if (!mergeConvergent(nextState, fcn->next[input], li))  {
						li.unprocessedConfirmedTransition.clear();
						return;
					}
				}
			}
			li.unprocessedConfirmedTransition.pop_front();
			li.unconfirmedTransitions--;
		}
	}

#if CHECK_PREDECESSORS
	static bool checkPredecessors(shared_ptr<OTreeNode> node, shared_ptr<ConvergentNode> parentCN, SPYLearningInfo& li) {
		bool reduced;
		do {
			reduced = false;
			auto input = node->accessSequence.back();
			for (auto it = parentCN->domain.begin(); it != parentCN->domain.end();) {
				if ((*it)->next[input] && (((*it)->next[input]->isRN &&
					(!(*it)->next[input]->domain.count(parentCN->next[input].get())))
					|| (!(*it)->next[input]->isRN &&
					isIntersectionEmpty(parentCN->next[input]->domain, (*it)->next[input]->domain)))) {

					(*it)->domain.erase(parentCN.get());
					if (parentCN->isRN) {// a state node
						if ((*it)->domain.empty()) {
							storeInconsistentNode((*it)->convergent.front(), li);
							it = parentCN->domain.erase(it);
							li.identifiedNodes.clear();
							return false;
						}
						if ((*it)->domain.size() == 1) {
							storeIdentifiedNode((*it)->convergent.front(), li);
						}
						li.nodesWithChangedDomain.insert(*it);
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
					storeInconsistentNode(parentCN->convergent.front(), li);
					li.identifiedNodes.clear();
					return false;
				}
				if (parentCN->domain.size() == 1) {
					storeIdentifiedNode(parentCN->convergent.front(), li);
					//break;
				}
				node = parentCN->convergent.front();
				parentCN = node->parent.lock()->convergentNode.lock();
			}
		} while (reduced);
		return true;
	}

	static bool processChangedNodes(SPYLearningInfo& li) {
		while (!li.nodesWithChangedDomain.empty()) {
			auto it = li.nodesWithChangedDomain.end();
			--it;
			auto node = (*it)->convergent.front();
			li.nodesWithChangedDomain.erase(it);
			if (!checkPredecessors(node, node->parent.lock()->convergentNode.lock(), li)) {
				li.nodesWithChangedDomain.clear();
				return false;
			}
		}
		return true;
	}
	
	static bool reduceDomainsBySuccessors(SPYLearningInfo& li) {
		for (const auto& cn : li.ot.rn) {
			for (auto it = cn->domain.begin(); it != cn->domain.end();) {
				if (areConvergentNodesDistinguished(cn, (*it)->convergent.front()->convergentNode.lock())) {
					(*it)->domain.erase(cn.get());
					if ((*it)->domain.empty()) {
						storeInconsistentNode((*it)->convergent.front(), li);
						it = cn->domain.erase(it);
						li.nodesWithChangedDomain.clear();
						return false;
					}
					if ((*it)->domain.size() == 1) {
						storeIdentifiedNode((*it)->convergent.front(), li);
					}
					li.nodesWithChangedDomain.insert(*it);
					it = cn->domain.erase(it);
				} else {
					++it;
				}
			}
		}
		return processChangedNodes(li);
	}
#endif

	static bool mergeConvergentNoES(shared_ptr<ConvergentNode>& fromCN, const shared_ptr<ConvergentNode>& toCN, SPYLearningInfo& li) {
		if (toCN->isRN) {// a state node
			//if (li.inconsistency == SPYLearningInfo::NO_INCONSISTENCY) {
				if (fromCN->domain.find(toCN.get()) == fromCN->domain.end()) {
#if CHECK_PREDECESSORS
					for (auto& node : fromCN->convergent) {
						if (!node->domain.count(toCN->state)) {// inconsistent node
							storeInconsistentNode(node, li, LearningInfo::INCONSISTENT_DOMAIN);
						}
					}
					li.nodesWithChangedDomain.insert(fromCN.get());
					//return false;
#else
					for (auto& n : fromCN->convergent) {
						if (!n->domain.count(toCN->state)) {
							li.inconsistentSequence = getQueriedSeparatingSequence(n, toCN->convergent.front());
							break;
						}
					}
					li.testedState = NULL_STATE;
					if (li.inconsistentSequence.empty()) {
						cn_pair_set_t closed;
						li.inconsistentSequence = getQueriedSeparatingSequenceOfCN(fromCN, toCN, closed);
						li.testedState = toCN->state;
						li.testedInput = STOUT_INPUT;
						if (li.inconsistentSequence.empty()) {
							li.inconsistentNodes.push_back(fromCN->convergent.front());
						}
						else if (li.inconsistentSequence.front() == STOUT_INPUT) {
							throw "";
							li.inconsistentSequence.pop_front();
							li.inconsistentSequence.push_back(STOUT_INPUT);
						}
						else {
							li.testedInput = input_t(li.inconsistentSequence.size()); // a hack to derive separating suffix
						}
					}
					//storeInconsistentNode(fromCN->convergent.front(), toCN->convergent.front(), li, LearningInfo::EMPTY_CN_DOMAIN);
					return false;
#endif	
				}
				for (auto toIt = toCN->domain.begin(); toIt != toCN->domain.end();) {
					cn_pair_set_t closed;
					if (areConvergentNodesDistinguished(fromCN, (*toIt)->convergent.front()->convergentNode.lock(), closed)) {
						(*toIt)->domain.erase(toCN.get());
						if ((*toIt)->domain.empty()) {
							//auto& n = fromCN->convergent.front();
							//n->convergentNode = fromCN;
							//n->assumedState = WRONG_STATE;
							//toCN->convergent.remove(n);
							storeInconsistentNode((*toIt)->convergent.front(), li, SPYLearningInfo::EMPTY_CN_DOMAIN);
							//return false;
						}
						if ((*toIt)->domain.size() == 1) {
							storeIdentifiedNode((*toIt)->convergent.front(), li);
						}
#if CHECK_PREDECESSORS
						if (li.inconsistentNodes.empty()) {
							li.nodesWithChangedDomain.insert(*toIt);
						}
#endif	
						toIt = toCN->domain.erase(toIt);
					}
					else {
						++toIt;
					}
				//}
			}
			for (auto& consistent : fromCN->domain) {
				consistent->domain.erase(fromCN.get());
			}
			//fromCN->domain.clear();
			auto& state = toCN->convergent.front()->state;
			for (auto& node : fromCN->convergent) {
				node->state = state;
				node->observationStatus = OTreeNode::QUERIED_IN_RN;
				node->convergentNode = toCN;
				//if (node->accessSequence.size() < toCN->convergent.front()->accessSequence.size()) swap ref state nodes
				// TODO swap refSN
				toCN->convergent.emplace_back(node);
			}
		}
		else {
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
			bool reduced = false;
			// intersection of domains
			auto toIt = toCN->domain.begin();
			while (toIt != toCN->domain.end()) {
				if (fromCN->domain.count(*toIt)) {
					++toIt;
				}
				else {
					(*toIt)->domain.erase(toCN.get());
					toIt = toCN->domain.erase(toIt);
					reduced = true;
				}
			}
			for (auto& rn : fromCN->domain) {
				rn->domain.erase(fromCN.get());
			}
			if (li.inconsistency == SPYLearningInfo::NO_INCONSISTENCY) {
				if (toCN->domain.empty()) {
					auto& n = toCN->convergent.front();
					//n->convergentNode = fromCN;
					//toCN->convergent.remove(n);
					//li.inconsistentNodes.clear();
					storeInconsistentNode(n, li, SPYLearningInfo::EMPTY_CN_DOMAIN);
					//return false;
				}
				else if (reduced) {
					if (toCN->domain.size() == 1) {
						storeIdentifiedNode(toCN->convergent.front(), li);
					}
#if CHECK_PREDECESSORS
					if (li.inconsistentNodes.empty()) {
						li.nodesWithChangedDomain.insert(toCN.get());
					}
#endif	
				}
			}
		}
		auto tmpCN = fromCN;
		fromCN = toCN;
		// merge successors
		for (input_t i = 0; i < li.conjecture->getNumberOfInputs(); i++) {
			if (tmpCN->next[i]) {
				if (toCN->next[i]) {
					if (tmpCN->next[i] == toCN->next[i]) continue;
					if (tmpCN->next[i]->isRN) {
						if (toCN->next[i]->isRN) {// a different state node
							li.inconsistentSequence = getQueriedSeparatingSequence(
								tmpCN->next[i]->convergent.front(), toCN->next[i]->convergent.front());
							if (li.inconsistentSequence.empty()) {
								throw "";
							}
							li.testedState = NULL_STATE;
							li.inconsistentSequence.emplace_front(i);
							fromCN = tmpCN;
							return false;
						}
						if (!mergeConvergentNoES(toCN->next[i], tmpCN->next[i], li)) {
							li.inconsistentSequence.emplace_front(i);
							fromCN = tmpCN;
							if ((li.testedState != NULL_STATE) && (li.testedInput == STOUT_INPUT)) {
								if (toCN->isRN && (toCN->state == li.testedState)) {
									cn_pair_set_t closed;
									auto sepSeq = getQueriedSeparatingSequenceOfCN(li.inconsistentNodes.back()->convergentNode.lock(),
										fromCN, closed);
									if (!sepSeq.empty()) {
										if (li.inconsistentSequence.back() == STOUT_INPUT) li.inconsistentSequence.pop_back();
										li.testedState = NULL_STATE;
										li.testedInput = input_t(sepSeq.size()); // a hack to derive separating suffix
										li.inconsistentSequence.splice(li.inconsistentSequence.end(), move(sepSeq));
									}
								}
								if (li.testedState != NULL_STATE) {
									if (li.inconsistentSequence.empty() || (li.inconsistentSequence.back() != STOUT_INPUT)) {
										li.inconsistentSequence.push_back(STOUT_INPUT);
									}
									else if (!li.inconsistentSequence.empty()) {
										li.inconsistentSequence.pop_back();
									}
								}
							}
#if CHECK_PREDECESSORS
							for (auto& node : fromCN->convergent) {
								node->convergentNode = fromCN;
							}
							if (li.inconsistentNodes.empty()) {
								li.nodesWithChangedDomain.insert(fromCN.get());
							}
#endif
							return false;
						}
						if (toCN->isRN) {// state node
							auto& rq = li.requestedQueries[toCN->state];
							auto it = rq->next.find(i);
							if (it != rq->next.end()) {
								auto nIt = toCN->convergent.begin();
								while (!(*nIt)->next[i] || ((*nIt)->next[i]->state == NULL_STATE)) ++nIt;
								auto& node = (*nIt)->next[i];
								li.conjecture->setTransition((*nIt)->state, i, node->state,
									(li.conjecture->isOutputTransition() ? node->incomingOutput : DEFAULT_OUTPUT));
								li.unconfirmedTransitions--;
								rq->seqCount--;
								rq->next.erase(it);
							}
						}
					}
					else {
						if (!mergeConvergentNoES(tmpCN->next[i], toCN->next[i], li)) {
							li.inconsistentSequence.emplace_front(i);
							fromCN = tmpCN;
							if ((li.testedState != NULL_STATE) && (li.testedInput == STOUT_INPUT)
								&& toCN->isRN && (toCN->state == li.testedState)) {
								cn_pair_set_t closed;
								auto sepSeq = getQueriedSeparatingSequenceOfCN(li.inconsistentNodes.back()->convergentNode.lock(),
									fromCN, closed);
								if (!sepSeq.empty()) {
									if (li.inconsistentSequence.back() == STOUT_INPUT) li.inconsistentSequence.pop_back();
									li.testedState = NULL_STATE;
									li.testedInput = input_t(sepSeq.size()); // a hack to derive separating suffix
									li.inconsistentSequence.splice(li.inconsistentSequence.end(), move(sepSeq));
								}
							}
#if CHECK_PREDECESSORS
							for (auto& node : fromCN->convergent) {
								node->convergentNode = fromCN;
							}
							if (li.inconsistentNodes.empty()) {
								li.nodesWithChangedDomain.insert(fromCN.get());
							}
#endif
							return false;
						}
					}
				}
				else {
					toCN->next[i] = tmpCN->next[i];
					if (toCN->isRN && toCN->next[i]->isRN) {// state nodes
						auto& rq = li.requestedQueries[toCN->state];
						auto it = rq->next.find(i);
						if (it != rq->next.end()) {
							auto nIt = toCN->convergent.begin();
							while (!(*nIt)->next[i] || ((*nIt)->next[i]->state == NULL_STATE)) ++nIt;
							auto& node = (*nIt)->next[i];
							li.conjecture->setTransition((*nIt)->state, i, node->state,
								(li.conjecture->isOutputTransition() ? node->incomingOutput : DEFAULT_OUTPUT));
							li.unconfirmedTransitions--;
							rq->seqCount--;
							rq->next.erase(it);
						}
					}
				}
			}
		}
#if CHECK_PREDECESSORS
		li.nodesWithChangedDomain.erase(tmpCN.get());
#endif	
		return true;
	}

	static bool processIdentified(SPYLearningInfo& li) {
		while (!li.identifiedNodes.empty()) {
			auto node = move(li.identifiedNodes.front());
			li.identifiedNodes.pop_front();
			if (node->state == NULL_STATE) {
				auto parentCN = node->parent.lock()->convergentNode.lock();
				auto input = node->accessSequence.back();
				auto refCN = (*(node->convergentNode.lock()->domain.begin()))->convergent.front()->convergentNode.lock();
				cn_pair_set_t closed;
				auto cn = node->convergentNode.lock();
				if (areConvergentNodesDistinguished(cn, refCN, closed)) {
					cn->domain.erase(refCN.get());
					storeInconsistentNode(node, li, SPYLearningInfo::EMPTY_CN_DOMAIN);
					li.identifiedNodes.clear();
					return false;
				}
				if (!mergeConvergentNoES(parentCN->next[input], refCN, li)) {
					refCN->convergent.remove(node);
					node->state = NULL_STATE;
					node->observationStatus = OTreeNode::QUERIED_NOT_IN_RN;
					node->convergentNode = parentCN->next[input];
					li.identifiedNodes.clear();
#if CHECK_PREDECESSORS
					if (!li.inconsistentNodes.empty() || !processChangedNodes(li)) {
						li.nodesWithChangedDomain.clear();
						return false;
					}
#else
					if ((li.testedState != NULL_STATE) && (li.testedInput == STOUT_INPUT)) {
						cn_pair_set_t closed;
						auto sepSeq = getQueriedSeparatingSequenceOfCN(li.inconsistentNodes.back()->convergentNode.lock(),
							(li.inconsistentSequence.back() == STOUT_INPUT) ? cn : li.ot.rn[li.testedState], closed);
						if (sepSeq.empty()) {
							sepSeq = getQueriedSeparatingSequenceOfCN(li.inconsistentNodes.back()->convergentNode.lock(),
								(li.inconsistentSequence.back() == STOUT_INPUT) ? li.ot.rn[li.testedState] : cn, closed);
							if (sepSeq.empty()) {
								throw;
							}
						}
						if (li.inconsistentSequence.back() == STOUT_INPUT) li.inconsistentSequence.pop_back();
						li.testedInput = input_t(sepSeq.size()); // a hack to derive separating suffix
						li.inconsistentSequence.splice(li.inconsistentSequence.end(), move(sepSeq));
					}
					li.inconsistentNodes.clear();
					storeInconsistentNode(node, li, SPYLearningInfo::WRONG_MERGE);
#endif	
					return false;
				}
				if (!li.inconsistentNodes.empty()
#if CHECK_PREDECESSORS
						|| !processChangedNodes(li)
#endif	
					) {
#if CHECK_PREDECESSORS
					li.nodesWithChangedDomain.clear();
#endif	
					li.identifiedNodes.clear();
					return false;
				}
				if (parentCN->isRN) {// state node
					auto& rq = li.requestedQueries[parentCN->state];
					auto it = rq->next.find(input);
					if (it != rq->next.end()) {
						li.conjecture->setTransition(parentCN->convergent.front()->state, input, node->state,
							(li.conjecture->isOutputTransition() ? node->incomingOutput : DEFAULT_OUTPUT));
						li.unconfirmedTransitions--;
						rq->seqCount--;
						rq->next.erase(it);
					}
					else {
						li.identifiedNodes.clear();
						return false;
					}
				}
#if CHECK_PREDECESSORS
				else {
					bool reduced = false;
					for (auto it = parentCN->domain.begin(); it != parentCN->domain.end();) {
						if ((*it)->next[input] &&
							(((*it)->next[input]->isRN && ((*it)->next[input] != refCN)) ||
							(!(*it)->next[input]->isRN && !(*it)->next[input]->domain.count(refCN.get())))) {

							(*it)->domain.erase(parentCN.get());
							it = parentCN->domain.erase(it);
							reduced = true;
						}
						else ++it;
					}
					if (reduced) {
						if (parentCN->domain.empty()) {
							storeInconsistentNode(parentCN->convergent.front(), li);
							li.identifiedNodes.clear();
							return false;
						}
						if (parentCN->domain.size() == 1) {
							storeIdentifiedNode(parentCN->convergent.front(), li);
						}
						li.nodesWithChangedDomain.insert(parentCN.get());
						if (!processChangedNodes(li)) {
							return false;
						}
					}
				}
#endif
			}
		}
		return true;
	}

	static void checkNode(const shared_ptr<OTreeNode>& node, SPYLearningInfo& li) {
		if (node->domain.empty()) {// new state
			storeIdentifiedNode(node, li);
			li.inconsistency = SPYLearningInfo::NEW_STATE_REVEALED;
		}
		else if (node->state == NULL_STATE) {
			if (node->convergentNode.lock()->domain.empty()) {
				storeInconsistentNode(node, li, SPYLearningInfo::EMPTY_CN_DOMAIN);
			}
			else if (node->convergentNode.lock()->domain.size() == 1) {
				storeIdentifiedNode(node, li);
			}
		}
		else if (!node->domain.count(node->state)) {// inconsistent node
			storeInconsistentNode(node, li, SPYLearningInfo::INCONSISTENT_DOMAIN);
		}
	}

	static void reduceDomain(const shared_ptr<OTreeNode>& node, const long long& suffixLen, SPYLearningInfo& li) {
		const auto& cn = node->convergentNode.lock();
		for (auto snIt = node->domain.begin(); snIt != node->domain.end();) {
			if (areNodesDifferentUnder(node, li.ot.rn[*snIt]->convergent.front(), suffixLen)) {
				if ((li.numberOfExtraStates == 0) && (cn->domain.erase(li.ot.rn[*snIt].get()))) {
					li.ot.rn[*snIt]->domain.erase(cn.get());
				}
				snIt = node->domain.erase(snIt);
			}
			else {
				if ((li.inconsistency < SPYLearningInfo::INCONSISTENT_DOMAIN) && (li.numberOfExtraStates == 0)) {
					auto cnIt = cn->domain.find(li.ot.rn[*snIt].get());
					if (cnIt != cn->domain.end()) {
						if (areNodeAndConvergentDifferentUnder(node, li.ot.rn[*snIt].get())) {
							cn->domain.erase(cnIt);
							li.ot.rn[*snIt]->domain.erase(cn.get());
						}
					}
				}
				++snIt;
			}
		}
		checkNode(node, li);
	}

	static void checkAgainstAllNodes(const shared_ptr<OTreeNode>& node, const long long& suffixLen, SPYLearningInfo& li) {
		stack<shared_ptr<OTreeNode>> nodes;
		nodes.emplace(li.ot.rn[0]->convergent.front());//root
		while (!nodes.empty()) {
			auto otNode = move(nodes.top());
			nodes.pop();
			if ((otNode != node) && (otNode->domain.count(node->state))) {
				if (areNodesDifferentUnder(node, otNode, suffixLen)) {
					otNode->domain.erase(node->state);
					if ((li.inconsistency < SPYLearningInfo::INCONSISTENT_DOMAIN) && (li.numberOfExtraStates == 0) 
						&& otNode->convergentNode.lock()->domain.erase(li.ot.rn[node->state].get())) {
						li.ot.rn[node->state]->domain.erase(otNode->convergentNode.lock().get());
#if CHECK_PREDECESSORS
						if (li.inconsistency == LearningInfo::NO_INCONSISTENCY) {
							li.nodesWithChangedDomain.insert(otNode->convergentNode.lock().get());
							processChangedNodes(li);
						}
#endif
					}
					checkNode(otNode, li);
				}
			}
			for (const auto& nn : otNode->next) {
				if (nn && (static_cast<long long>(nn->maxSuffixLen) >= suffixLen)) nodes.emplace(nn);
			}
		}
	}

	static void reduceDomainStateNode(const shared_ptr<OTreeNode>& node, const long long& suffixLen, SPYLearningInfo& li) {
		if (node->observationStatus == OTreeNode::QUERIED_SN) {
			checkAgainstAllNodes(node, suffixLen, li);
		}
		else {
			for (auto snIt = node->domain.begin(); snIt != node->domain.end();) {
				if (areNodesDifferentUnder(node, li.ot.rn[*snIt]->convergent.front(), suffixLen)) {
					snIt = node->domain.erase(snIt);
				}
				else {
					++snIt;
				}
			}
			checkNode(node, li);
		}
		if ((li.inconsistency < SPYLearningInfo::INCONSISTENT_DOMAIN) && (li.numberOfExtraStates == 0)) {
			const auto& cn = node->convergentNode.lock();
			for (auto cnIt = cn->domain.begin(); cnIt != cn->domain.end();) {
				if (areNodeAndConvergentDifferentUnder(node, *cnIt)) {
					(*cnIt)->domain.erase(cn.get());
					checkNode((*cnIt)->convergent.front(), li);
#if CHECK_PREDECESSORS
					if (li.inconsistency == LearningInfo::NO_INCONSISTENCY) {
						li.nodesWithChangedDomain.insert(*cnIt);
					}
#endif
					cnIt = cn->domain.erase(cnIt);
				}
				else ++cnIt;
			}
#if CHECK_PREDECESSORS
			if (li.inconsistency == LearningInfo::NO_INCONSISTENCY) {
				processChangedNodes(li);
			}
			else {
				li.nodesWithChangedDomain.clear();
			}
#endif
		}
	}

	static void checkPrevious(shared_ptr<OTreeNode> node, SPYLearningInfo& li, long long suffixAdded = 0) {
		seq_len_t suffixLen = 0;
		input_t input(STOUT_INPUT);
		do {
			node->lastQueriedInput = input;
			if (suffixLen > node->maxSuffixLen) node->maxSuffixLen = suffixLen;
			if (node->state == NULL_STATE) {
				reduceDomain(node, suffixAdded, li);
			}
			else {// state node
				reduceDomainStateNode(node, suffixAdded, li);
			}
			if (!node->accessSequence.empty()) input = node->accessSequence.back();
			node = node->parent.lock();
			suffixLen++; suffixAdded++;
		} while (node);
	}

	static void moveWithCN(shared_ptr<OTreeNode>& currNode, input_t input, SPYLearningInfo& li) {
		if (li.numberOfExtraStates > 0) {
			if (currNode->observationStatus != OTreeNode::QUERIED_NOT_IN_RN) {//&& (li.ot.rn[currNode->state]->next[input])) {
				auto state = getNextStateIfVerified(currNode->state, input, li);
				if (state != NULL_STATE) {
					mergeConvergent(state, currNode->next[input], li);
				}
			}
			currNode = currNode->next[input];
			return;
		}
		auto cn = currNode->convergentNode.lock();
		if (cn->next[input]) {
			auto it = cn->convergent.begin();
			while ((*it == currNode) || (!(*it)->next[input])) {
				++it;
			}
			auto& refNode = (*it)->next[input];
			auto& currNext = currNode->next[input];
			auto& nextCN = cn->next[input];
			if ((refNode->incomingOutput != currNext->incomingOutput) ||
				(refNode->stateOutput != currNext->stateOutput)) {
				currNext->state = WRONG_STATE;
				nextCN->state = WRONG_STATE;
				//storeInconsistentNode(currNext, li);//refNode
			}
			currNode = currNext;
			if (nextCN->isRN) {// state node
				if (currNode->state != WRONG_STATE) {
					currNode->state = nextCN->state;
				}
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
			auto nextCN = make_shared<ConvergentNode>(currNode);
			for (auto state : currNode->domain) {
				nextCN->domain.emplace(li.ot.rn[state].get());
				li.ot.rn[state]->domain.emplace(nextCN.get());
			}
			cn->next[input] = move(nextCN);
		}
		currNode->convergentNode = cn->next[input];
	}

	static void checkRQofPrevious(shared_ptr<OTreeNode> node, SPYLearningInfo& li) {
		sequence_in_t seq;
		while (!node->accessSequence.empty()) {
			seq.push_front(node->accessSequence.back());
			node = node->parent.lock();
			if (node->observationStatus != OTreeNode::QUERIED_NOT_IN_RN) {
				auto rq = li.requestedQueries[node->state];
				for (auto input : seq) {
					auto rqIt = rq->next.find(input);
					if (rqIt == rq->next.end()) {
						rq = nullptr;
						break;
					}
					rq = rqIt->second;
				}
				if (rq) {
					processRelatedTransitions(rq, node->state, seq, li);
				}
			}
		}
	}

	static bool makeStateNode(shared_ptr<OTreeNode> node, SPYLearningInfo& li, const unique_ptr<Teacher>& teacher);

	static bool checkIdentified(SPYLearningInfo& li, const unique_ptr<Teacher>& teacher) {
		if (!li.identifiedNodes.empty()) {
			if (li.identifiedNodes.front()->domain.empty()) {// new state
				makeStateNode(move(li.identifiedNodes.front()), li, teacher);
				return false;
			} else if (li.inconsistency == SPYLearningInfo::NO_INCONSISTENCY) {
				processIdentified(li);
			} else {
				li.identifiedNodes.clear();
			}
		}
		else if (li.numberOfExtraStates > 0) {
			processConfirmedTransitions(li);
		}
		return (li.inconsistency == SPYLearningInfo::NO_INCONSISTENCY);
	}

	static bool moveAndCheck(shared_ptr<OTreeNode>& currNode, input_t input, SPYLearningInfo& li, const unique_ptr<Teacher>& teacher) {
		if (currNode->next[input]) {
			currNode = currNode->next[input];
			return true;
		}
		query(currNode, input, li, teacher);
		moveWithCN(currNode, input, li);
		if (li.numberOfExtraStates > 0) checkRQofPrevious(currNode, li);
		checkPrevious(currNode, li);
		return checkIdentified(li, teacher);
	}

	static bool queryRequiredSequence(shared_ptr<OTreeNode>& currNode, shared_ptr<requested_query_node_t> rq,
		sequence_in_t seq, SPYLearningInfo& li, const unique_ptr<Teacher>& teacher) {
		auto lastBranch = rq;
		auto lastBranchInput = seq.empty() ? rq->next.begin()->first : seq.front();
		for (auto nextInput : seq) {
			if (rq->next.size() > 1) {
				lastBranch = rq;
				lastBranchInput = nextInput;
			}
			rq = rq->next.at(nextInput);
			if (!moveAndCheck(currNode, nextInput, li, teacher)) {
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
			if (!moveAndCheck(currNode, nextInput, li, teacher)) {
				return true;
			}
		}
		lastBranch->next.erase(lastBranchInput);// no harm even if it is not used (after merging)
		return false;
	}

	static bool areDistinguished(const list<list<shared_ptr<OTreeNode>>>& nodes) {
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

	static void chooseADS(const shared_ptr<ads_cv_t>& ads, const SPYLearningInfo& li, frac_t& bestVal, frac_t& currVal,
		seq_len_t& totalLen, seq_len_t currLength = 1, state_t undistinguishedStates = 0, double prob = 1) {
		auto numStates = state_t(ads->nodes.size()) + undistinguishedStates;
		frac_t localBest(1);
		seq_len_t subtreeLen, minLen = seq_len_t(-1);
		for (input_t i = 0; i < li.conjecture->getNumberOfInputs(); i++) {
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
								it->second->nodes.emplace_back(list<shared_ptr<OTreeNode>>({ node->next[i] }));
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
				else if (li.conjecture->isOutputState()) {
					for (auto& cn : p.second->nodes) {
						auto it = p.second->next.find(cn.front()->stateOutput);
						if (it == p.second->next.end()) {
							it = p.second->next.emplace(cn.front()->stateOutput, make_shared<ads_cv_t>()).first;
						}
						it->second->nodes.emplace_back(list<shared_ptr<OTreeNode>>(cn));
					}
					for (auto& sp : p.second->next) {
						if ((sp.second->nodes.size() == 1) || (!areDistinguished(sp.second->nodes))) {
							adsVal += frac_t(sp.second->nodes.size() + undistinguishedStates - 1) /
								(prob * next.size() * p.second->next.size() * (numStates - 1));
							subtreeLen += (currLength * (sp.second->nodes.size() + undistinguishedStates));
						}
						else {
							chooseADS(sp.second, li, bestVal, adsVal, subtreeLen, currLength + 1,
								undistinguishedStates, prob * next.size() * p.second->next.size());
						}
					}
				}
				else {
					chooseADS(p.second, li, bestVal, adsVal, subtreeLen, currLength + 1, undistinguishedStates, prob * next.size());
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

	static bool identifyByADS(shared_ptr<OTreeNode>& currNode, shared_ptr<ads_cv_t> ads,
		SPYLearningInfo& li, const unique_ptr<Teacher>& teacher) {
		while (ads->input != STOUT_INPUT) {
			if (!moveAndCheck(currNode, ads->input, li, teacher)) {
				return false;
			}
			auto it = ads->next.find(currNode->incomingOutput);
			if (it != ads->next.end()) {
				ads = it->second;
				if (li.conjecture->isOutputState() && (ads->input == STOUT_INPUT)) {
					it = ads->next.find(currNode->stateOutput);
					if (it != ads->next.end()) {
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
		return true;
	}

	static bool identifyNextState(shared_ptr<OTreeNode>& currNode, SPYLearningInfo& li, const unique_ptr<Teacher>& teacher) {
		if (!moveAndCheck(currNode, li.testedInput, li, teacher)) {
			return true;
		}
		if (currNode->state != NULL_STATE) {// identified
			// optimization - apply the same input again
			if (!moveAndCheck(currNode, li.testedInput, li, teacher)) {
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
				(*(++(cn->domain.begin())))->convergent.front()->state);
			auto& sepSeq = li.separatingSequences[idx];
			for (auto input : sepSeq) {
				if (!moveAndCheck(currNode, input, li, teacher)) {
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
			chooseADS(ads, li, bestVal, currVal, totalLen);
			if (!identifyByADS(currNode, ads, li, teacher)) {
				return true;
			}
		}
		return false;
	}

	static bool querySequenceAndCheck(shared_ptr<OTreeNode> currNode, const sequence_in_t& seq,
		SPYLearningInfo& li, const unique_ptr<Teacher>& teacher, bool allowIdentification = true) {
		long long suffixAdded = 0;
		for (auto& input : seq) {
			if (input == STOUT_INPUT) continue;
			if (!currNode->next[input]) {
				suffixAdded--;
				query(currNode, input, li, teacher);
				moveWithCN(currNode, input, li);
			}
			else {
				currNode = currNode->next[input];
			}
		}
		checkPrevious(move(currNode), li, suffixAdded);
		return (allowIdentification ? checkIdentified(li, teacher) :
			(li.inconsistency == SPYLearningInfo::NO_INCONSISTENCY));
	}

	static bool moveNewStateNode(shared_ptr<OTreeNode>& node, SPYLearningInfo& li, const unique_ptr<Teacher>& teacher) {
		li.identifiedNodes.clear();
		auto parent = node->parent.lock();
		auto state = ((parent->state == NULL_STATE) || !parent->domain.count(parent->state)) ? NULL_STATE : parent->state;
		bool stateNotKnown = (state == NULL_STATE);
		auto& input = node->accessSequence.back();
		do {
			if (stateNotKnown) state = *(parent->domain.begin());
			auto newSN = li.ot.rn[state]->convergent.front();
			if (!newSN->next[input]) {
				querySequenceAndCheck(newSN, sequence_in_t({ input }), li, teacher, false);
			}
			newSN = newSN->next[input];
			while (!newSN->domain.empty() && parent->domain.count(state)) {
				auto newSNstate = *(newSN->domain.begin());
				sequence_in_t sepSeq;
				if ((node->state == NULL_STATE) || (node->state == newSNstate) ||
					!isSeparatingSequenceQueried(node, newSNstate, sepSeq, li)) {
					sepSeq = getQueriedSeparatingSequence(node, li.ot.rn[newSNstate]->convergent.front());
				}
				querySequenceAndCheck(newSN, sepSeq, li, teacher, false);
			}
			if (newSN->domain.empty()) {
				node = newSN;
				break;
			}
			else if (parent->domain.empty()) {
				node = parent;
				break;
			}
			else if (!stateNotKnown) {// parent is inconsistent
				storeInconsistentNode(parent, li, SPYLearningInfo::INCONSISTENT_DOMAIN);
				return false;
			}
		} while (!parent->domain.empty());
		return true;
	}

	static bool updateAndInitCN(const shared_ptr<OTreeNode>& node, SPYLearningInfo& li, const unique_ptr<Teacher>& teacher) {
		// update OTree with new state and fill li.identifiedNodes with nodes with |domain|<= 1
		li.identifiedNodes.clear();
		updateOTreeWithNewState(node, li);

		if (!li.identifiedNodes.empty() && (li.identifiedNodes.front()->domain.empty())) {// new state
			return makeStateNode(move(li.identifiedNodes.front()), li, teacher);
		}
		li.numberOfExtraStates = 0;
		li.testedState = 0;
		generateRequestedQueries(li);

		li.inconsistency = SPYLearningInfo::NO_INCONSISTENCY;
		li.inconsistentSequence.clear();
		li.inconsistentNodes.clear();
#if CHECK_PREDECESSORS
		if (!reduceDomainsBySuccessors(li)) {
			return false;
		}
#endif
		return processIdentified(li);
	}

	static bool makeStateNode(shared_ptr<OTreeNode> node, SPYLearningInfo& li, const unique_ptr<Teacher>& teacher) {
		auto parent = node->parent.lock();
		if (parent->observationStatus != OTreeNode::QUERIED_SN) {
			if (parent->domain.empty()) {
				return makeStateNode(parent, li, teacher);
			}
			if (!moveNewStateNode(node, li, teacher)) {
				if (!li.identifiedNodes.empty() && (li.identifiedNodes.front()->domain.empty())) {// new state
					return makeStateNode(move(li.identifiedNodes.front()), li, teacher);
				}
				return false;// inconsistency
			}
			if (node == parent) {
				return makeStateNode(parent, li, teacher);
			}
			parent = node->parent.lock();
		}
		if (node->state == WRONG_STATE) node->state = NULL_STATE;
		sequence_set_t hsi;
		// update hsi
		for (state_t state = 0; state < li.ot.rn.size(); state++) {
			sequence_in_t sepSeq;
			if ((node->state == NULL_STATE) || (node->state == state) ||
				!isSeparatingSequenceQueried(node, state, sepSeq, li)) {
				sepSeq = getQueriedSeparatingSequence(node, li.ot.rn[state]->convergent.front());
				if (!isPrefix(sepSeq, li.stateIdentifier[state])) {
					removePrefixes(sepSeq, li.stateIdentifier[state]);
					li.stateIdentifier[state].emplace(sepSeq);
				}
			}// else sepSeq is set to a separating prefix of the sep. seq. of (state,node->assumedState), see isSeparatingSequenceQueried
			if (!isPrefix(sepSeq, hsi)) {
				removePrefixes(sepSeq, hsi);
				hsi.emplace(sepSeq);
			}
			li.separatingSequences.emplace_back(move(sepSeq));
		}
		li.stateIdentifier.emplace_back(move(hsi));

		node->observationStatus = OTreeNode::QUERIED_SN;
		node->state = li.conjecture->addState(node->stateOutput);
		//li.ot.rn.emplace_back(make_shared<ConvergentNode>(node, true));
		li.requestedQueries.emplace_back(make_shared<requested_query_node_t>());
		auto cn = node->convergentNode.lock();
		li.numberOfExtraStates = 0;
		if (!cn || cn->isRN) {
			li.ot.rn.emplace_back(make_shared<ConvergentNode>(node, true));
			separateConvergentSubtree(li.ot.rn.back(), node, li);
		}
		else {
			cn->isRN = true;
			cn->state = node->state;
			if (cn->convergent.front() != node) {
				cn->convergent.remove(node);
				cn->convergent.emplace_front(node);
			}
			li.ot.rn.emplace_back(cn);
		}
		//generateConvergentSubtree(li.ot.rn.back(), li, cn);
		li.ot.rn[parent->state]->next[node->accessSequence.back()] = li.ot.rn.back();
		
		return updateAndInitCN(node, li, teacher);
	}

	static shared_ptr<OTreeNode> queryIfNotQueried(shared_ptr<OTreeNode> node, sequence_in_t seq, 
			SPYLearningInfo& li, const unique_ptr<Teacher>& teacher) {
		while (!seq.empty()) {
			if (!node->next[seq.front()]) {
				auto numStates = li.ot.rn.size();
				querySequenceAndCheck(node, seq, li, teacher);
				if (numStates != li.ot.rn.size() || (li.inconsistency == SPYLearningInfo::NO_INCONSISTENCY)) {
					// new state found or OTree node with inconsistent domain observed
					return nullptr;
				}
				return li.bbNode;
			}
			node = node->next[seq.front()];
			seq.pop_front();
		}
		return move(node);
	}

	static bool eliminateSeparatedStatesFromDomain(const shared_ptr<OTreeNode>& node,
		SPYLearningInfo& li, const unique_ptr<Teacher>& teacher) {
		auto cn = node->convergentNode.lock();
		auto domain = node->domain;
		for (auto state : domain) {
			if (node->domain.count(state)) {
				cn_pair_set_t closed;
				auto sepSeq = getQueriedSeparatingSequenceOfCN(cn, li.ot.rn[state], closed);
				if (sepSeq.empty()) {
					//throw;// should not happen
					continue;
				}
#if CHECK_PREDECESSORS
				if (sepSeq.front() == STOUT_INPUT) {
					do {
						sepSeq.pop_front();// STOUT
						auto retVal = proveSepSeqOfEmptyDomainIntersection(li.ot.rn[state]->convergent.front(), cn, sepSeq, closed, li, teacher);
						if (retVal == 1) return true;// new state or inconsistent domain found
						if (retVal == 2) break;// seq updated to contain a separating sequence
						sepSeq = getQueriedSeparatingSequenceOfCN(cn, li.ot.rn[state], closed);
					} while (!sepSeq.empty());
					if (sepSeq.empty()) {
						//throw;// should not happen
						continue;
					}
				}
#endif
				if (!queryIfNotQueried(node, sepSeq, li, teacher)) {
					return true;
				}
				if (!queryIfNotQueried(li.ot.rn[state]->convergent.front(), move(sepSeq), li, teacher)) {
					return true;
				}
			}
		}
		cn->state = WRONG_STATE;
		return false;
	}

	static void resolveEmptyDomain(shared_ptr<OTreeNode>& inconsNode, SPYLearningInfo& li, const unique_ptr<Teacher>& teacher) {
		auto inconsPredecessor = inconsNode->parent.lock();
		sequence_in_t sepSeq;
		sepSeq.emplace_back(inconsNode->accessSequence.back());
		while (inconsPredecessor->state == NULL_STATE) {
			sepSeq.emplace_front(inconsPredecessor->accessSequence.back());
			inconsPredecessor = inconsPredecessor->parent.lock();
		}
		if (!inconsPredecessor->domain.count(inconsPredecessor->state)) {
			return;
		}
		if (inconsPredecessor->observationStatus != OTreeNode::QUERIED_SN) {
			inconsPredecessor = li.ot.rn[inconsPredecessor->state]->convergent.front();
			inconsNode = queryIfNotQueried(inconsPredecessor, move(sepSeq), li, teacher);
			if (!inconsNode) {
				return;
			}
		}
		if (eliminateSeparatedStatesFromDomain(inconsNode, li, teacher)) {
			return;
		}
		return;
	}

	static void resolveInconsistentDomain(shared_ptr<OTreeNode>& inconsNode, SPYLearningInfo& li, const unique_ptr<Teacher>& teacher) {
		sequence_in_t sepSeq;
		sepSeq.emplace_back(inconsNode->accessSequence.back());
		auto inconsPredecessor = inconsNode->parent.lock();
		while (inconsPredecessor->state == NULL_STATE) {
			sepSeq.emplace_front(inconsPredecessor->accessSequence.back());
			inconsPredecessor = inconsPredecessor->parent.lock();
		}
		if (!inconsPredecessor->domain.count(inconsPredecessor->state)) {
			// inconsPredecessor is inconsistent
			return;
		}
		if (inconsPredecessor->observationStatus == OTreeNode::QUERIED_SN) {
			if (inconsNode->state == WRONG_STATE) {
				// updateAndInitCN(nullptr, li, teacher); 
				// forces to update CN layer from the Slearner body function
				li.inconsistentNodes.clear();
				inconsNode->state = NULL_STATE;
				//storeInconsistentNode(inconsNode, li, SPYLearningInfo::EMPTY_CN_DOMAIN);
				return;
			}
			inconsNode->state = NULL_STATE;
			/*if (eliminateSeparatedStatesFromDomain(inconsNode, li, teacher)) {
			return;
			}
			return;*/
			auto inconsPredecessorCN = inconsPredecessor->convergentNode.lock();
			auto transferSeq = move(sepSeq);
			auto domain = inconsNode->domain;
			for (auto state : domain) {
				if (!inconsNode->domain.count(state)) continue;
				sepSeq.clear();
				sequence_in_t seqToDistDomains;
				cn_pair_set_t closed;
				for (auto n : inconsPredecessorCN->convergent) {
					for (auto& input : transferSeq) {
						n = n->next[input];
						if (!n) break;
					}
					if (n) {
						sepSeq = getQueriedSeparatingSequenceFromSN(n, li.ot.rn[state], closed);
						if (!sepSeq.empty()) {
#if CHECK_PREDECESSORS
							if (sepSeq.front() == STOUT_INPUT) {
								if (seqToDistDomains.empty() || (seqToDistDomains.size() > sepSeq.size())) {
									seqToDistDomains = move(sepSeq);
								}
								else {
									sepSeq.clear();
								}
							}
							else
#endif
								break;
						}
					}
				}
#if CHECK_PREDECESSORS
				if (sepSeq.empty() && !seqToDistDomains.empty()) {
					sepSeq = move(seqToDistDomains);
					do {
						sepSeq.pop_front();// STOUT
						auto retVal = proveSepSeqOfEmptyDomainIntersection(li.ot.rn[state]->convergent.front(),
							inconsNode->convergentNode.lock(), sepSeq, closed, li, teacher);
						if (retVal == 1) return;// new state or inconsistent domain found
						if (retVal == 2) break;// seq updated to contain a separating sequence
						sepSeq = getQueriedSeparatingSequenceOfCN(inconsNode->convergentNode.lock(), li.ot.rn[state], closed);
					} while (!sepSeq.empty());
					if (sepSeq.empty()) {
						throw;// should not happen
						continue;
					}
				}
#endif
				if (!sepSeq.empty()) {
					if (!queryIfNotQueried(li.ot.rn[state]->convergent.front(), sepSeq, li, teacher)) {
						return;
					}
					if (!queryIfNotQueried(inconsNode, move(sepSeq), li, teacher)) {
						return;
					}
				}
			}

		}
		else {
			auto parentState = inconsPredecessor->state;
			//inconsPredecessor->state = NULL_STATE;
			if (inconsNode->state != WRONG_STATE) {
				sequence_in_t sepSuffix;
				sepSuffix = getQueriedSeparatingSequence(inconsNode, li.ot.rn[inconsNode->state]->convergent.front());
				/*
				if (sepSuffix.empty() || (sepSuffix.front() == STOUT_INPUT)) {
					throw;
					return;
				}*/
				sepSeq.splice(sepSeq.end(), move(sepSuffix));
			}
			inconsNode->state = NULL_STATE;
			if (!queryIfNotQueried(li.ot.rn[parentState]->convergent.front(), sepSeq, li, teacher)) {
				return;
			}
			//storeInconsistentNode(inconsNode, li);
			return;
		}
	}

	static void processInconsistent(SPYLearningInfo& li, const unique_ptr<Teacher>& teacher) {
		auto inconsNode = move(li.inconsistentNodes.front());
		li.inconsistentNodes.pop_front();
		if ((inconsNode->state != NULL_STATE) && (!inconsNode->domain.count(inconsNode->state))) {
			//li.inconsistency = SPYLearningInfo::INCONSISTENT_DOMAIN;
			resolveInconsistentDomain(inconsNode, li, teacher);
		}
		else {
			auto cn1 = inconsNode->convergentNode.lock();
			if (cn1->domain.empty()) {
				resolveEmptyDomain(inconsNode, li, teacher);
			}
			else {// unsuccessfully merged CNs
				if (cn1->convergent.front()->accessSequence.size() < inconsNode->accessSequence.size()) {
					inconsNode = cn1->convergent.front();
				}
				if (!queryIfNotQueried(inconsNode, li.inconsistentSequence, li, teacher)) {
					return;
				}
				if (!cn1->domain.empty()) {
					if (!queryIfNotQueried((*(cn1->domain.begin()))->convergent.front(), li.inconsistentSequence, li, teacher)) {
						return;
					}
					if (li.inconsistentNodes.empty() && !cn1->domain.empty()) {
						auto cn = li.ot.rn[(*(cn1->domain.begin()))->state];
						auto it = li.inconsistentSequence.begin();
						while ((cn != cn1) && (it != li.inconsistentSequence.end())) {
							cn = cn->next[*it];
							++it;
						}
						if (cn == cn1) {
							auto bIt = li.inconsistentSequence.begin();
							auto eIt = it;
							do {
								while ((bIt != eIt) && (it != li.inconsistentSequence.end()) && (*bIt == *it)) {
									++bIt;
									++it;
								}
								if (bIt == eIt) {
									eIt = it;
								}
							} while ((it != li.inconsistentSequence.end()) && (*bIt == *it));
							sequence_in_t sepSeq(eIt, li.inconsistentSequence.end());
							if (!queryIfNotQueried((*(cn1->domain.begin()))->convergent.front(), move(sepSeq), li, teacher)) {
								return;
							}
						}
						if (li.inconsistentNodes.empty()) {
							if (li.testedState != NULL_STATE) {
								while (li.inconsistentSequence.size() > li.testedInput) li.inconsistentSequence.pop_front();
								if (!queryIfNotQueried(li.ot.rn[li.testedState]->convergent.front(), li.inconsistentSequence, li, teacher)) {
									return;
								}
								li.testedState = NULL_STATE;
							}
						}
					}
				}
				return;
			}
		}
	}

	static shared_ptr<ads_cv_t> getADSwithFixedPrefix(shared_ptr<OTreeNode> node, SPYLearningInfo& li) {
		auto ads = make_shared<ads_cv_t>();
		auto cn = node->convergentNode.lock();
		for (auto& sn : cn->domain) {
			ads->nodes.push_back(sn->convergent);
		}
		auto currAds = ads;
		while (node->lastQueriedInput != STOUT_INPUT) {
			currAds->input = node->lastQueriedInput;
			node = node->next[node->lastQueriedInput];
			auto nextADS = make_shared<ads_cv_t>();
			for (auto& currCN : currAds->nodes) {
				list<shared_ptr<OTreeNode>> nextNodes;
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
		chooseADS(currAds, li, bestVal, currVal, totalLen);
		return ads;
	}

	static seq_len_t chooseTransitionToVerify(SPYLearningInfo& li) {
		seq_len_t minLen = seq_len_t(-1);
		const auto numInputs = li.conjecture->getNumberOfInputs();
		auto reqIt = li.queriesFromNextState.begin();
		size_t tranId = 0;
		for (state_t state = 0; state < li.ot.rn.size(); state++) {
			const auto& sn = li.ot.rn[state];
			const auto& rq = li.requestedQueries[state];
			const auto& refSN = sn->convergent.front();
			for (input_t input = 0; input < numInputs; input++, tranId++) {
				seq_len_t len = minLen;
				auto inIt = rq->next.find(input);
				bool isRq = ((inIt != rq->next.end()) && (inIt->second->seqCount > 0));
				if ((reqIt != li.queriesFromNextState.end()) && (tranId == reqIt->first)) {
					if (reqIt->second.empty()) {
						reqIt = li.queriesFromNextState.erase(reqIt);
						if (isRq) {
							len = inIt->second->seqCount * refSN->accessSequence.size();
						}
					}
					else {
						auto nextState = li.conjecture->getNextState(refSN->state, input);
						len = reqIt->second.size() * li.ot.rn[nextState]->convergent.front()->accessSequence.size();
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
					li.testedState = state;
					li.testedInput = input;
				}
			}
		}
		return minLen;
	}

	static bool tryExtendQueriedPath(shared_ptr<OTreeNode>& node, SPYLearningInfo& li, const unique_ptr<Teacher>& teacher) {
		node = li.bbNode;// should be the same
		sequence_in_t seq;
		while ((node->state == NULL_STATE) || (node->observationStatus == OTreeNode::QUERIED_NOT_IN_RN)) {// find the lowest state node
			seq.emplace_front(node->accessSequence.back());
			node = node->parent.lock();
		}
		li.testedInput = STOUT_INPUT;
		auto cn = li.ot.rn[node->state];
		if (seq.size() > 0) {
			if (li.numberOfExtraStates == 0) {
				auto inIt = li.requestedQueries[node->state]->next.find(seq.front());
				if (inIt != li.requestedQueries[node->state]->next.end()) {
					auto ads = getADSwithFixedPrefix(node->next[seq.front()], li);
					if (ads) {
						li.testedState = node->state;
						li.testedInput = seq.front();
						node = node->next[seq.front()];
						identifyByADS(node, ads, li, teacher);
						return true;// next time tryExtend is called again
					}
				}
				// find unidentified transition closest to the root
				seq_len_t minLen(li.ot.rn.size());
				for (const auto& sn : li.ot.rn) {
					if ((li.requestedQueries[sn->state]->seqCount > 0) && (minLen > sn->convergent.front()->accessSequence.size())) {
						node = sn->convergent.front();
						minLen = node->accessSequence.size();
						li.testedState = node->state;
						cn = sn;
					}
				}
			}
			else {
				do {
					auto rq = li.requestedQueries[node->state];
					for (auto input : seq) {
						auto rqIt = rq->next.find(input);
						if (rqIt == rq->next.end()) {
							rq = nullptr;
							break;
						}
						rq = rqIt->second;
					}
					if (rq) {
						li.testedState = node->state;
						li.testedInput = seq.front();
						queryRequiredSequence(node, li.requestedQueries[node->state], move(seq), li, teacher);
						return true;// next time tryExtend is called again
					}
					if (!node->accessSequence.empty())
						seq.emplace_front(node->accessSequence.back());
					node = node->parent.lock();
				} while (node);
					
				chooseTransitionToVerify(li);
				node = li.ot.rn[li.testedState]->convergent.front();
			}
		}
		else if (!li.requestedQueries[node->state]->next.empty()) {//the state node is leaf with required outcoming sequences
			li.testedState = node->state;
		}
		else {//the state node is leaf = if (node->lastQueriedInput == STOUT_INPUT)
			// find unidentified transition closest to the root
			seq_len_t minLen(li.ot.rn.size());
			if (li.numberOfExtraStates == 0) {
				for (const auto& sn : li.ot.rn) {
					if ((li.requestedQueries[sn->state]->seqCount > 0) && (minLen > sn->convergent.front()->accessSequence.size())) {
						minLen = sn->convergent.front()->accessSequence.size();
						li.testedState = sn->state;
					}
				}
			}
			else {
				minLen = chooseTransitionToVerify(li);
			}
			// extend or reset?
			bool extend = false;
			if (minLen > 1) {
				// find closest unidentified transition from the node
				queue<pair<sequence_in_t, shared_ptr<ConvergentNode>>> fifo;
				seq.clear();// should be empty
				fifo.emplace(seq, move(cn));
				while (!fifo.empty()) {
					auto p = move(fifo.front());
					fifo.pop();
					for (input_t i = 0; i < p.second->next.size(); i++) {
						if (((li.numberOfExtraStates == 0) && p.second->next[i] && p.second->next[i]->isRN)
							|| ((li.numberOfExtraStates > 0) && (getNextStateIfVerified(p.second->state, i, li) != NULL_STATE))) {
							seq = p.first;
							seq.emplace_back(i);
							if (!li.requestedQueries[p.second->next[i]->state]->next.empty()) {
								if (!querySequenceAndCheck(node, seq, li, teacher)) {
									// inconsistency found
									return true;
								}
								node = li.bbNode;
								cn = node->convergentNode.lock();
								li.testedState = node->state;
								li.testedInput = STOUT_INPUT;
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
				cn = li.ot.rn[li.testedState];
				node = cn->convergent.front();
			}
		}
		if (li.testedInput == STOUT_INPUT) {
			auto& rq = li.requestedQueries[cn->state];
			if (rq->next.empty()) return true;
			if (li.numberOfExtraStates == 0) {
				if (!node->accessSequence.empty() && rq->next.count(node->accessSequence.back())) {
					li.testedInput = node->accessSequence.back();
				}
				else {
					li.testedInput = rq->next.begin()->first;
				}
			}
			else {
				for (const auto& inIt : rq->next) {
					if (inIt.second->seqCount > 0) {
						li.testedInput = inIt.first;
						break;
					}
				}
			}
		}
		return false;
	}
	
	static void checkCNdomains(SPYLearningInfo& li) {
		for (const auto& rn : li.ot.rn) {
			for (auto cnIt = rn->domain.begin(); cnIt != rn->domain.end();) {
				cn_pair_set_t closed;
				if (areConvergentNodesDistinguished(rn, (*cnIt)->convergent.front()->convergentNode.lock(), closed)) {
					(*cnIt)->domain.erase(rn.get());
					if ((*cnIt)->domain.empty()) {
						storeInconsistentNode((*cnIt)->convergent.front(), li, SPYLearningInfo::EMPTY_CN_DOMAIN);
						li.identifiedNodes.clear();
						return;
					}
					if ((*cnIt)->domain.size() == 1) {
						storeIdentifiedNode((*cnIt)->convergent.front(), li);
					}
#if CHECK_PREDECESSORS
					if (li.inconsistentNodes.empty()) {
						li.nodesWithChangedDomain.insert(*toIt);
					}
#endif	
					cnIt = rn->domain.erase(cnIt);
				}
				else {
					++cnIt;
				}
			}
		}
		if (li.inconsistency == SPYLearningInfo::NO_INCONSISTENCY) {
			processIdentified(li);
		}
		else {
			li.identifiedNodes.clear();
		}
	}

	unique_ptr<DFSM> SPYlearner(const unique_ptr<Teacher>& teacher, state_t maxExtraStates,
		function<bool(const unique_ptr<DFSM>& conjecture)> provideTentativeModel, bool isEQallowed) {
		if (!teacher->isBlackBoxResettable()) {
			ERROR_MESSAGE("FSMlearning::SPYlearner - the Black Box needs to be resettable");
			return nullptr;
		}

		/// Observation Tree
		SPYLearningInfo li;
		auto numInputs = teacher->getNumberOfInputs();
		li.conjecture = FSMmodel::createFSM(teacher->getBlackBoxModelType(), 1, numInputs, teacher->getNumberOfOutputs());
		teacher->resetBlackBox();
		auto node = make_shared<OTreeNode>(DEFAULT_OUTPUT, 0, numInputs); // root
		node->domain.emplace(0);
		node->observationStatus = OTreeNode::QUERIED_SN;
		if (li.conjecture->isOutputState()) {
			node->stateOutput = teacher->outputQuery(STOUT_INPUT);
			checkNumberOfOutputs(teacher, li.conjecture);
			li.conjecture->setOutput(0, node->stateOutput);
		}
#if DUMP_OQ
		li.observationTree = FSMmodel::createFSM(teacher->getBlackBoxModelType(), 1, numInputs, teacher->getNumberOfOutputs());
		if (li.observationTree->isOutputState()) {
			li.observationTree->setOutput(0, node->stateOutput);
		}
#endif
		li.bbNode = node;
		li.numberOfExtraStates = 0;
		auto cn = make_shared<ConvergentNode>(node, true);
		node->convergentNode = cn;
		li.requestedQueries.emplace_back(make_shared<requested_query_node_t>());
		li.ot.rn.emplace_back(cn);
		li.stateIdentifier.emplace_back(sequence_set_t());
		generateRequestedQueries(li);
		
		shared_ptr<OTreeNode> lastInconsistent;
		SPYLearningInfo::OTreeInconsistency lastInconsistency;
		sequence_in_t lastInconsistentSequence;
		li.testedState = 0;
		li.testedInput = 0;
		bool checkPossibleExtension = false;
		bool unlearned = true;
		while (unlearned) {
			if (!li.inconsistentNodes.empty()) {
				li.numberOfExtraStates = 0;
				auto numStates = li.ot.rn.size();
				processInconsistent(li, teacher);
				if (numStates == li.ot.rn.size()) {
					if (li.inconsistentNodes.empty()) {
						//throw;
						updateAndInitCN(nullptr, li, teacher);
						if (li.inconsistentNodes.empty() && (numStates == li.ot.rn.size())) {
							checkCNdomains(li);
						}
						if (li.inconsistentNodes.empty() && (numStates == li.ot.rn.size())) {
							throw;
						} else if (!li.inconsistentNodes.empty()) {
							if (lastInconsistent && (lastInconsistent == li.inconsistentNodes.back())
								&& (lastInconsistency == li.inconsistency) &&
								((li.inconsistency != SPYLearningInfo::WRONG_MERGE) || (lastInconsistentSequence == li.inconsistentSequence))) {
								throw;
							}
							lastInconsistent = li.inconsistentNodes.back();
							lastInconsistency = li.inconsistency;
							if (lastInconsistency == SPYLearningInfo::WRONG_MERGE) {
								lastInconsistentSequence = li.inconsistentSequence;
							}
						}
						else {
							lastInconsistent = nullptr;
						}
					}
				}
				else {
					lastInconsistent = nullptr;
				}
				//node = li.bbNode; // in tryExtend
			}
			else if (li.unconfirmedTransitions > 0) {
				if (checkPossibleExtension) {
					if (tryExtendQueriedPath(node, li, teacher)) {
						// inconsistency
						continue;
					}
				}
				else {
					node = li.ot.rn[li.testedState]->convergent.front();
				}
				if (li.numberOfExtraStates == 0) {
					checkPossibleExtension = (identifyNextState(node, li, teacher)
						|| (li.ot.rn[li.testedState]->next[li.testedInput]->isRN));
				} else {
					auto rq = li.requestedQueries[li.testedState];
					if (li.testedInput == STOUT_INPUT) {
						queryRequiredSequence(node, rq, sequence_in_t(), li, teacher);
					}
					else {
						auto rqIt = rq->next.find(li.testedInput);
						if ((rqIt != rq->next.end()) && (rqIt->second->seqCount > 0)) {
							queryRequiredSequence(node, rq, sequence_in_t({ li.testedInput }), li, teacher);
						}
						else {
							auto reqIt = li.queriesFromNextState.find(li.testedState * numInputs + li.testedInput);
							if ((reqIt != li.queriesFromNextState.end()) && (reqIt->second.empty())) {
								li.queriesFromNextState.erase(reqIt);
								reqIt = li.queriesFromNextState.end();
							}
							if (reqIt != li.queriesFromNextState.end()) {
								li.testedState = li.conjecture->getNextState(li.testedState, li.testedInput);
								node = li.ot.rn[li.testedState]->convergent.front();
								queryRequiredSequence(node, li.requestedQueries[li.testedState],
									reqIt->second.front(), li, teacher);
							}
						}
					}
				}
			}
			else {// increase the number of assumed ES
				checkPossibleExtension = true;

				li.numberOfExtraStates++;
				if (li.numberOfExtraStates > maxExtraStates) {
					if (isEQallowed) {
						auto ce = teacher->equivalenceQuery(li.conjecture);
						if (!ce.empty()) {
							li.numberOfExtraStates--;
							querySequenceAndCheck(li.ot.rn[0]->convergent.front(), ce, li, teacher);
							continue;
						}
					}
					unlearned = false;
				}
				else {
					//update
					generateRequestedQueries(li);
					// update RQ by subtree of state nodes
					/*li.testedState = 0;
					for (auto it = li.ot.rn.begin(); it != li.ot.rn.end(); ++it, li.testedState++) {
						if (!mergeConvergent(li.testedState, (*it)->convergent.front(), li, true)) {
							throw "";
						}
					}*/
					for (state_t state = 0; state < li.conjecture->getNumberOfStates(); state++) {
						sequence_in_t seq;
						updateRequestedQueries(li.requestedQueries[state], li.ot.rn[state]->convergent.front(), state, seq, li);
					}

					processConfirmedTransitions(li);
					for (auto it = li.queriesFromNextState.begin(); it != li.queriesFromNextState.end();) {
						if (it->second.empty()) {
							it = li.queriesFromNextState.erase(it);
						}
						else ++it;
					}
					li.testedState = 0;
				}
			}
			if (provideTentativeModel && unlearned) {
#if DUMP_OQ
				unlearned = provideTentativeModel(li.observationTree);
#else
				unlearned = provideTentativeModel(li.conjecture);
#endif // DUMP_OQ
			}
		}
		return move(li.conjecture);
	}
}
