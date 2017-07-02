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

#define DUMP_OQ 0

#define CHECK_PREDECESSORS 1
	/*
	struct ConvergentNode;

	struct OTreeNode {
		sequence_in_t accessSequence;
		output_t incomingOutput;
		output_t stateOutput;
		state_t state;
		state_t assumedState;
		seq_len_t maxSuffixLen;
		weak_ptr<OTreeNode> parent;
		vector<shared_ptr<OTreeNode>> next;

		input_t lastQueriedInput;
		set<state_t> refStates;
		weak_ptr<ConvergentNode> convergentNode;

		OTreeNode(input_t numberOfInputs) :
			incomingOutput(DEFAULT_OUTPUT), state(NULL_STATE), stateOutput(DEFAULT_OUTPUT),
			next(numberOfInputs), lastQueriedInput(STOUT_INPUT), assumedState(NULL_STATE), maxSuffixLen(0)
		{
		}

		OTreeNode(const shared_ptr<OTreeNode>& parent, input_t input,
			output_t transitionOutput, output_t stateOutput, input_t numInputs) :
			accessSequence(parent->accessSequence), incomingOutput(transitionOutput), state(NULL_STATE),
			stateOutput(stateOutput), next(numInputs), parent(parent), lastQueriedInput(STOUT_INPUT),
			assumedState(NULL_STATE), maxSuffixLen(0)
		{
			accessSequence.push_back(input);
		}
	};

	inline bool sCNcompare(const ConvergentNode* ls, const ConvergentNode* rs);

	struct sCNcomp {
		bool operator() (const ConvergentNode* ls, const ConvergentNode* rs) const {
			return sCNcompare(ls, rs);
		}
	};

	typedef set<ConvergentNode*, sCNcomp> s_cn_set_t;

	struct ConvergentNode {
		list<shared_ptr<OTreeNode>> convergent;
		vector<shared_ptr<ConvergentNode>> next;
		s_cn_set_t domain;// or consistent cn in case of state nodes
		bool isRN = false;
		
		ConvergentNode(const shared_ptr<OTreeNode>& node) {
			convergent.emplace_back(node);
			next.resize(node->next.size());
		}
	};
	*/
	typedef set<ConvergentNode*> s_cn_set_t;

	inline bool sCNcompare(const ConvergentNode* ls, const ConvergentNode* rs) {
		const auto& las = ls->convergent.front()->accessSequence;
		const auto& ras = rs->convergent.front()->accessSequence;
		if (las.size() != ras.size()) return las.size() < ras.size();
		return las < ras;
	}
	struct SOTree {
		vector<shared_ptr<ConvergentNode>> stateNodes;
		shared_ptr<OTreeNode> bbNode;

		state_t numberOfExtraStates;
		unique_ptr<DFSM> conjecture;

		unique_ptr<SplittingTree> st;
		list<pair<state_t,input_t>> unconfirmedTransitions;

		list<shared_ptr<OTreeNode>> identifiedNodes;
		list<shared_ptr<OTreeNode>> inconsistentNodes;
		sequence_in_t inconsistentSequence;
		list<pair<state_t, sequence_in_t>> requestedQueries;

		state_t testedState;
		input_t testedInput;
		vector<sequence_in_t> separatingSequences;
#if CHECK_PREDECESSORS
		s_cn_set_t nodesWithChangedDomain;
#endif
#if DUMP_OQ
		unique_ptr<DFSM> OTree;
#endif
	};

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

	static void storeInconsistentNode(const shared_ptr<OTreeNode>& node, SOTree& ot, bool inconsistent = true) {
		ot.inconsistentNodes.emplace_front(node);
	}
	
	static void storeInconsistentNode(const shared_ptr<OTreeNode>& node, const shared_ptr<OTreeNode>& diffNode, SOTree& ot) {
		ot.inconsistentNodes.emplace_front(diffNode);
		ot.inconsistentNodes.emplace_front(node);
	}

	static void storeIdentifiedNode(const shared_ptr<OTreeNode>& node, SOTree& ot) {
		if (ot.identifiedNodes.empty()) {
			ot.identifiedNodes.emplace_back(node);
		}
		else if (node->refStates.empty()) {
			if (!ot.identifiedNodes.front()->refStates.empty() ||
				(ot.identifiedNodes.front()->accessSequence.size() > node->accessSequence.size())) {
				ot.identifiedNodes.clear();
				ot.identifiedNodes.emplace_back(node);
			}
		}
		else if (!ot.identifiedNodes.front()->refStates.empty()) {
			if (node->accessSequence.size() < ot.identifiedNodes.front()->accessSequence.size()) {
				ot.identifiedNodes.emplace_front(node);
			}
			else {
				ot.identifiedNodes.emplace_back(node);
			}
		}
	}

	static bool isIntersectionEmpty(const s_cn_set_t& domain1, const s_cn_set_t& domain2) {
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
		return ((n2->next[idx]) && ((n1->next[idx]->incomingOutput != n2->next[idx]->incomingOutput)
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
		set<pair<state_t, ConvergentNode*>>& closed) {
		if (cn1 == cn2) return false;
		if (cn1->convergent.front()->stateOutput != cn2->convergent.front()->stateOutput) return true;
		if (cn1->isRN || cn2->isRN)  {
			if (cn2->isRN) return !cn1->domain.count(cn2.get());
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
					|| areConvergentNodesDistinguished(cn1->next[i], cn2->next[i], closed))
					return true;
			}
		}
		return false;
	}

	static bool isSeparatingSequenceQueried(shared_ptr<OTreeNode> node, state_t state, sequence_in_t& sepSeq, SOTree& ot) {
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
			if (retVal.empty() && isIntersectionEmpty(p.first->refStates, p.second->refStates)) {
				retVal = getAccessSequence(n1, p.first);
				retVal.emplace_front(STOUT_INPUT);
			}
#endif
			fifo.pop();
		}
		return retVal;
	}

	static sequence_in_t getQueriedSeparatingSequenceFromSN(const shared_ptr<OTreeNode>& n1, const shared_ptr<ConvergentNode>& cn2) {
		if (n1->stateOutput != cn2->convergent.front()->stateOutput) {
			return sequence_in_t();
		}
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
			if (retVal.empty() && !p.second->isRN && !p.first->convergentNode.lock()->isRN
				&& isIntersectionEmpty(p.first->convergentNode.lock()->domain, p.second->domain)) {
				retVal = getAccessSequence(n1, p.first);
				retVal.emplace_front(STOUT_INPUT);
			}
#endif
			fifo.pop();
		}
		return retVal;
	}

	static bool isCNdifferent(const shared_ptr<ConvergentNode>& cn1, const shared_ptr<ConvergentNode>& cn2) {
		// cn1 has empty domain
		s_cn_set_t domain(cn2->domain);
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

	static sequence_in_t getQueriedSeparatingSequenceOfCN(const shared_ptr<ConvergentNode>& cn1, const shared_ptr<ConvergentNode>& cn2) {
		if (cn1->convergent.front()->stateOutput != cn2->convergent.front()->stateOutput) {
			return sequence_in_t();
		}
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
				((p.first->isRN && !p.second->isRN
				&& !p.second->domain.empty() && !p.second->domain.count(p.first.get())) ||
				(!p.first->isRN && p.second->isRN
				&& !p.first->domain.empty() && !p.first->domain.count(p.second.get())))) ||
				(!p.first->isRN && !p.second->isRN
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

	static void query(const shared_ptr<OTreeNode>& node, input_t input, SOTree& ot, const unique_ptr<Teacher>& teacher) {
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
		auto otreeState = ot.OTree->addState(stateOutput);
		auto otreeStartState = ot.OTree->getEndPathState(0, node->accessSequence);
		ot.OTree->setTransition(otreeStartState, input, otreeState, (ot.OTree->isOutputTransition() ? transitionOutput : DEFAULT_OUTPUT));
#endif // DUMP_OQ
		checkNumberOfOutputs(teacher, ot.conjecture);
		auto leaf = make_shared<OTreeNode>(node, input, transitionOutput, stateOutput, NULL_STATE, ot.conjecture->getNumberOfInputs());// teacher->getNextPossibleInputs());
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

	static bool makeStateNode(shared_ptr<OTreeNode> node, SOTree& ot, const unique_ptr<Teacher>& teacher);

#if CHECK_PREDECESSORS
	static bool checkPredecessors(shared_ptr<OTreeNode> node, shared_ptr<ConvergentNode> parentCN, SOTree& ot) {
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

	static bool processChangedNodes(SOTree& ot) {
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

	static bool reduceDomainsBySuccessors(SOTree& ot) {
		for (const auto& cn : ot.stateNodes) {
			for (auto it = cn->domain.begin(); it != cn->domain.end();) {
				set<pair<state_t, ConvergentNode*>> closed;
				if (areConvergentNodesDistinguished(cn, (*it)->convergent.front()->convergentNode.lock(), closed)) {
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
				}
				else {
					++it;
				}
			}
		}
		return processChangedNodes(ot);
	}
#endif

	static bool updateUnconfirmedTransitions(const shared_ptr<ConvergentNode>& toCN, input_t i, SOTree& ot,
			shared_ptr<OTreeNode> node = nullptr) {
		/*auto it = toCN->requestedQueries->next.find(i);
		if (it != toCN->requestedQueries->next.end()) {
			if (!node) {
				auto nIt = toCN->convergent.begin();
				while (!(*nIt)->next[i] || ((*nIt)->next[i]->state == NULL_STATE)) ++nIt;
				node = (*nIt)->next[i];
			}
			ot.conjecture->setTransition(toCN->convergent.front()->state, i, node->state,
				(ot.conjecture->isOutputTransition() ? node->incomingOutput : DEFAULT_OUTPUT));
			ot.unconfirmedTransitions--;
			toCN->requestedQueries->seqCount--;
			toCN->requestedQueries->next.erase(it);
			return true;
		}*/
		return false;
	}
	static bool mergeConvergentNoES(shared_ptr<ConvergentNode>& fromCN, const shared_ptr<ConvergentNode>& toCN, SOTree& ot) {
		if (toCN->isRN) {// a state node
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
				set<pair<state_t, ConvergentNode*>> closed;
				if (areConvergentNodesDistinguished(fromCN, (*toIt)->convergent.front()->convergentNode.lock(), closed)) {
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
			}
			else
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
					if (tmpCN->next[i]->isRN) {
						if (toCN->next[i]->isRN) {// a different state node
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
						if (toCN->isRN) {// state node
							updateUnconfirmedTransitions(toCN, i, ot);
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
					if (toCN->isRN && toCN->next[i]->isRN) {// state nodes
						updateUnconfirmedTransitions(toCN, i, ot);
					}
				}
			}
		}
#if CHECK_PREDECESSORS
		ot.nodesWithChangedDomain.erase(tmpCN.get());
#endif	
		return true;
	}

	static bool processIdentified(SOTree& ot) {
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
				if (parentCN->isRN) {// state node
					if (!updateUnconfirmedTransitions(parentCN, input, ot, node)) {
						ot.identifiedNodes.clear();
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

	static bool checkNodeNoES(const shared_ptr<OTreeNode>& node, SOTree& ot) {
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
			}
			else if (node->convergentNode.lock()->domain.size() == 1) {
				storeIdentifiedNode(node, ot);
			}
		}
		return true;
	}

	static void checkAgainstAllNodesNoES(const shared_ptr<OTreeNode>& node, const seq_len_t& suffixLen, bool& isConsistent, SOTree& ot) {
		stack<shared_ptr<OTreeNode>> nodes;
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

	static void reduceDomainNoES(const shared_ptr<OTreeNode>& node, const seq_len_t& suffixLen, bool& isConsistent, SOTree& ot) {
		const auto& cn = node->convergentNode.lock();
		for (auto snIt = node->refStates.begin(); snIt != node->refStates.end();) {
			if (areNodesDifferentUnder(node, ot.stateNodes[*snIt]->convergent.front(), suffixLen)) {
				if (cn->domain.erase(ot.stateNodes[*snIt].get())) {
					ot.stateNodes[*snIt]->domain.erase(cn.get());
				}
				snIt = node->refStates.erase(snIt);
			}
			else {
				if (isConsistent) {
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

	static void reduceDomainStateNodeNoES(const shared_ptr<OTreeNode>& node, const seq_len_t& suffixLen, bool& isConsistent, SOTree& ot) {
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

	static bool checkPreviousNoES(shared_ptr<OTreeNode> node, SOTree& ot, const unique_ptr<Teacher>& teacher) {
		bool isConsistent = ot.inconsistentNodes.empty() && ot.inconsistentSequence.empty();
		seq_len_t suffixLen = 0;
		input_t input(STOUT_INPUT);
		do {
			node->lastQueriedInput = input;
			if (suffixLen > node->maxSuffixLen) node->maxSuffixLen = suffixLen;
			if (node->state == NULL_STATE) {
				reduceDomainNoES(node, suffixLen, isConsistent, ot);
				isConsistent &= checkNodeNoES(node, ot);
			}
			else {// state node
				reduceDomainStateNodeNoES(node, suffixLen, isConsistent, ot);
			}
			if (!node->accessSequence.empty()) input = node->accessSequence.back();
			node = node->parent.lock();
			suffixLen++;
		} while (node);

		if (!ot.identifiedNodes.empty()) {
			if (ot.identifiedNodes.front()->refStates.empty() && (ot.testedState != NULL_STATE)) {// new state
				isConsistent &= makeStateNode(move(ot.identifiedNodes.front()), ot, teacher);
			}
			else if (isConsistent) {// no inconsistency
				isConsistent &= processIdentified(ot);
			}
			else {
				ot.identifiedNodes.clear();
			}
		}
		return isConsistent;
	}

	static bool moveAndCheckNoES(shared_ptr<OTreeNode>& currNode, input_t input, SOTree& ot, const unique_ptr<Teacher>& teacher) {
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
			if (nextCN->isRN) {// state node
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
		}
		else {
			currNode = currNode->next[input];
			auto nextCN = make_shared<ConvergentNode>(currNode);
			for (auto state : currNode->refStates) {
				nextCN->domain.emplace(ot.stateNodes[state].get());
				ot.stateNodes[state]->domain.emplace(nextCN.get());
			}
			cn->next[input] = move(nextCN);
		}
		currNode->convergentNode = cn->next[input];
		return checkPreviousNoES(currNode, ot, teacher);
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

	static void chooseADS(const shared_ptr<s_ads_cv_t>& ads, const SOTree& ot, frac_t& bestVal, frac_t& currVal,
		seq_len_t& totalLen, seq_len_t currLength = 1, state_t undistinguishedStates = 0, double prob = 1) {
		auto numStates = state_t(ads->nodes.size()) + undistinguishedStates;
		frac_t localBest(1);
		seq_len_t subtreeLen, minLen = seq_len_t(-1);
		for (input_t i = 0; i < ot.conjecture->getNumberOfInputs(); i++) {
			undistinguishedStates = numStates - state_t(ads->nodes.size());
			map<output_t, shared_ptr<s_ads_cv_t>> next;
			for (auto& cn : ads->nodes) {
				bool isUndistinguished = true;
				for (auto& node : cn) {
					if (node->next[i]) {
						auto it = next.find(node->next[i]->incomingOutput);
						if (it == next.end()) {
							next.emplace(node->next[i]->incomingOutput, make_shared<s_ads_cv_t>(node->next[i]));
						}
						else if (isUndistinguished) {
							it->second->nodes.emplace_back(list<shared_ptr<OTreeNode>>({ node->next[i] }));
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
								p.second->next.emplace(node->stateOutput, make_shared<s_ads_cv_t>(node));
							}
							else if (isFirstNode) {
								it->second->nodes.emplace_back(list<shared_ptr<OTreeNode>>({ node }));
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

	static bool identifyByADS(shared_ptr<OTreeNode>& currNode, shared_ptr<s_ads_cv_t> ads, 
			SOTree& ot, const unique_ptr<Teacher>& teacher) {
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

	static bool identifyNextState(shared_ptr<OTreeNode>& currNode, SOTree& ot, const unique_ptr<Teacher>& teacher) {
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
		}
		else {
			// choose ads
			auto ads = make_shared<s_ads_cv_t>();
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


	static bool querySequenceAndCheck(shared_ptr<OTreeNode> currNode, const sequence_in_t& seq,
			SOTree& ot, const unique_ptr<Teacher>& teacher, bool allowIdentification = true) {
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
		else {/*
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
			}*/
		}
		return isConsistent;
	}

	static bool moveNewStateNode(shared_ptr<OTreeNode>& node, SOTree& ot, const unique_ptr<Teacher>& teacher) {
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

	static void updateOTreeWithNewState(const shared_ptr<OTreeNode>& node, SOTree& ot) {
		queue<shared_ptr<OTreeNode>> nodes;
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

	static bool makeStateNode(shared_ptr<OTreeNode> node, SOTree& ot, const unique_ptr<Teacher>& teacher) {
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
		/*sequence_set_t hsi;
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
		*/
		node->state = ot.conjecture->addState(node->stateOutput);
		ot.stateNodes.emplace_back(make_shared<ConvergentNode>(node));
		ot.stateNodes.back()->isRN = true; // make_shared<requested_query_node_t>();
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
		//generateRequestedQueries(ot);

		ot.inconsistentSequence.clear();
		ot.inconsistentNodes.clear();
#if CHECK_PREDECESSORS
		if (!reduceDomainsBySuccessors(ot)) {
			return false;
		}
#endif
		return processIdentified(ot);
	}

	static shared_ptr<OTreeNode> queryIfNotQueried(shared_ptr<OTreeNode> node, sequence_in_t seq, 
			SOTree& ot, const unique_ptr<Teacher>& teacher) {
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

	static bool proveSepSeqOfEmptyDomainIntersection(shared_ptr<OTreeNode> n1, list<shared_ptr<OTreeNode>>& nodes2,
			sequence_in_t& sepSeq, SOTree& ot, const unique_ptr<Teacher>& teacher) {
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
									list<shared_ptr<OTreeNode>>({ next2 }), seq, ot, teacher)) {
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
									list<shared_ptr<OTreeNode>>({ next1 }), seq, ot, teacher)) {
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

	static bool proveSepSeqOfEmptyDomainIntersection(const shared_ptr<OTreeNode>& n1, shared_ptr<ConvergentNode> cn2,
		sequence_in_t& sepSeq, SOTree& ot, const unique_ptr<Teacher>& teacher) {
		for (auto& input : sepSeq) {
			cn2 = cn2->next[input];
		}
		return proveSepSeqOfEmptyDomainIntersection(n1, cn2->convergent, sepSeq, ot, teacher);
	}

	static bool eliminateSeparatedStatesFromDomain(const shared_ptr<OTreeNode>& node, const shared_ptr<ConvergentNode>& cn,
		SOTree& ot, const unique_ptr<Teacher>& teacher) {
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
		const sequence_in_t& transferSeq, SOTree& ot, const unique_ptr<Teacher>& teacher) {
		sequence_in_t sepSeq;
#if CHECK_PREDECESSORS
		sequence_in_t seqToDistDomains;
		shared_ptr<OTreeNode> distDomN1, distDomN2;
#endif
		list<shared_ptr<OTreeNode>> diffCN, cn;
		queue<shared_ptr<OTreeNode>> nodes;
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
			if (proveSepSeqOfEmptyDomainIntersection(distDomN1, list<shared_ptr<OTreeNode>>({ distDomN2 }), sepSeq, ot, teacher)) {
				return seqToDistDomains;
			}
		}
#endif
		return sepSeq;
	}

	static void processInconsistent(SOTree& ot, const unique_ptr<Teacher>& teacher) {
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
					}
					else {
						shared_ptr<OTreeNode> distN2;
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
							sepSeq.pop_front();
							distN2 = queryIfNotQueried(distN2, sepSeq, ot, teacher);
							if (proveSepSeqOfEmptyDomainIntersection(ot.stateNodes[state]->convergent.front(),
								list<shared_ptr<OTreeNode>>({ distN2 }), sepSeq, ot, teacher)) {
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
			}
			else {
				if (n1->assumedState != WRONG_STATE) {
					sequence_in_t sepSuffix;
					if ((ot.numberOfExtraStates == 0) && isPrefix(n1->accessSequence, ot.bbNode->accessSequence)) {
						sepSuffix = getQueriedSeparatingSequenceFromSN(n1, ot.stateNodes[n1->assumedState]);
					}
					else {
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
				}
				if (!queryIfNotQueried(n1, move(ot.inconsistentSequence), ot, teacher)) {
					return;
				}
				return;
			}
		}
	}
	/*
	static bool distinguish(const shared_ptr<ConvergentNode>& cn, const list<shared_ptr<ConvergentNode>>& nodes,
		SOTree& ot, const unique_ptr<Teacher>& teacher) {
		list<shared_ptr<ConvergentNode>> domain;
		auto state = cn->convergent.front()->state;
		if (!cn->requestedQueries) {
			for (auto dIt = cn->domain.begin(); dIt != cn->domain.end(); ++dIt) {
				const auto& node = (*dIt)->convergent.front();
				if (state != node->state) {
					domain.emplace_back(node->convergentNode.lock());
				}
			}
		}
		for (const auto& np : nodes) {
			set<pair<state_t, ConvergentNode*>> closed;
			if ((state != np->convergent.front()->state) && !areConvergentNodesDistinguished(np, cn, closed))  {
				domain.emplace_back(np);
			}
		}
		//auto accessSeq = getAccessSequence(nodes.front()->convergent);
		while (!domain.empty()) {
			set<state_t> diffStates;
			for (const auto& n : domain) {
				diffStates.insert(n->convergent.front()->state);
			}
			diffStates.insert(state);
			auto seq = FSMsequence::getSeparatingSequenceFromSplittingTree(ot.conjecture, ot.st, state, diffStates, true);

			querySequenceAndCheck()
			addSequence(cn, seq, ot, fsm);
			auto refOut = fsm->getOutputAlongPath(state, seq);
			for (auto dIt = domain.begin(); dIt != domain.end();) {
				auto out = fsm->getOutputAlongPath((*dIt)->state, seq);
				auto outIt = out.begin();
				auto refOutIt = refOut.begin();
				auto inIt = seq.begin();
				while ((refOutIt != refOut.end()) && (*outIt == *refOutIt)) {
					++outIt;
					++refOutIt;
					++inIt;
				}
				if (refOutIt != refOut.end()) {
					++inIt;
					sequence_in_t diffSeq(seq.begin(), inIt);
					addSequence((*dIt), diffSeq, ot, fsm);
					cn->domain.erase((*dIt).get());
					dIt = domain.erase(dIt);
				}
				else {
					++dIt;
				}
			}
			// check for a possible separation by the recent added sequences
			for (auto dIt = domain.begin(); dIt != domain.end();) {
				set<pair<state_t, ConvergentNodeS*>> closed;
				if (!cn->domain.count((*dIt).get()) &&
					((find(nodes.begin(), nodes.end(), *dIt) == nodes.end())
					|| areConvergentNodesDistinguished(cn, *dIt, ot.es == 0, closed))) {
					dIt = domain.erase(dIt);
				}
				else {
					++dIt;
				}
			}
		}
	}

	static bool verifyConvergence(const shared_ptr<ConvergentNode>& cn, const shared_ptr<ConvergentNode>& refCN,
		list<shared_ptr<ConvergentNode>>& nodes, int depth, SOTree& ot, const unique_ptr<Teacher>& teacher) {
		if (depth > 0) {
			nodes.emplace_back(cn);
			if (refCN && !refCN->requestedQueries) nodes.emplace_back(refCN);
			for (input_t i = 0; i < ot.conjecture->getNumberOfInputs(); i++) {
				auto ncn = cn;
				moveAndCheck(ncn);
				verifyConvergence(ncn, refCN->next[i], nodes, depth - 1, ot, teacher);
			}
			if (refCN && !refCN->requestedQueries) nodes.pop_back();
			nodes.pop_back();
		}
		else {
			distinguish(cn, nodes, fsm, sepSeq, ot);
			distinguish(refCN, nodes, fsm, sepSeq, ot);
		}
	}
	*/
	unique_ptr<DFSM> Slearner(const unique_ptr<Teacher>& teacher, state_t maxExtraStates,
		function<bool(const unique_ptr<DFSM>& conjecture)> provideTentativeModel, bool isEQallowed) {
		if (!teacher->isBlackBoxResettable()) {
			ERROR_MESSAGE("FSMlearning::Slearner - the Black Box needs to be resettable");
			return nullptr;
		}

		/// Observation Tree
		SOTree ot;
		auto numInputs = teacher->getNumberOfInputs();
		ot.conjecture = FSMmodel::createFSM(teacher->getBlackBoxModelType(), 1, numInputs, teacher->getNumberOfOutputs());
		teacher->resetBlackBox();
		auto node = make_shared<OTreeNode>(DEFAULT_OUTPUT, 0, numInputs); // root
		node->refStates.emplace(0);
		if (ot.conjecture->isOutputState()) {
			node->stateOutput = teacher->outputQuery(STOUT_INPUT);
			checkNumberOfOutputs(teacher, ot.conjecture);
			ot.conjecture->setOutput(0, node->stateOutput);
		}
#if DUMP_OQ
		ot.OTree = FSMmodel::createFSM(teacher->getBlackBoxModelType(), 1, numInputs, teacher->getNumberOfOutputs());
		if (ot.OTree->isOutputState()) {
			ot.OTree->setOutput(0, node->stateOutput);
		}
#endif
		ot.bbNode = node;
		ot.numberOfExtraStates = 0;
		auto cn = make_shared<ConvergentNode>(node);
		node->convergentNode = cn;
		ot.stateNodes.emplace_back(cn);
		ot.st = make_unique<SplittingTree>(1);
		for (input_t i = 0; i < numInputs; i++) {
			ot.unconfirmedTransitions.emplace_back(0, i);
		}
		
		//cn->requestedQueries = make_shared<requested_query_node_t>();
		//ot.stateIdentifier.emplace_back(sequence_set_t());
		//generateRequestedQueries(ot);

		//ot.testedState = 0;
		//ot.testedInput = 0;
		bool checkPossibleExtension = false;
		bool unlearned = true;
		while (unlearned) {
			while ((!ot.unconfirmedTransitions.empty()) || (!ot.inconsistentNodes.empty())) {
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
							//generateRequestedQueries(ot);

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
				} else if (ot.numberOfExtraStates == 0) {
					auto& tran = ot.unconfirmedTransitions.front();
					//cn = ot.stateNodes[tran.first];

					ot.testedState = tran.first;
					ot.testedInput = tran.second;
					if (checkPossibleExtension) {
						//if (tryExtendQueriedPath(node, ot, teacher)) {
							// inconsistency
							continue;
						//}
					}
					else {
						node = ot.stateNodes[ot.testedState]->convergent.front();
					}
					checkPossibleExtension = (identifyNextState(node, ot, teacher)
						|| (ot.stateNodes[ot.testedState]->next[ot.testedInput]->isRN));
				} else {
					// choose tran
					auto& tran = ot.unconfirmedTransitions.front();

					// identify
					//query;
					//tryExtendQueriedPath(node, ot, teacher);

					//generate requested queries;
					while (!ot.requestedQueries.empty() && ot.inconsistentNodes.empty()) {
						//query;
						//tryExtendQueriedPath(node, ot, teacher);
					}
				}
				if (provideTentativeModel) {
#if DUMP_OQ
					unlearned = provideTentativeModel(ot.OTree);
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
			}
			else {
				if (ot.numberOfExtraStates == 1) {
					ot.st = FSMsequence::getSplittingTree(ot.conjecture, true, true);
				}
				/*/update
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
				}*/
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