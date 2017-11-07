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

#include "FSMlearning.h"

using namespace FSMtesting;

namespace FSMlearning {

#define DUMP_OQ 1

#define CHECK_PREDECESSORS 0

//#define NOT_QUERIED		NULL_STATE
//#define QUERIED_NOT_RN	0
//#define QUERIED_RN		1
	
	inline bool sCNcompare(const ConvergentNode* ls, const ConvergentNode* rs) {
		const auto& las = ls->convergent.front()->accessSequence;
		const auto& ras = rs->convergent.front()->accessSequence;
		if (las.size() != ras.size()) return las.size() < ras.size();
		return las < ras;
	}

	struct sCNcomp {
		bool operator() (const ConvergentNode* ls, const ConvergentNode* rs) const {
			return sCNcompare(ls, rs);
		}
	};

	typedef set<ConvergentNode*, sCNcomp> s_cn_set_t;

	struct LearningInfo {
		OTree ot;
		StateCharacterization sc;
		shared_ptr<OTreeNode> bbNode;

		state_t numberOfExtraStates;
		unique_ptr<DFSM> conjecture;

		map<state_t,set<input_t>> unconfirmedTransitions;

		list<shared_ptr<OTreeNode>> identifiedNodes;
		list<shared_ptr<OTreeNode>> inconsistentNodes;
		sequence_in_t inconsistentSequence;
		sequence_set_t requestedQueries;

		enum OTreeInconsistency { NO_INCONSISTENCY, WRONG_MERGE, EMPTY_CN_DOMAIN, INCONSISTENT_DOMAIN, NEW_STATE_REVEALED }
			inconsistency = NO_INCONSISTENCY;

		state_t testedState;
		input_t testedInput;
		vector<sequence_in_t> separatingSequences;
#if CHECK_PREDECESSORS
		s_cn_set_t nodesWithChangedDomain;// compare them by the length of access seq
#endif
#if DUMP_OQ
		unique_ptr<DFSM> observationTree;
#endif
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

	struct s_ads_cv_t {
		list<list<shared_ptr<OTreeNode>>> nodes;
		input_t input;
		map<output_t, shared_ptr<s_ads_cv_t>> next;

		s_ads_cv_t() : input(STOUT_INPUT) {}
		s_ads_cv_t(const shared_ptr<OTreeNode>& node) : nodes({ list<shared_ptr<OTreeNode>>({ node }) }), input(STOUT_INPUT) {}
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

