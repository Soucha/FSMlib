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

#include "FSMtesting.h"

using namespace FSMsequence;

namespace FSMtesting {
	static bool isIntersectionEmpty(const cn_set_t& domain1, const cn_set_t& domain2) {
		//if (domain1.empty() || domain2.empty()) return false;
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

	static bool areConvergentNodesDistinguished(const shared_ptr<ConvergentNode>& cn1, const shared_ptr<ConvergentNode>& cn2,
			bool noES, set<pair<state_t, ConvergentNode*>>& closed) {
		if ((cn1 == cn2) || (cn1->state == cn2->state)) return false;
		if (cn1->convergent.front()->stateOutput != cn2->convergent.front()->stateOutput) return true;
		if (cn1->isRN || cn2->isRN)  {
			if ((cn1->isRN && cn2->isRN) || // different RNs are distinguished
				!cn1->domain.count(cn2.get())) return true;
			if (cn1->isRN) {
				if (!closed.emplace(cn1->state, cn2.get()).second) return false;
			}
			else {
				if (!closed.emplace(cn2->state, cn1.get()).second) return false;
			}
		}
		else if (noES && isIntersectionEmpty(cn1->domain, cn2->domain)) return true;
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
					|| areConvergentNodesDistinguished(cn1->next[i], cn2->next[i], noES, closed))
					return true;
			}
		}
		return false;
	}
	
	static bool areDistinguishedUnder(const shared_ptr<OTreeNode>& node, ConvergentNode* cn, bool noES) {
		if (node->stateOutput != cn->convergent.front()->stateOutput) return true;
		auto& cn1 = node->convergentNode.lock();
		if (cn->isRN || cn1->isRN)  {
			if (cn->isRN && cn1->isRN) {
				if (cn1.get() != cn) return true;
			}
			else if (!cn1->domain.count(cn)) return true;
		}
		else if (noES && isIntersectionEmpty(cn1->domain, cn->domain)) return true;
		if ((node->lastQueriedInput == STOUT_INPUT) || (!cn->next[node->lastQueriedInput])) return false;
		auto& input = node->lastQueriedInput;
		auto it = cn->convergent.begin();
		while (!(*it)->next[input]) {
			++it;
		}
		return ((node->next[input]->incomingOutput != (*it)->next[input]->incomingOutput)
			|| areDistinguishedUnder(node->next[input], cn->next[input].get(), noES));
	}

	static void changeFirstNode(const shared_ptr<ConvergentNode>& cn, const shared_ptr<OTreeNode>& node) {
		for (auto& n : cn->domain) {
			n->domain.erase(cn.get());
		}
		cn->convergent.emplace_front(node);
		for (auto& n : cn->domain) {
			n->domain.insert(cn.get());
		}
	}

	static void mergeCN(const shared_ptr<ConvergentNode>& fromCN, const shared_ptr<ConvergentNode>& toCN,
		list<shared_ptr<ConvergentNode>>& identifiedNodes, bool noES) {
		for (auto& rn : fromCN->domain) {
			rn->domain.erase(fromCN.get());
		}
		if (fromCN->convergent.front()->accessSequence.size() < toCN->convergent.front()->accessSequence.size()) {
			changeFirstNode(toCN, fromCN->convergent.front());
			fromCN->convergent.pop_front();
			toCN->convergent.front()->convergentNode = toCN;
		}
		for (auto&& n : fromCN->convergent) {
			n->convergentNode = toCN;
			toCN->convergent.emplace_back(n);
		}
		fromCN->convergent.clear();
		for (auto&& n : fromCN->leafNodes) {
			toCN->leafNodes.emplace_back(n);
		}
		fromCN->leafNodes.clear();

		// merge successors
		for (input_t i = 0; i < fromCN->next.size(); i++) {
			if (fromCN->next[i]) {
				if (toCN->next[i]) {
					if (fromCN->next[i]->isRN) {
						if (!toCN->next[i]->isRN) {
							auto nextCN = move(toCN->next[i]);
							toCN->next[i] = fromCN->next[i];
							mergeCN(nextCN, fromCN->next[i], identifiedNodes, noES);
						}
						// else they are already merged
					}
					else {
						mergeCN(fromCN->next[i], toCN->next[i], identifiedNodes, noES);
					}
				}
				else {
					toCN->next[i] = fromCN->next[i];
				}
			}
		}
		for (auto toIt = toCN->domain.begin(); toIt != toCN->domain.end();) {
			set<pair<state_t, ConvergentNode*>> closed;
			if ((!toCN->isRN && !fromCN->domain.count(*toIt)) ||
				(toCN->isRN && areConvergentNodesDistinguished(toCN, (*toIt)->convergent.front()->convergentNode.lock(), noES, closed))) {
				(*toIt)->domain.erase(toCN.get());
				toIt = toCN->domain.erase(toIt);
			}
			else ++toIt;
		}
		if (noES && !toCN->isRN && (toCN->domain.size() == 1)) {
			identifiedNodes.emplace_back(toCN);
		}
		fromCN->domain.clear();
	}

	static void processIdentified(list<shared_ptr<ConvergentNode>>& identifiedNodes, bool noES) {
		while (!identifiedNodes.empty()) {
			auto& identifiedCN = identifiedNodes.front();
			if (!identifiedCN->convergent.empty()) {
				auto& n = identifiedCN->convergent.front();
				auto prevCN = n->parent.lock()->convergentNode.lock();
				auto incomingInput = n->accessSequence.back();
				auto nextStateCN = (*(identifiedCN->domain.begin()))->convergent.front()->convergentNode.lock();
				prevCN->next[incomingInput] = nextStateCN;
				mergeCN(identifiedCN, nextStateCN, identifiedNodes, noES);
			}
			identifiedNodes.pop_front();
		}
	}

	static void updateDomains(shared_ptr<OTreeNode> node, bool noES) {
		input_t input(STOUT_INPUT);
		list<shared_ptr<ConvergentNode>> identifiedNodes;
		do {
			node->lastQueriedInput = input;
			auto cn = node->convergentNode.lock();
			for (auto dIt = cn->domain.begin(); dIt != cn->domain.end();) {
				if (areDistinguishedUnder(node, *dIt, noES)) {
					(*dIt)->domain.erase(cn.get());
					if (noES && !(*dIt)->isRN && ((*dIt)->domain.size() == 1)) {
						identifiedNodes.emplace_back((*dIt)->convergent.front()->convergentNode.lock());
					}
					dIt = cn->domain.erase(dIt);
				}
				else ++dIt;
			}
			if (noES && !cn->isRN && (cn->domain.size() == 1)) {
				identifiedNodes.emplace_back(cn);
			}
			if (!node->accessSequence.empty()) input = node->accessSequence.back();
			node = node->parent.lock();
		} while (node);
		processIdentified(identifiedNodes, noES);
	}

	static shared_ptr<OTreeNode> extendNodeWithInput(const shared_ptr<OTreeNode>& node, input_t input, const unique_ptr<DFSM>& fsm) {
		state_t state = fsm->getNextState(node->state, input);
		output_t outputState = (state == NULL_STATE) ? WRONG_OUTPUT :
			(fsm->isOutputState()) ? fsm->getOutput(state, STOUT_INPUT) : DEFAULT_OUTPUT;
		output_t outputTransition = (state == NULL_STATE) ? WRONG_OUTPUT :
			(fsm->isOutputTransition()) ? fsm->getOutput(node->state, input) : DEFAULT_OUTPUT;
		auto nextNode = make_shared<OTreeNode>(node, input,
			outputTransition, outputState, state, fsm->getNumberOfInputs());
		node->next[input] = nextNode;
		return nextNode;
	}

	static shared_ptr<OTreeNode> appendSequence(shared_ptr<OTreeNode> node,
			const sequence_in_t& seq, const OTree& ot, const unique_ptr<DFSM>& fsm, bool checkDomains = true) {
		shared_ptr<ConvergentNode> cn = (checkDomains) ? node->convergentNode.lock() : nullptr;
		auto it = seq.begin();
		for (; it != seq.end(); ++it) {
			if (*it == STOUT_INPUT) continue;
			if (node->next[*it]) {
				node = node->next[*it];
				if (checkDomains) cn = cn->next[*it];
			}
			else break;
		}
		if (it != seq.end()) {
			for (; it != seq.end(); ++it) {
				if (*it == STOUT_INPUT) continue;
				node = extendNodeWithInput(node, *it, fsm);
				if (checkDomains) {
					if (!cn->next[*it]) {
						cn->next[*it] = make_shared<ConvergentNode>(node);
						cn = cn->next[*it];
						if (fsm->isOutputState()) {
							for (state_t i = 0; i < ot.rn.size(); i++) {
								if (node->stateOutput == fsm->getOutput(i, STOUT_INPUT)) {
									cn->domain.insert(ot.rn[i].get());
									ot.rn[i]->domain.insert(cn.get());
								}
							}
						}
						else {
							for (const auto& sn : ot.rn) {
								cn->domain.insert(sn.get());
								sn->domain.insert(cn.get());
							}
						}
					}
					else {
						cn = cn->next[*it];
						if (node->accessSequence.size() < cn->convergent.front()->accessSequence.size()) {
							changeFirstNode(cn, node);
						}
						else {
							cn->convergent.emplace_back(node);
						}
					}
					node->convergentNode = cn;
				}
			}
			if (checkDomains) {
				cn->leafNodes.emplace_back(node);
				updateDomains(node, ot.es == 0);
			}
		}
		return node;
	}

	static bool hasLeafBack(const shared_ptr<ConvergentNode>& prevCN) {
		while (!prevCN->leafNodes.empty() && (prevCN->leafNodes.back()->lastQueriedInput != STOUT_INPUT)) {
			prevCN->leafNodes.pop_back();
		}
		return !prevCN->leafNodes.empty();
	}

	static void addSequence(const shared_ptr<ConvergentNode>& cn, const sequence_in_t& seq, 
			const OTree& ot, const unique_ptr<DFSM>& fsm) {
		auto bestStartNodeIt = cn->convergent.begin();
		seq_len_t maxLen(0);
		for (auto nodeIt = cn->convergent.begin(); nodeIt != cn->convergent.end(); ++nodeIt) {
			seq_len_t len(1);// enable to start at cn's leaf
			auto node = *nodeIt;
			for (auto it = seq.begin(); it != seq.end(); ++it, len++) {
				if (*it == STOUT_INPUT) continue;
				if (node->next[*it]) {
					node = node->next[*it];
				}
				else {
					if ((node->lastQueriedInput == STOUT_INPUT) && (maxLen < len)) {
						maxLen = len;
						bestStartNodeIt = nodeIt;
					}
					node = nullptr;
					break;
				}
			}
			if (node) return;// sequence already queried
		}
		if ((maxLen == 0) && (!cn->isRN || (cn->state != 0))) {
			auto minLen = (*bestStartNodeIt)->accessSequence.size();
			queue<pair<shared_ptr<ConvergentNode>, sequence_in_t>> fifo;
			fifo.emplace(cn, sequence_in_t());
			set<ConvergentNode*> closedCNs;
			closedCNs.insert(cn.get());
			sequence_in_t bestTransferSeq;
			while (!fifo.empty()) {
				auto p = move(fifo.front());
				fifo.pop();
				for (auto& n : p.first->convergent) {
					auto nPrevCN = n->parent.lock()->convergentNode.lock();
					if (closedCNs.insert(nPrevCN.get()).second) {
						auto nSeq = p.second;
						nSeq.push_front(n->accessSequence.back());
						if (hasLeafBack(nPrevCN)) {
							nSeq.insert(nSeq.end(), seq.begin(), seq.end());
							auto ln = nPrevCN->leafNodes.back();
							nPrevCN->leafNodes.pop_back();
							appendSequence(ln, nSeq, ot, fsm);
							return;
						}
						if ((nPrevCN->convergent.front()->accessSequence.size() + nSeq.size()) < 
							((*bestStartNodeIt)->accessSequence.size() + bestTransferSeq.size())) {
							bestTransferSeq = nSeq;
							bestStartNodeIt = nPrevCN->convergent.begin();
						}
						if ((nSeq.size() < minLen) && (!nPrevCN->isRN || (nPrevCN->state != 0))) {
							fifo.emplace(move(nPrevCN), move(nSeq));
						}
					}
					if (!p.first->isRN) break;
				}
			}
			bestTransferSeq.insert(bestTransferSeq.end(), seq.begin(), seq.end());
			appendSequence(*bestStartNodeIt, bestTransferSeq, ot, fsm);
		}
		else {
			appendSequence(*bestStartNodeIt, seq, ot, fsm);
		}
	}

	static void generateConvergentSubtree(const shared_ptr<ConvergentNode>& cn, const OTree& ot,
			list<shared_ptr<OTreeNode>>& leaves) {
		const auto& node = cn->convergent.front();
		node->convergentNode = cn;
		if (!cn->isRN) {
			for (const auto& sn : ot.rn) {
				cn->domain.insert(sn.get());
				sn->domain.insert(cn.get());
			}
		}
		bool isLeaf = true;
		for (input_t input = 0; input < cn->next.size(); input++) {
			if (node->next[input]) {
				if (!node->next[input]->convergentNode.lock()) {// not a state node
					cn->next[input] = make_shared<ConvergentNode>(node->next[input]);
				}
				generateConvergentSubtree(cn->next[input], ot, leaves);
				isLeaf = false;
			}
			else if (cn->next[input]) {
				cn->next[input].reset();
			}
		}
		if (isLeaf) {
			leaves.emplace_back(node);
		}
	}

	static void createDivergencePreservingStateCover(const unique_ptr<DFSM>& fsm, OTree& ot, const StateCharacterization& sc) {
		const auto numInputs = fsm->getNumberOfInputs();
		ot.rn.resize(fsm->getNumberOfStates());
		auto& stateNodes = ot.rn;
		if (!stateNodes[0]) {
			output_t outputState = (fsm->isOutputState()) ? fsm->getOutput(0, STOUT_INPUT) : DEFAULT_OUTPUT;
			auto root = make_shared<OTreeNode>(outputState, 0, fsm->getNumberOfInputs());

			stateNodes[0] = make_shared<ConvergentNode>(root, true);
			root->convergentNode = stateNodes[0];

			for (state_t i = 0; i < stateNodes.size(); i++) {
				root->domain.emplace(i);
			}
			auto seq = getSeparatingSequenceFromSplittingTree(fsm, sc.st, 0, root->domain, true);
			appendSequence(root, seq, ot, fsm, false);
		}
		queue<shared_ptr<OTreeNode>> fifo, fifoNext;
		fifo.emplace(stateNodes[0]->convergent.front());
		do {
			auto numNodes = fifo.size();
			for (size_t i = 0; i < numNodes; i++) {
				auto node = move(fifo.front());
				fifo.pop();
				for (input_t input = 0; input < numInputs; input++) {
					const auto& nn = node->next[input];
					if (nn && (!stateNodes[nn->state] || (nn->observationStatus == OTreeNode::QUERIED_RN))) {
						if (!stateNodes[nn->state]) {
							stateNodes[nn->state] = make_shared<ConvergentNode>(nn, true);
							nn->convergentNode = stateNodes[nn->state];
							stateNodes[node->state]->next[input] = stateNodes[nn->state];
							for (state_t i = 0; i < stateNodes.size(); i++) {
								nn->domain.emplace(i);
							}
							auto seq = getSeparatingSequenceFromSplittingTree(fsm, sc.st, nn->state, nn->domain, true);
							appendSequence(nn, seq, ot, fsm, false);
						}
						fifoNext.emplace(nn);
					}
				}
				fifo.emplace(node);
			}
			while (!fifo.empty()) {
				auto node = move(fifo.front());
				fifo.pop();
				for (input_t input = 0; input < fsm->getNumberOfInputs(); input++) {
					auto nextState = fsm->getNextState(node->state, input);
					if ((nextState != NULL_STATE) && !stateNodes[nextState]) {
						set<state_t> states;
						for (state_t i = 0; i < stateNodes.size(); i++) {
							states.emplace(i);
						}
						auto seq = getSeparatingSequenceFromSplittingTree(fsm, sc.st, nextState, states, true);
						seq.push_front(input);
						appendSequence(node, seq, ot, fsm, false);
						auto& nn = node->next[input];
						nn->domain.swap(states);
						stateNodes[nn->state] = make_shared<ConvergentNode>(nn, true);
						nn->convergentNode = stateNodes[nn->state];
						stateNodes[node->state]->next[input] = stateNodes[nn->state];
						fifoNext.emplace(nn);
					}
				}
			}
			fifo.swap(fifoNext);
		} while (!fifo.empty());
		//printf("sc\n");
		// make state cover divergence preserving
		for (const auto& sn : stateNodes) {
			while (sn->convergent.size() > 1) {
				if (sn->convergent.back()->observationStatus == OTreeNode::QUERIED_RN) {
					swap(sn->convergent.front(), sn->convergent.back());
				}
				sn->convergent.back()->convergentNode.reset();
				sn->convergent.pop_back();
			}
			sn->leafNodes.clear();
			const auto& node = sn->convergent.front();
			for (auto snIt = node->domain.begin(); snIt != node->domain.end();) {
				if ((node->state != *snIt) && areNodesDifferent(node, stateNodes[*snIt]->convergent.front())) {
					snIt = node->domain.erase(snIt);
				}
				else {
					++snIt;
				}
			}
			while (node->domain.size() > 1) {
				auto seq = getSeparatingSequenceFromSplittingTree(fsm, sc.st, sn->state, node->domain, true);
				appendSequence(node, seq, ot, fsm, false);
				auto refOut = fsm->getOutputAlongPath(sn->state, seq);
				for (auto it = node->domain.begin(); it != node->domain.end();) {
					if (*it != sn->state) {
						auto out = fsm->getOutputAlongPath(*it, seq);
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
							appendSequence(stateNodes[*it]->convergent.front(), diffSeq, ot, fsm, false);
							it = node->domain.erase(it);
						}
						else ++it;
					}
					else ++it;
				}
			}
		}
		//printf("div sc\n");
		// generate convergent and init domains
		list<shared_ptr<OTreeNode>> leaves;
		generateConvergentSubtree(stateNodes[0], ot, leaves);
		for (auto& node : leaves) {
			updateDomains(node, ot.es == 0);
		}
	}

	static void printTStree(const shared_ptr<OTreeNode>& node, string prefix = "") {
		printf("%s%d/%d <- %d\n", prefix.c_str(), node->state, node->stateOutput, node->incomingOutput);
		input_t i = 0;
		for (auto nn : node->next) {
			if (nn) {
				printf("%s - %d ->\n", prefix.c_str(), i);
				printTStree(nn, prefix + "\t");
			}
			i++;
		}
	}
	
	static void distinguish(const shared_ptr<ConvergentNode>& cn, const list<shared_ptr<ConvergentNode>>& nodes,
			const unique_ptr<DFSM>& fsm, const StateCharacterization& sepSeq, const OTree& ot) {
		list<shared_ptr<ConvergentNode>> domain;
		auto state = cn->state;
		if (!cn->isRN) {
			for (auto dIt = cn->domain.begin(); dIt != cn->domain.end(); ++dIt) {
				const auto& node = (*dIt)->convergent.front();
				if (state != node->state) {
					domain.emplace_back(node->convergentNode.lock());
				}
			}
		}
		for (const auto& np : nodes) {
			set<pair<state_t, ConvergentNode*>> closed;
			if ((state != np->state) && !areConvergentNodesDistinguished(np, cn, ot.es == 0, closed))  {
				domain.emplace_back(np);
			}
		}
		while (!domain.empty()) {
			set<state_t> diffStates;
			for (const auto& n : domain) {
				diffStates.insert(n->state);
			}
			diffStates.insert(state);
			auto seq = getSeparatingSequenceFromSplittingTree(fsm, sepSeq.st, state, diffStates, true);
			/*
			auto seq = getShortestSepSeq(state, diffStates, sepSeq, fsm);
			cost_t cost(getLenCost(cn, seq), state_t(domain.size()));
			if (cost.first > seq.size()) {// try to find better sequence
				auto refOut = fsm->getOutputAlongPath(state, seq);
				calcCost(cost, seq, refOut, domain, fsm);
				auto minCost = cost;
				list<cost_t> succExtNodes;
				auto succSeq = chooseSepSeqAsExtension(cn, minCost, cost_t(0,state_t(domain.size())), domain, succExtNodes, fsm, sepSeq);
				if (cmpCosts(minCost, cost)) {
					seq.swap(succSeq);
				}
			}*/
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
				set<pair<state_t, ConvergentNode*>> closed;
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

	static void distinguishCNs(const shared_ptr<ConvergentNode>& cn, const shared_ptr<ConvergentNode>& refCN,
		list<shared_ptr<ConvergentNode>>& nodes, const unique_ptr<DFSM>& fsm, const StateCharacterization& sepSeq,
			int depth, const OTree& ot) {
		distinguish(cn, nodes, fsm, sepSeq, ot);
		if (refCN) distinguish(refCN, nodes, fsm, sepSeq, ot);
		if (depth > 0) {
			nodes.emplace_back(cn);
			if (refCN && !refCN->isRN) nodes.emplace_back(refCN);
			for (input_t i = 0; i < fsm->getNumberOfInputs(); i++) {
				distinguishCNs(cn->next[i], refCN ? refCN->next[i] : nullptr, nodes, fsm, sepSeq, depth - 1, ot);
			}
			if (refCN && !refCN->isRN) nodes.pop_back();
			nodes.pop_back();
		}
	}

	static list<sequence_in_t> getLongestTraversalSequences(input_t numInputs, state_t extraStates) {
		list<sequence_in_t> sequences;
		queue<sequence_in_t> fifo;
		fifo.emplace(sequence_in_t());
		while (!fifo.empty()) {
			auto seq = move(fifo.front());
			fifo.pop();
			if (seq.size() == extraStates) {
				sequences.emplace_back(move(seq));
			}
			else {
				for (input_t i = 0; i < numInputs; i++) {
					seq.push_back(i);
					fifo.emplace(seq);
					seq.pop_back();
				}
			}
		}
		return sequences;
	}

	static sequence_set_t getSequences(const shared_ptr<OTreeNode>& node, const unique_ptr<DFSM>& fsm) {
		sequence_set_t TS;
		stack<pair<shared_ptr<OTreeNode>, sequence_in_t>> lifo;
		sequence_in_t seq;
		if (fsm->isOutputState()) seq.push_back(STOUT_INPUT);
		lifo.emplace(node, move(seq));
		while (!lifo.empty()) {
			auto p = move(lifo.top());
			lifo.pop();
			bool hasSucc = false;
			for (input_t i = 0; i < p.first->next.size(); i++) {
				if (p.first->next[i]) {
					hasSucc = true;
					sequence_in_t seq(p.second);
					seq.push_back(i);
					if (fsm->isOutputState()) seq.push_back(STOUT_INPUT);
					lifo.emplace(p.first->next[i], move(seq));
				}
			}
			if (!hasSucc && (p.first->observationStatus == OTreeNode::NOT_QUERIED)) {
				TS.emplace(move(p.second));
			}
		}
		return TS;
	}

	static void extendWithTravSeqs(const shared_ptr<ConvergentNode>& sn, const list<sequence_in_t>& travSeqs, const unique_ptr<DFSM>& fsm) {
		sn->leafNodes.clear();
		for (auto& cn : sn->domain) {
			auto& node = cn->convergent.front();
			node->convergentNode = sn;
			sn->convergent.emplace_back(node);
			input_t i = 0;
			for (; i < node->next.size(); i++) {
				if (node->next[i]) break;
			}
			if (i == node->next.size()) {
				sn->leafNodes.emplace_back(node);
			}
		}
		sn->domain.clear();
		for (auto& seq : travSeqs) {
			auto node = sn->convergent.front();
			for (auto& input : seq) {
				if (node->next[input]) {
					node = node->next[input];
				}
				else {
					node = extendNodeWithInput(node, input, fsm);
					node->convergentNode = sn;
					sn->convergent.emplace_back(node);
				}
			}
			if (node->observationStatus == OTreeNode::NOT_QUERIED)
				sn->leafNodes.emplace_back(node);
		}
		for (input_t i = 0; i < sn->next.size(); i++) {
			sn->next[i] = sn;
		}
	}

	sequence_set_t S_method_ext(const unique_ptr<DFSM>& fsm, OTree& ot, StateCharacterization& sc) {
		RETURN_IF_UNREDUCED(fsm, "FSMtesting::S_method", sequence_set_t());
		if (ot.es < 0) {
			return sequence_set_t();
		}
		if (!sc.st)
			sc.st = getSplittingTree(fsm, true, false);
		createDivergencePreservingStateCover(fsm, ot, sc);
		// stateNodes are initialized divergence-preserving state cover
		auto& stateNodes = ot.rn;
		auto travSeqs = getLongestTraversalSequences(fsm->getNumberOfInputs(), ot.es);

		if (fsm->getNumberOfStates() == 1) {
			extendWithTravSeqs(stateNodes[0], travSeqs, fsm);
			return getSequences(stateNodes[0]->convergent.front(), fsm);
		}
		//printf("divPres SC designed\n");
		
		using tran_t = tuple<shared_ptr<ConvergentNode>, input_t, shared_ptr<ConvergentNode>>;
		list<tran_t> transitions;
		for (const auto& sn : stateNodes) {
			for (input_t i = 0; i < sn->next.size(); i++) {
				if ((!sn->next[i] || !sn->next[i]->isRN) && (fsm->getNextState(sn->state, i) != NULL_STATE)) {
					transitions.emplace_back(sn, i, stateNodes[fsm->getNextState(sn->state, i)]);
				}
			}
		}
		transitions.sort([](const tran_t& t1, const tran_t& t2){
			return (get<0>(t1)->convergent.front()->accessSequence.size() + get<2>(t1)->convergent.front()->accessSequence.size())
				< (get<0>(t2)->convergent.front()->accessSequence.size() + get<2>(t2)->convergent.front()->accessSequence.size());
		});

		// confirm all transitions -> convergence-preserving transition cover
		while (!transitions.empty()) {
			//printf("%u\n", transitions.size());
			auto startCN = move(get<0>(transitions.front()));
			auto input = get<1>(transitions.front());
			auto nextStateCN = move(get<2>(transitions.front()));
			transitions.pop_front();
			if (startCN->next[input] && (startCN->next[input] == nextStateCN)) continue; // no ES and already verified

			//bool proveConvergence = false;  //!transitions.empty();// TODO a condition when else to omit converngence proof
			set<state_t> startingStates;
			for (const auto& t : transitions) {
				startingStates.insert(get<0>(t)->state);
			}
			bool proveConvergence(startingStates.count(nextStateCN->state) > 0 ? true : false);
			if (!proveConvergence && !startingStates.empty()) {
				for (auto& seq : travSeqs) {
					auto state = nextStateCN->state;
					for (auto& i : seq) {
						state = fsm->getNextState(state, i);
						if (startingStates.count(state)) {
							proveConvergence = true;
							break;
						}
					}
					if (proveConvergence) break;
				}
			}
			//if (!proveConvergence) printf("|");
			proveConvergence = true;
			// query minimal requested subtree
			for (auto& seq : travSeqs) {
				seq.push_front(input);
				addSequence(startCN, seq, ot, fsm);
				seq.pop_front();
				if (proveConvergence) addSequence(nextStateCN, seq, ot, fsm);
			}
			// identify next state
			auto ncn = startCN->next[input];
			list<shared_ptr<ConvergentNode>> tmp;
			distinguishCNs(ncn, proveConvergence ? nextStateCN : nullptr, tmp, fsm, sc, ot.es, ot);

			if (proveConvergence && (startCN->next[input] != nextStateCN)) {// already merged in case of no ES
				startCN->next[input] = nextStateCN;
				mergeCN(ncn, nextStateCN, tmp, ot.es == 0);
				processIdentified(tmp, ot.es == 0);
			}
		}
		// obtain TS
		//printTStree(root);
		return getSequences(stateNodes[0]->convergent.front(), fsm);
	}

	sequence_set_t S_method(const unique_ptr<DFSM>& fsm, int extraStates) {
		RETURN_IF_UNREDUCED(fsm, "FSMtesting::S_method", sequence_set_t());
		if (extraStates < 0) {
			return sequence_set_t();
		}
		OTree ot;
		ot.es = extraStates;
		return S_method_ext(fsm, ot, StateCharacterization());
	}
}