	static void storeInconsistentNode(const shared_ptr<OTreeNode>& node, LearningInfo& li, LearningInfo::OTreeInconsistency inconsistencyType) {
		if (find(li.inconsistentNodes.begin(), li.inconsistentNodes.end(), node) != li.inconsistentNodes.end()) return;
		if (inconsistencyType == LearningInfo::INCONSISTENT_DOMAIN) {
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
	
	static void storeInconsistentNode(const shared_ptr<OTreeNode>& node, const shared_ptr<OTreeNode>& diffNode, 
			LearningInfo& li, LearningInfo::OTreeInconsistency inconsistencyType) {
		if (inconsistencyType == LearningInfo::INCONSISTENT_DOMAIN) {
			li.inconsistency = inconsistencyType;
			li.inconsistentNodes.emplace_front(diffNode);
			li.inconsistentNodes.emplace_front(node);
		}
		else {
			if (li.inconsistentNodes.empty()) {
				li.inconsistency = inconsistencyType;
			}
			li.inconsistentNodes.emplace_back(node);
			li.inconsistentNodes.emplace_back(diffNode);
		}
		
	}

	static void storeIdentifiedNode(const shared_ptr<OTreeNode>& node, LearningInfo& li) {
		if (li.identifiedNodes.empty()) {
			li.identifiedNodes.emplace_back(node);
		}
		else if (node->domain.empty()) {
			if (!li.identifiedNodes.front()->domain.empty() ||
				(li.identifiedNodes.front()->accessSequence.size() > node->accessSequence.size())) {
				li.identifiedNodes.clear();
				li.identifiedNodes.emplace_back(node);
			}
		}
		else if (!li.identifiedNodes.front()->domain.empty()) {
			if (node->accessSequence.size() < li.identifiedNodes.front()->accessSequence.size()) {
				li.identifiedNodes.emplace_front(node);
			}
			else {
				li.identifiedNodes.emplace_back(node);
			}
		}
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

	static bool areNodesDifferent(const shared_ptr<OTreeNode>& n1, const shared_ptr<OTreeNode>& n2) {
		if (n1->stateOutput != n2->stateOutput) return true;
		for (input_t i = 0; i < n1->next.size(); i++) {
			if ((n1->next[i]) && (n2->next[i]) && ((n1->next[i]->incomingOutput != n2->next[i]->incomingOutput)
				|| areNodesDifferent(n1->next[i], n2->next[i])))
				return true;
		}
		return false;
	}

	static bool areNodesDifferentUnder(const shared_ptr<OTreeNode>& n1, const shared_ptr<OTreeNode>& n2, long long len) {
		if ((n1->stateOutput != n2->stateOutput)) return true;
		if ((n1->lastQueriedInput == STOUT_INPUT) || (static_cast<long long>(n2->maxSuffixLen) < len)) return false;
		auto& idx = n1->lastQueriedInput;
		return (n2->next[idx] && (n2->next[idx]->observationStatus != OTreeNode::NOT_QUERIED)
			&& ((n1->next[idx]->incomingOutput != n2->next[idx]->incomingOutput)
			|| areNodesDifferentUnder(n1->next[idx], n2->next[idx], len - 1)));
	}
	
	static bool areNodeAndConvergentDifferentUnder(const shared_ptr<OTreeNode>& node, ConvergentNode* cn) {
		if (node->stateOutput != cn->convergent.front()->stateOutput) return true;
#if CHECK_PREDECESSORS
		auto& cn1 = node->convergentNode.lock();
		if (cn->isRN || cn1->isRN)  {
			if (cn->isRN && cn1->isRN) {
				if (cn1.get() != cn) return true;
			}
			else if (!cn1->domain.count(cn)) return true;
		}
		else if (isIntersectionEmpty(cn1->domain, cn->domain)) return true;
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
			}
			else {
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

	static bool isSeparatingSequenceQueried(shared_ptr<OTreeNode> node, state_t state, sequence_in_t& sepSeq, LearningInfo& li) {
		auto otherNode = li.ot.rn[state]->convergent.front();
		if (node->stateOutput != otherNode->stateOutput) return true;
		return false;// TODO
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
		closed.emplace(n1->convergentNode.lock().get(), cn2.get());
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

	static bool query(const shared_ptr<OTreeNode>& node, input_t input, LearningInfo& li, const unique_ptr<Teacher>& teacher) {
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
		if (li.conjecture->getNumberOfInputs() != teacher->getNumberOfInputs()) {
			// update all requested spyOTree
			li.conjecture->incNumberOfInputs(teacher->getNumberOfInputs() - li.conjecture->getNumberOfInputs());
		}
		auto leaf = node->next[input] ? node->next[input] :
			make_shared<OTreeNode>(node, input, transitionOutput, stateOutput,
			li.ot.es > 0 ? li.conjecture->getNextState(node->state, input) : NULL_STATE, li.conjecture->getNumberOfInputs());// teacher->getNextPossibleInputs());
		bool isConsistent = true;
		leaf->observationStatus = OTreeNode::QUERIED_NOT_RN;
		if (node->next[input]) {// verification - ES > 0
			if ((transitionOutput != leaf->incomingOutput) || (stateOutput != leaf->stateOutput)) {
				leaf->incomingOutput = transitionOutput;
				leaf->stateOutput = stateOutput;
				leaf->state = WRONG_STATE;
				isConsistent = false;
			}
		}
		else {
			node->next[input] = leaf;
		}
		li.bbNode = leaf;
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
		return isConsistent;
	}

	static bool makeStateNode(shared_ptr<OTreeNode> node, LearningInfo& li, const unique_ptr<Teacher>& teacher);

#if CHECK_PREDECESSORS
	static bool checkPredecessors(shared_ptr<OTreeNode> node, shared_ptr<ConvergentNode> parentCN, LearningInfo& li) {
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
							storeInconsistentNode((*it)->convergent.front(), li, LearningInfo::EMPTY_CN_DOMAIN);
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
					storeInconsistentNode(parentCN->convergent.front(), li, LearningInfo::EMPTY_CN_DOMAIN);
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

	static bool processChangedNodes(LearningInfo& li) {
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

	static bool reduceDomainsBySuccessors(LearningInfo& li) {
		for (const auto& cn : li.ot.rn) {
			for (auto it = cn->domain.begin(); it != cn->domain.end();) {
				cn_pair_set_t closed;
				if (areConvergentNodesDistinguished(cn, (*it)->convergent.front()->convergentNode.lock(), closed)) {
					(*it)->domain.erase(cn.get());
					if ((*it)->domain.empty()) {
						storeInconsistentNode((*it)->convergent.front(), li, LearningInfo::EMPTY_CN_DOMAIN);
						it = cn->domain.erase(it);
						li.nodesWithChangedDomain.clear();
						return false;
					}
					if ((*it)->domain.size() == 1) {
						storeIdentifiedNode((*it)->convergent.front(), li);
					}
					li.nodesWithChangedDomain.insert(*it);
					it = cn->domain.erase(it);
				}
				else {
					++it;
				}
			}
		}
		return processChangedNodes(li);
	}
#endif

	static bool updateUnconfirmedTransitions(const shared_ptr<ConvergentNode>& toCN, input_t i, LearningInfo& li,
			shared_ptr<OTreeNode> node = nullptr) {
		auto it = li.unconfirmedTransitions.find(toCN->state);
		if ((it != li.unconfirmedTransitions.end()) && (it->second.count(i))) {
			if (!node) {
				auto nIt = toCN->convergent.begin();
				while (!(*nIt)->next[i] || ((*nIt)->next[i]->state == NULL_STATE)) ++nIt;
				node = (*nIt)->next[i];
			}
			li.conjecture->setTransition(toCN->state, i, node->state,
				(li.conjecture->isOutputTransition() ? node->incomingOutput : DEFAULT_OUTPUT));
			it->second.erase(i);
			if (it->second.empty()) li.unconfirmedTransitions.erase(it); 
			return true;
		}
		return false;
	}

	static bool mergeConvergentNoES(shared_ptr<ConvergentNode>& fromCN, const shared_ptr<ConvergentNode>& toCN, LearningInfo& li) {
		if (toCN->isRN) {// a state node
			if (li.inconsistency == LearningInfo::NO_INCONSISTENCY) {
				if ((fromCN->domain.find(toCN.get()) == fromCN->domain.end())) {// || areConvergentNodesDistinguished(fromCN, toCN, cn_pair_set_t())) {
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
							//throw "";
						} else if (li.inconsistentSequence.front() == STOUT_INPUT) {
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
							//n->observationStatus = WRONG_STATE;
							//toCN->convergent.remove(n);
							storeInconsistentNode((*toIt)->convergent.front(), li, LearningInfo::EMPTY_CN_DOMAIN);
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
				}
			}
			for (auto& consistent : fromCN->domain) {
				consistent->domain.erase(fromCN.get());
			}
			//fromCN->domain.clear();
			for (auto& node : fromCN->convergent) {
				//node->observationStatus = 
				node->state = toCN->state;
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
			/*
			auto toIt = toCN->domain.begin();
			auto fromIt = fromCN->domain.begin();
			while ((toIt != toCN->domain.end()) && (fromIt != fromCN->domain.end())) {
				if (sCNcompare(*toIt, *fromIt)) {
					(*toIt)->domain.erase(toCN.get());
					toIt = toCN->domain.erase(toIt);
					reduced = true;
				}
				else {
					if (!(sCNcompare(*fromIt, *toIt))) {
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
			}*/
			if (li.inconsistency == LearningInfo::NO_INCONSISTENCY) {
				if (toCN->domain.empty()) {
					auto& n = toCN->convergent.front();
					//n->convergentNode = fromCN;
					//toCN->convergent.remove(n);
					//li.inconsistentNodes.clear();
					storeInconsistentNode(n, li, LearningInfo::EMPTY_CN_DOMAIN);
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
							updateUnconfirmedTransitions(toCN, i, li);
						}
					}
					else {
						if (!mergeConvergentNoES(tmpCN->next[i], toCN->next[i], li)) {
							li.inconsistentSequence.emplace_front(i);
							fromCN = tmpCN;
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
						updateUnconfirmedTransitions(toCN, i, li);
					}
				}
			}
		}
#if CHECK_PREDECESSORS
		li.nodesWithChangedDomain.erase(tmpCN.get());
#endif	
		return true;
	}

	static bool processIdentified(LearningInfo& li) {
		while (!li.identifiedNodes.empty()) {
			auto node = move(li.identifiedNodes.front());
			li.identifiedNodes.pop_front();
			if (node->state == NULL_STATE) {
				auto parentCN = node->parent.lock()->convergentNode.lock();
				auto input = node->accessSequence.back();
				auto refCN = (*(node->convergentNode.lock()->domain.begin()))->convergent.front()->convergentNode.lock();
//#if CHECK_PREDECESSORS
				cn_pair_set_t closed;
				auto cn = node->convergentNode.lock();
				if (areConvergentNodesDistinguished(cn, refCN, closed)) {
					cn->domain.erase(refCN.get());
					storeInconsistentNode(node, li, LearningInfo::EMPTY_CN_DOMAIN);
					li.identifiedNodes.clear();
					return false;
				}
//#endif
				if (!mergeConvergentNoES(parentCN->next[input], refCN, li)) {
					refCN->convergent.remove(node);
					node->state = NULL_STATE;
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
							li.ot.rn[li.testedState], closed);
						if (sepSeq.empty()) {
							throw;
						}
						li.testedInput = input_t(sepSeq.size()); // a hack to derive separating suffix
						li.inconsistentSequence.splice(li.inconsistentSequence.end(), move(sepSeq));
					}
					li.inconsistentNodes.clear();
					storeInconsistentNode(node, li, LearningInfo::WRONG_MERGE);
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
					if (!updateUnconfirmedTransitions(parentCN, input, li, node)) {
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
							storeInconsistentNode(parentCN->convergent.front(), li, LearningInfo::EMPTY_CN_DOMAIN);
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
			/*
			else {
			node->state = node->observationStatus;
			}*/
		}
		return true;
	}

	static void checkNode(const shared_ptr<OTreeNode>& node, LearningInfo& li) {
		if (node->domain.empty()) {// new state
			storeIdentifiedNode(node, li);
			li.inconsistency = LearningInfo::NEW_STATE_REVEALED;
		}
		else if (node->state == NULL_STATE) {
			if (node->convergentNode.lock()->domain.empty()) {
				storeInconsistentNode(node, li, LearningInfo::EMPTY_CN_DOMAIN);
			}
			else if (node->convergentNode.lock()->domain.size() == 1) {
				storeIdentifiedNode(node, li);
			}
		}
		else if (!node->domain.count(node->state)) {// inconsistent node
			storeInconsistentNode(node, li, LearningInfo::INCONSISTENT_DOMAIN);
		}
	}
	
	static void reduceDomain(const shared_ptr<OTreeNode>& node, const long long& suffixLen, LearningInfo& li) {
		const auto& cn = node->convergentNode.lock();
		for (auto snIt = node->domain.begin(); snIt != node->domain.end();) {
			if (areNodesDifferentUnder(node, li.ot.rn[*snIt]->convergent.front(), suffixLen)) {
				if (cn->domain.erase(li.ot.rn[*snIt].get())) {
					li.ot.rn[*snIt]->domain.erase(cn.get());
				}
				snIt = node->domain.erase(snIt);
			}
			else {
				if (li.inconsistency == LearningInfo::NO_INCONSISTENCY) {
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

	static void checkAgainstAllNodes(const shared_ptr<OTreeNode>& node, const long long& suffixLen, LearningInfo& li) {
		stack<shared_ptr<OTreeNode>> nodes;
		nodes.emplace(li.ot.rn[0]->convergent.front());//root
		while (!nodes.empty()) {
			auto otNode = move(nodes.top());
			nodes.pop();
			if ((otNode != node) && (otNode->domain.count(node->state))) {
				if (areNodesDifferentUnder(node, otNode, suffixLen)) {
					otNode->domain.erase(node->state);
					if ((li.ot.es == 0) && otNode->convergentNode.lock()->domain.erase(li.ot.rn[node->state].get())) {
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

	static void reduceDomainStateNode(const shared_ptr<OTreeNode>& node, const long long& suffixLen, LearningInfo& li) {
		if (node->observationStatus == OTreeNode::QUERIED_RN) {
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
		if ((li.inconsistency == LearningInfo::NO_INCONSISTENCY) && (li.ot.es == 0)) {
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

	static void checkPrevious(shared_ptr<OTreeNode> node, LearningInfo& li, long long suffixAdded = 0) {
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

	static void moveWithCN(shared_ptr<OTreeNode>& currNode, input_t input, LearningInfo& li) {
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
				if (currNode->state != WRONG_STATE) currNode->state = nextCN->state;
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
		}
		else {
			currNode = currNode->next[input];
			auto nextCN = make_shared<ConvergentNode>(currNode);
			for (auto& state : currNode->domain) {
				nextCN->domain.emplace(li.ot.rn[state].get());
				li.ot.rn[state]->domain.emplace(nextCN.get());
			}
			cn->next[input] = move(nextCN);
		}
		currNode->convergentNode = cn->next[input];
	}

	static bool checkIdentified(LearningInfo& li, const unique_ptr<Teacher>& teacher) {
		if (!li.identifiedNodes.empty()) {
			if (li.identifiedNodes.front()->domain.empty()) {// new state
				makeStateNode(move(li.identifiedNodes.front()), li, teacher);
				return false;
			}
			else if (li.inconsistency == LearningInfo::NO_INCONSISTENCY) {
				processIdentified(li);
			}
			else {
				li.identifiedNodes.clear();
			}
		}
		return (li.inconsistency == LearningInfo::NO_INCONSISTENCY);
	}

	static bool moveAndCheckNoES(shared_ptr<OTreeNode>& currNode, input_t input, LearningInfo& li, const unique_ptr<Teacher>& teacher) {
		if (currNode->next[input]) {
			currNode = currNode->next[input];
			return true;
		}
		query(currNode, input, li, teacher);
		moveWithCN(currNode, input, li);
		checkPrevious(currNode, li);
		return checkIdentified(li, teacher);
	}

	static bool moveAndCheckEach(shared_ptr<OTreeNode>& currNode, const sequence_in_t& seq, LearningInfo& li, const unique_ptr<Teacher>& teacher) {
		for (auto& input : seq) {
			if (!moveAndCheckNoES(currNode, input, li, teacher)) {
				return false;
			}
		}
		return true;
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

	static void chooseADS(const shared_ptr<s_ads_cv_t>& ads, const LearningInfo& li, frac_t& bestVal, frac_t& currVal,
		seq_len_t& totalLen, seq_len_t currLength = 1, state_t undistinguishedStates = 0, double prob = 1) {
		auto numStates = state_t(ads->nodes.size()) + undistinguishedStates;
		frac_t localBest(1);
		seq_len_t subtreeLen, minLen = seq_len_t(-1);
		for (input_t i = 0; i < li.conjecture->getNumberOfInputs(); i++) {
			undistinguishedStates = numStates - state_t(ads->nodes.size());
			map<output_t, shared_ptr<s_ads_cv_t>> next;
			for (auto& cn : ads->nodes) {
				auto it = next.end();
				for (auto& node : cn) {
					if (node->next[i]) {
						if (it == next.end()) {
							it = next.find(node->next[i]->incomingOutput);
							if (it == next.end()) {
								it = next.emplace(node->next[i]->incomingOutput, make_shared<s_ads_cv_t>(node->next[i])).first;
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
				//else if (li.conjecture->getType() == TYPE_DFSM) {
				else if (li.conjecture->isOutputState()) {
					for (auto& cn : p.second->nodes) {
						auto it = p.second->next.find(cn.front()->stateOutput);
						if (it == p.second->next.end()) {
							it = p.second->next.emplace(cn.front()->stateOutput, make_shared<s_ads_cv_t>()).first;
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

	static bool identifyByADS(shared_ptr<OTreeNode>& currNode, shared_ptr<s_ads_cv_t> ads, 
			LearningInfo& li, const unique_ptr<Teacher>& teacher) {
		while (ads->input != STOUT_INPUT) {
			if (!moveAndCheckNoES(currNode, ads->input, li, teacher)) {
				return false;
			}
			auto it = ads->next.find(currNode->incomingOutput);
			if (it != ads->next.end()) {// && (!it->second->next.empty())) {
				ads = it->second;
				//if ((li.conjecture->getType() == TYPE_DFSM) && (ads->input == STOUT_INPUT)) {
				if (li.conjecture->isOutputState() && (ads->input == STOUT_INPUT)) {
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
		return true;
	}

	static bool identifyNextState(shared_ptr<OTreeNode>& currNode, LearningInfo& li, const unique_ptr<Teacher>& teacher) {
		if (!moveAndCheckNoES(currNode, li.testedInput, li, teacher)) {
			return false;
		}
		if (currNode->state != NULL_STATE) {// identified
			// optimization - apply the same input again
			if (!moveAndCheckNoES(currNode, li.testedInput, li, teacher)) {
				return false;
			}
			return true;
		}
		auto cn = currNode->convergentNode.lock();
		if (cn->domain.size() < 2) {
			return false;
		}
		if (cn->domain.size() == 2) {// query sepSeq
			//auto idx = FSMsequence::getStatePairIdx((*(cn->domain.begin()))->state, (*(cn->domain.rbegin()))->state);
			auto sepSeq = getQueriedSeparatingSequence((*(cn->domain.begin()))->convergent.front(), (*(++(cn->domain.begin())))->convergent.front());
			//cn_pair_set_t closed;
			//auto sepSeq = getQueriedSeparatingSequenceOfCN(
				//li.ot.rn[(*(cn->domain.begin()))->state], 
				//li.ot.rn[(*(cn->domain.rbegin()))->state], closed);//li.separatingSequences[idx];
			if (!moveAndCheckEach(currNode, sepSeq, li, teacher)) {
				return false;
			}
		}
		else {
			// choose ads
			auto ads = make_shared<s_ads_cv_t>();
			for (auto& sn : cn->domain) {
				ads->nodes.push_back(sn->convergent);
			}
			seq_len_t totalLen = 0;
			frac_t bestVal(1), currVal(0);
			chooseADS(ads, li, bestVal, currVal, totalLen);
			if (!identifyByADS(currNode, ads, li, teacher)) {
				return false;
			}
		}
		return true;
	}

	static void cleanOTreeFromRequestedQueries(LearningInfo& li) {
		for (auto& sn : li.ot.rn) {
			for (auto it = sn->convergent.begin(); it != sn->convergent.end();)	{
				if ((*it)->observationStatus == OTreeNode::NOT_QUERIED) {
					auto parent = (*it)->parent.lock();
					if (parent) {
						parent->next[(*it)->accessSequence.back()].reset();
					}
					it = sn->convergent.erase(it);
				}
				else {
					++it;
				}
			}
		}
		li.requestedQueries.clear();
	}

	static void querySequenceAndCheck(shared_ptr<OTreeNode> currNode, const sequence_in_t& seq,
			LearningInfo& li, const unique_ptr<Teacher>& teacher, bool allowIdentification = true) {
		long long suffixAdded = 0;
		for (auto& input : seq) {
			if (input == STOUT_INPUT) continue;
			if (!currNode->next[input] || (currNode->next[input]->observationStatus == OTreeNode::NOT_QUERIED)) {
				suffixAdded--; 
				if (!query(currNode, input, li, teacher)) {
					currNode = currNode->next[input];
					cleanOTreeFromRequestedQueries(li);
					li.ot.es = 0;
					break;
				}
				if (li.ot.es == 0) {
					moveWithCN(currNode, input, li);
				}
				else {
					currNode = currNode->next[input];
				}
			}
			else {
				currNode = currNode->next[input];
			}
		}
		checkPrevious(move(currNode), li, suffixAdded);
		if (allowIdentification) checkIdentified(li, teacher);
	}

	static bool moveNewStateNode(shared_ptr<OTreeNode>& node, LearningInfo& li, const unique_ptr<Teacher>& teacher) {
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
				storeInconsistentNode(parent, li, LearningInfo::INCONSISTENT_DOMAIN);
				return false;
			}
		} while (!parent->domain.empty());
		return true;
	}

	static void updateOTreeWithNewState(const shared_ptr<OTreeNode>& node, LearningInfo& li) {
		queue<shared_ptr<OTreeNode>> nodes;
		nodes.emplace(li.ot.rn[0]->convergent.front());//root
		while (!nodes.empty()) {
			auto otNode = move(nodes.front());
			nodes.pop();
			if (node && ((node == otNode) || !areNodesDifferent(node, otNode))) {
				otNode->domain.emplace(node->state);
			}
			if (otNode->observationStatus == OTreeNode::QUERIED_NOT_RN) {
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

	static void generateConvergentSubtree(const shared_ptr<ConvergentNode>& cn, OTree& ot, 
			const shared_ptr<ConvergentNode>& origCN = nullptr) {
		const auto& node = cn->convergent.front();
		if (origCN) origCN->convergent.remove(node);
		node->convergentNode = cn;
		if (!cn->isRN) {
			for (auto& state : node->domain) {
				cn->domain.emplace(ot.rn[state].get());
				ot.rn[state]->domain.emplace(cn.get());
			}
		}
		for (input_t input = 0; input < cn->next.size(); input++) {
			if (node->next[input]) {
				if (node->next[input]->observationStatus != OTreeNode::QUERIED_RN) {// not a state node
					//node->next[input]->state = NULL_STATE;
					cn->next[input] = make_shared<ConvergentNode>(node->next[input]);
				}
				if (origCN) {
					if (origCN->next[input]->convergent.size() == 1) {
						cn->next[input].swap(origCN->next[input]);
					} else {
						if (node->next[input]->observationStatus != OTreeNode::QUERIED_RN) {// not a state node
							//node->next[input]->state = NULL_STATE;
							cn->next[input] = make_shared<ConvergentNode>(node->next[input]);
						}
						generateConvergentSubtree(cn->next[input], ot, origCN->next[input]);
					}
				} else {
					if (node->next[input]->observationStatus != OTreeNode::QUERIED_RN) {// not a state node
						//node->next[input]->state = NULL_STATE;
						cn->next[input] = make_shared<ConvergentNode>(node->next[input]);
					}
					generateConvergentSubtree(cn->next[input], ot);
				}
			}
		}
	}

	static bool updateAndInitCN(const shared_ptr<OTreeNode>& node, LearningInfo& li, const unique_ptr<Teacher>& teacher) {
		// update OTree with new state and fill li.identifiedNodes with nodes with |domain|<= 1
		li.identifiedNodes.clear();
		updateOTreeWithNewState(node, li);

		if (!li.identifiedNodes.empty() && (li.identifiedNodes.front()->domain.empty())) {// new state
			return makeStateNode(move(li.identifiedNodes.front()), li, teacher);
		}
		li.ot.es = 0;
		li.testedState = NULL_STATE;
		li.unconfirmedTransitions.clear();
		for (auto& cn : li.ot.rn) {
			cn->domain.clear();
			// clear convergent
			auto stateNode = cn->convergent.front();
			cn->convergent.clear();
			cn->convergent.emplace_back(stateNode);
			cn->state = stateNode->state;
			for (input_t i = 0; i < cn->next.size(); i++) {
				if (!stateNode->next[i] || (stateNode->next[i]->observationStatus != OTreeNode::QUERIED_RN)) {
					cn->next[i].reset();
					auto it = li.unconfirmedTransitions.find(stateNode->state);
					if (it == li.unconfirmedTransitions.end()) {
						li.unconfirmedTransitions.emplace(stateNode->state, set<input_t>({ i }));
					}
					else {
						it->second.insert(i);
					}
				}
			}
		}
		generateConvergentSubtree(li.ot.rn[0], li.ot);

		li.inconsistency = LearningInfo::NO_INCONSISTENCY;
		li.requestedQueries.clear();
		li.inconsistentSequence.clear();
		li.inconsistentNodes.clear();
#if CHECK_PREDECESSORS
		if (!reduceDomainsBySuccessors(li)) {
			return false;
		}
#endif
		return processIdentified(li);
	}

	static bool makeStateNode(shared_ptr<OTreeNode> node, LearningInfo& li, const unique_ptr<Teacher>& teacher) {
		auto parent = node->parent.lock();
		if ((parent->state == NULL_STATE) || (parent->observationStatus != OTreeNode::QUERIED_RN)) {
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
		node->observationStatus = OTreeNode::QUERIED_RN;
		node->state = li.conjecture->addState(node->stateOutput);
		li.conjecture->setTransition(parent->state, node->accessSequence.back(), node->state,
			(li.conjecture->isOutputTransition() ? node->incomingOutput : DEFAULT_OUTPUT));
		auto cn = node->convergentNode.lock();
		li.ot.rn.emplace_back(make_shared<ConvergentNode>(node, true));
		generateConvergentSubtree(li.ot.rn.back(), li.ot, cn);
		/*
		if (cn) {
			if (cn->isRN) {
				cn->convergent.remove(node);
				node->convergentNode = li.ot.rn.back();
				for (input_t i = 0; i < cn->next.size(); i++) {
					if (cn->next[i] && node->next[i]) {
						bool hasSucc = false;
						for (auto& n : cn->convergent) {
							if (n->next[i]) {
								hasSucc = true;
								break;
							}
						}
						if (!hasSucc) {
							li.ot.rn.back()->next[i].swap(cn->next[i]);
						}
					}
				}
			}
			else {
				cn->isRN = true;
				cn->state = node->state;
				cn->convergent.remove(node);
				cn->convergent.emplace_front(node);
				li.ot.rn.emplace_back(cn);
			}
		}
		else {
			li.ot.rn.emplace_back(make_shared<ConvergentNode>(node, true));
			node->convergentNode = li.ot.rn.back();
		}*/
		li.ot.rn[parent->state]->next[node->accessSequence.back()] = li.ot.rn.back();

		return updateAndInitCN(node, li, teacher);
	}

	static shared_ptr<OTreeNode> queryIfNotQueried(shared_ptr<OTreeNode> node, sequence_in_t seq, 
			LearningInfo& li, const unique_ptr<Teacher>& teacher) {
		while (!seq.empty()) {
			if (!node->next[seq.front()]) {
				auto numStates = li.ot.rn.size();
				querySequenceAndCheck(node, seq, li, teacher);
				if (numStates != li.ot.rn.size() || (li.inconsistency == LearningInfo::NO_INCONSISTENCY)) {
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

	static int proveSepSeqOfEmptyDomainIntersection(shared_ptr<OTreeNode> n1, shared_ptr<ConvergentNode> cn2,
		sequence_in_t& sepSeq, cn_pair_set_t& closed, LearningInfo& li, const unique_ptr<Teacher>& teacher);

	static int findSepSeq(const shared_ptr<OTreeNode>& n1, const shared_ptr<ConvergentNode>& cn2,
		const shared_ptr<ConvergentNode>& cn3, const state_t& state,
		sequence_in_t& sepSeq, cn_pair_set_t& closed, LearningInfo& li, const unique_ptr<Teacher>& teacher) {
		auto seq = getQueriedSeparatingSequenceOfCN(cn2, cn3, closed);
		if (!seq.empty()) {
			if (seq.front() == STOUT_INPUT) {
				do {
					seq.pop_front();// STOUT
					auto retVal = proveSepSeqOfEmptyDomainIntersection(cn3->convergent.front(), cn2, seq, closed, li, teacher);
					if (retVal == 1) return 1;// new state or inconsistent domain found
					if (retVal == 2) break;// seq updated to contain a separating sequence
					seq = getQueriedSeparatingSequenceOfCN(cn2, cn3, closed);
				} while (!seq.empty());
				if (seq.empty()) return -1;
			}
			else {
				if (!queryIfNotQueried(cn3->convergent.front(), seq, li, teacher)) {
					return 1;
				}
				if (!queryIfNotQueried(cn2->convergent.front(), seq, li, teacher)) {
					return 1;
				}
			}
			if (!queryIfNotQueried(n1, seq, li, teacher)) {
				return 1;
			}
			if (areNodeAndConvergentDifferentUnder(n1, cn2.get())) {
				sepSeq.splice(sepSeq.end(), move(seq));
				return 2;
			}
			return 0;
		}
		return -1;
	}

	static int proveSepSeqOfEmptyDomainIntersection(shared_ptr<OTreeNode> n1, shared_ptr<ConvergentNode> cn2,
		sequence_in_t& sepSeq, cn_pair_set_t& closed, LearningInfo& li, const unique_ptr<Teacher>& teacher) {
		for (auto& input : sepSeq) {
			cn2 = cn2->next[input];
		}
		n1 = queryIfNotQueried(n1, sepSeq, li, teacher);
		if (!n1) {
			return 1;
		}
		auto d1 = n1->domain;
		for (auto& s : d1) {
			if (n1->domain.count(s)) {
				for (const auto& n2 : cn2->convergent) {
					if (!n2->domain.count(s)) {
						auto seq = getQueriedSeparatingSequence(n2, li.ot.rn[s]->convergent.front());
						if (seq.empty() || (seq.front() == STOUT_INPUT)) {
							throw;
						}
						if (!queryIfNotQueried(n1, seq, li, teacher)) {
							return 1;
						}
						if (n1->domain.count(s) || areNodesDifferentUnder(n1, n2, seq.size())) {
							sepSeq.splice(sepSeq.end(), move(seq));
							return 2;
						}
						break;
					}
				}
			}
		}
		auto retVal = findSepSeq(n1, cn2, n1->convergentNode.lock(), NULL_STATE, sepSeq, closed, li, teacher);
		if (retVal > 0) {
			return retVal;
		}
		d1 = n1->domain;
		for (const auto& s : d1) {
			if (n1->domain.count(s)) {
				retVal = findSepSeq(n1, cn2, li.ot.rn[s], s, sepSeq, closed, li, teacher);
				if (retVal == -1) {
					retVal = findSepSeq(cn2->convergent.front(), n1->convergentNode.lock(), li.ot.rn[s], s, sepSeq, closed, li, teacher);
					/*
					if (cn2->convergent.front()->domain.count(s) || areNodesDifferentUnder(cn2->convergent.front(), n1, seq.size())) {
					sepSeq.splice(sepSeq.end(), move(seq));
					return false;
					}
					*/
				}
				if (retVal > 0) {
					return retVal;
				}
			}
		}
		return 0;
	}

	static bool eliminateSeparatedStatesFromDomain(const shared_ptr<OTreeNode>& node, 
			LearningInfo& li, const unique_ptr<Teacher>& teacher) {
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

	static void resolveEmptyDomain(shared_ptr<OTreeNode>& inconsNode, LearningInfo& li, const unique_ptr<Teacher>& teacher) {
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
		if (inconsPredecessor->observationStatus != OTreeNode::QUERIED_RN) {
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

	static void resolveInconsistentDomain(shared_ptr<OTreeNode>& inconsNode, LearningInfo& li, const unique_ptr<Teacher>& teacher) {
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
		if (inconsPredecessor->observationStatus == OTreeNode::QUERIED_RN) {
			if (inconsNode->state == WRONG_STATE) {
				// updateAndInitCN(nullptr, li, teacher); 
				// forces to update CN layer from the Slearner body function
				li.inconsistentNodes.clear();
				inconsNode->state = NULL_STATE;
				storeInconsistentNode(inconsNode, li, LearningInfo::EMPTY_CN_DOMAIN);
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
				//if ((li.ot.es == 0) && isPrefix(inconsNode->accessSequence, li.bbNode->accessSequence)) {
					sepSuffix = getQueriedSeparatingSequence(inconsNode, li.ot.rn[inconsNode->state]->convergent.front());
				//}
				//else {
					//sepSuffix = getQueriedSeparatingSequence(inconsNode, li.ot.rn[inconsNode->state]->convergent.front());
				//}
				if (sepSuffix.empty() || (sepSuffix.front() == STOUT_INPUT)) {
					throw;
					return;
				}
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

	
	static void processInconsistent(LearningInfo& li, const unique_ptr<Teacher>& teacher) {
		auto inconsNode = move(li.inconsistentNodes.front());
		li.inconsistentNodes.pop_front();
		if ((inconsNode->state != NULL_STATE) && (!inconsNode->domain.count(inconsNode->state))) {
			li.inconsistency = LearningInfo::INCONSISTENT_DOMAIN;
			resolveInconsistentDomain(inconsNode, li, teacher);
		}
		else {
			auto cn1 = inconsNode->convergentNode.lock();
			if (cn1->domain.empty()) {
				resolveEmptyDomain(inconsNode, li, teacher);
			}
			else {// unsuccessfully merged CNs
				//throw;
				/*auto inconsPredecessor = move(li.inconsistentNodes.front());
				li.inconsistentNodes.pop_front();
				auto fn1 = move(li.inconsistentNodes.front());
				li.inconsistentNodes.pop_front();
				auto fn2 = move(li.inconsistentNodes.front());
				li.inconsistentNodes.pop_front();
				if (li.inconsistentSequence.back() == STOUT_INPUT) {
					li.inconsistentSequence.pop_back();
					cn_pair_set_t closed;
					if (proveSepSeqOfEmptyDomainIntersection(inconsPredecessor, inconsNode->convergentNode.lock(), 
						li.inconsistentSequence, closed, li, teacher) == 1) {
						return;
					}
				}
				inconsPredecessor = queryIfNotQueried(inconsPredecessor, li.inconsistentSequence, li, teacher);
				if (!inconsPredecessor) {
					return;
				}*/
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
					if (li.inconsistentNodes.empty()) {
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
	
	static shared_ptr<s_ads_cv_t> getADSwithFixedPrefix(shared_ptr<OTreeNode> node, LearningInfo& li) {
		auto ads = make_shared<s_ads_cv_t>();
		auto cn = node->convergentNode.lock();
		for (auto& sn : cn->domain) {
			ads->nodes.push_back(sn->convergent);
		}
		auto currAds = ads;
		while (node->lastQueriedInput != STOUT_INPUT) {
			currAds->input = node->lastQueriedInput;
			node = node->next[node->lastQueriedInput];
			auto nextADS = make_shared<s_ads_cv_t>();
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

	static bool tryExtendQueriedPath(shared_ptr<OTreeNode>& node, LearningInfo& li, const unique_ptr<Teacher>& teacher) {
		node = li.bbNode;// should be the same
		sequence_in_t seq;
		while (node->state == NULL_STATE) {// find the lowest state node
			seq.emplace_front(node->accessSequence.back());
			node = node->parent.lock();
		}
		auto cn = li.ot.rn[node->state];
		if (seq.size() > 0) {
			if (!cn->next[seq.front()] || !cn->next[seq.front()]->isRN) {
				auto ads = getADSwithFixedPrefix(node->next[seq.front()], li);
				if (ads) {
					li.testedInput = seq.front();
					node = node->next[seq.front()];
					identifyByADS(node, ads, li, teacher);
					return false;// next time tryExtend is called again
				}
			}
			// find unidentified transition closest to the root
			seq_len_t minLen(li.ot.rn.size());
			for (auto& p : li.unconfirmedTransitions) {
				auto& sn = li.ot.rn[p.first];
				if (minLen > sn->convergent.front()->accessSequence.size()) {
					minLen = sn->convergent.front()->accessSequence.size();
					li.testedState = p.first;
					node = sn->convergent.front();
				}
			}
		}
		else {//the state node is leaf
			auto it = li.unconfirmedTransitions.find(node->state);
			if (it != li.unconfirmedTransitions.end()) {
				if (it->second.count(node->accessSequence.back())) {
					li.testedInput = node->accessSequence.back();
				}
				else {
					li.testedInput = *(it->second.begin());
				}
				li.testedState = node->state;
				return true;
			}
			seq_len_t minLen(li.ot.rn.size());
			for (auto& p : li.unconfirmedTransitions) {
				auto& sn = li.ot.rn[p.first];
				if (minLen > sn->convergent.front()->accessSequence.size()) {
					minLen = sn->convergent.front()->accessSequence.size();
					li.testedState = p.first;
				}
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
						if (p.second->next[i] && p.second->next[i]->isRN) {
							seq = p.first;
							seq.emplace_back(i);
							if (li.unconfirmedTransitions.count(p.second->next[i]->state)) {
								if (!moveAndCheckEach(node, seq, li, teacher)) {
									return false;
								}
								if (!li.unconfirmedTransitions.count(node->state)) {// identified by seq
									li.testedState = NULL_STATE;
									return false;
								}
								li.testedState = node->state;
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
				node = li.ot.rn[li.testedState]->convergent.front();
			}
		}
		auto& trans = li.unconfirmedTransitions.at(li.testedState);
		if (!node->accessSequence.empty() && trans.count(node->accessSequence.back())) {
			li.testedInput = node->accessSequence.back();
		}
		else {
			li.testedInput = *(trans.begin());
		}
		return true;
	}

	unique_ptr<DFSM> Slearner(const unique_ptr<Teacher>& teacher, state_t maxExtraStates,
		function<bool(const unique_ptr<DFSM>& conjecture)> provideTentativeModel, bool isEQallowed) {
		if (!teacher->isBlackBoxResettable()) {
			ERROR_MESSAGE("FSMlearning::Slearner - the Black Box needs to be resettable");
			return nullptr;
		}

		/// Observation Tree
		LearningInfo li;
		auto numInputs = teacher->getNumberOfInputs();
		li.conjecture = FSMmodel::createFSM(teacher->getBlackBoxModelType(), 1, numInputs, teacher->getNumberOfOutputs());
		teacher->resetBlackBox();
		auto node = make_shared<OTreeNode>(DEFAULT_OUTPUT, 0, numInputs); // root
		node->domain.emplace(0);
		node->observationStatus = OTreeNode::QUERIED_RN;
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
		li.numberOfExtraStates = 1;
		auto cn = make_shared<ConvergentNode>(node, true);
		node->convergentNode = cn;
		li.ot.rn.emplace_back(cn);
		li.ot.es = 0;
		//li.sc.st = make_unique<SplittingTree>(1);
		auto tIt = li.unconfirmedTransitions.emplace(0, set<input_t>()).first;
		for (input_t i = 0; i < numInputs; i++) {
			tIt->second.insert(i);
		}
		li.testedState = 0;
		li.testedInput = 0;
		bool unlearned = true;
		while (unlearned) {
			if (!li.inconsistentNodes.empty()) {
				auto numStates = li.ot.rn.size();
				processInconsistent(li, teacher);
				if (numStates == li.ot.rn.size()) {
					if (li.inconsistentNodes.empty()) {
						//updateAndInitCN(nullptr, li, teacher);
						throw;
					}
					else {
						auto& inconsNode = li.inconsistentNodes.front();
						if ((inconsNode->state == NULL_STATE) || (inconsNode->domain.count(inconsNode->state))) {
							updateAndInitCN(nullptr, li, teacher);
						}
					}
				}
				node = li.bbNode;
			} else if (!li.unconfirmedTransitions.empty()) {// identify all transitions
				if ((li.testedState != NULL_STATE) || tryExtendQueriedPath(node, li, teacher)) {
					if (identifyNextState(node, li, teacher)) {
						if (li.ot.rn[li.testedState]->next[li.testedInput]->isRN) {
							li.testedState = NULL_STATE;
						}
						else {
							node = li.ot.rn[li.testedState]->convergent.front();
						}
					}
				}
			} else if (!li.requestedQueries.empty()) {// verify transitions for given ES
				auto it = li.requestedQueries.end();
				--it;
				auto seq = move(*it);
				li.requestedQueries.erase(it);
				querySequenceAndCheck(li.ot.rn[0]->convergent.front(), seq, li, teacher);
			} else {// increase the number of assumed ES
				if (li.ot.es == 0) {
					li.ot.es = li.numberOfExtraStates;
					auto mapping = li.conjecture->minimize();
					for (auto p : mapping) {
						if (p.first != p.second) {
							throw;// states were reordered - should not occur
						}
					}
					li.sc.st = FSMsequence::getSplittingTree(li.conjecture, true, true);
				}
				else {
					if ((li.ot.rn.size() > 1) || (li.ot.es > 2 * maxExtraStates)) li.numberOfExtraStates++;
					li.ot.es++;
				}
				if (li.numberOfExtraStates > maxExtraStates) {
					unlearned = false;
					if (isEQallowed) {
						auto ce = teacher->equivalenceQuery(li.conjecture);
						if (!ce.empty()) {
							if (li.numberOfExtraStates > 1) li.numberOfExtraStates--;
							li.ot.es = 0;
							querySequenceAndCheck(li.ot.rn[0]->convergent.front(), ce, li, teacher);
							unlearned = true;
						}
					}
				} else {
					li.requestedQueries = S_method_ext(li.conjecture, li.ot, li.sc);
					for (auto& sn : li.ot.rn) {
						if (sn->convergent.front()->observationStatus != OTreeNode::QUERIED_RN) {
							auto it = find_if(sn->convergent.begin(), sn->convergent.end(),
								[](const shared_ptr<OTreeNode>& n) {
								return (n->observationStatus == OTreeNode::QUERIED_RN);
							});
							swap(sn->convergent.front(), *it);
						}
					}
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
