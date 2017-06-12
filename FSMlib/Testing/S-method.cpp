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

	struct ConvergentNodeS;

	struct TestNodeS {
		sequence_in_t accessSequence;
		output_t incomingOutput;
		output_t stateOutput;
		state_t state;
		weak_ptr<TestNodeS> parent;
		vector<shared_ptr<TestNodeS>> next;
		weak_ptr<ConvergentNodeS> convergentNode;
		input_t lastQueriedInput;

		TestNodeS(output_t stateOutput, state_t state, input_t numberOfInputs) :
			incomingOutput(DEFAULT_OUTPUT), stateOutput(stateOutput), state(state),
			next(numberOfInputs), lastQueriedInput(STOUT_INPUT)
		{
		}
		
		TestNodeS(const shared_ptr<TestNodeS>& parent, input_t input,
			output_t transitionOutput, output_t stateOutput, state_t state, input_t numberOfInputs) :
			accessSequence(parent->accessSequence), incomingOutput(transitionOutput), stateOutput(stateOutput),
			state(state), parent(parent), next(numberOfInputs), lastQueriedInput(STOUT_INPUT)
		{
			accessSequence.push_back(input);
		}
	};

	struct ConvergentNodeS {
		list<shared_ptr<TestNodeS>> leafNodes, convergent;
		set<ConvergentNodeS*> domain;
		vector<shared_ptr<ConvergentNodeS>> next;
		state_t state;
		bool isRN = false;

		ConvergentNodeS(input_t numberOfInputs) : next(numberOfInputs) {}

		ConvergentNodeS(const shared_ptr<TestNodeS>& node, bool isRN = false) : 
			state(node->state), next(node->next.size()), isRN(isRN) {
			convergent.emplace_back(node);
		}
	};

	struct SeparatingSequencesInfo {
		vector<LinkCell> sepSeq;
		map<set<state_t>, sequence_in_t> bestSeq;
		unique_ptr<SplittingTree> st;
	};

	struct OTreeS {
		vector<shared_ptr<ConvergentNodeS>> rn;
		state_t es;
	};

	static bool isIntersectionEmpty(const set<ConvergentNodeS*>& domain1, const set<ConvergentNodeS*>& domain2) {
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

	static bool areNodesDifferent(const shared_ptr<TestNodeS>& n1, const shared_ptr<TestNodeS>& n2) {
		if (n1->stateOutput != n2->stateOutput) return true;
		for (input_t i = 0; i < n1->next.size(); i++) {
			if ((n1->next[i]) && (n2->next[i]) && ((n1->next[i]->incomingOutput != n2->next[i]->incomingOutput)
				|| areNodesDifferent(n1->next[i], n2->next[i])))
				return true;
		}
		return false;
	}

	static bool areConvergentNodesDistinguished(const shared_ptr<ConvergentNodeS>& cn1, const shared_ptr<ConvergentNodeS>& cn2,
			bool noES, set<pair<state_t, ConvergentNodeS*>>& closed) {
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
	
	static bool areDistinguishedUnder(const shared_ptr<TestNodeS>& node, ConvergentNodeS* cn, bool noES) {
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

	static void mergeCN(const shared_ptr<ConvergentNodeS>& fromCN, const shared_ptr<ConvergentNodeS>& toCN,
		list<shared_ptr<ConvergentNodeS>>& identifiedNodes, bool noES) {
		if (fromCN->convergent.front()->accessSequence.size() < toCN->convergent.front()->accessSequence.size()) {
			toCN->convergent.emplace_front(move(fromCN->convergent.front()));
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

		for (auto& rn : fromCN->domain) {
			rn->domain.erase(fromCN.get());
		}
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
			set<pair<state_t, ConvergentNodeS*>> closed;
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

	static void processIdentified(list<shared_ptr<ConvergentNodeS>>& identifiedNodes, bool noES) {
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

	static void updateDomains(shared_ptr<TestNodeS> node, bool noES) {
		input_t input(STOUT_INPUT);
		list<shared_ptr<ConvergentNodeS>> identifiedNodes;
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

	static shared_ptr<TestNodeS> extendNodeWithInput(const shared_ptr<TestNodeS>& node, input_t input, const unique_ptr<DFSM>& fsm) {
		state_t state = fsm->getNextState(node->state, input);
		output_t outputState = (state == NULL_STATE) ? WRONG_OUTPUT :
			(fsm->isOutputState()) ? fsm->getOutput(state, STOUT_INPUT) : DEFAULT_OUTPUT;
		output_t outputTransition = (state == NULL_STATE) ? WRONG_OUTPUT :
			(fsm->isOutputTransition()) ? fsm->getOutput(node->state, input) : DEFAULT_OUTPUT;
		auto nextNode = make_shared<TestNodeS>(node, input,
			outputTransition, outputState, state, fsm->getNumberOfInputs());
		node->next[input] = nextNode;
		return nextNode;
	}

	static shared_ptr<TestNodeS> appendSequence(shared_ptr<TestNodeS> node,
			const sequence_in_t& seq, const OTreeS& ot, const unique_ptr<DFSM>& fsm, bool checkDomains = true) {
		shared_ptr<ConvergentNodeS> cn = (checkDomains) ? node->convergentNode.lock() : nullptr;
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
						cn->next[*it] = make_shared<ConvergentNodeS>(node);
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
							cn->convergent.emplace_front(node);
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

	static bool hasLeafBack(const shared_ptr<ConvergentNodeS>& prevCN) {
		while (!prevCN->leafNodes.empty() && (prevCN->leafNodes.back()->lastQueriedInput != STOUT_INPUT)) {
			prevCN->leafNodes.pop_back();
		}
		return !prevCN->leafNodes.empty();
	}

	static void addSequence(const shared_ptr<ConvergentNodeS>& cn, const sequence_in_t& seq, 
			const OTreeS& ot, const unique_ptr<DFSM>& fsm) {
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
		if ((maxLen == 0) && (cn->state != 0)) {
			auto minLen = (*bestStartNodeIt)->accessSequence.size() - 1;
			queue<pair<shared_ptr<ConvergentNodeS>, sequence_in_t>> fifo;
			fifo.emplace(cn, sequence_in_t());
			set<ConvergentNodeS*> closedCNs;
			closedCNs.insert(cn.get());
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
						if ((nSeq.size() < minLen) && (!nPrevCN->isRN || (nPrevCN->state != 0))) {
							fifo.emplace(move(nPrevCN), move(nSeq));
						}
					}
					if (!p.first->isRN) break;
				}
			}	
		}
		appendSequence(*bestStartNodeIt, seq, ot, fsm);
	}

	static void generateConvergentSubtree(const shared_ptr<ConvergentNodeS>& cn, const OTreeS& ot,
			list<shared_ptr<TestNodeS>>& leaves) {
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
					cn->next[input] = make_shared<ConvergentNodeS>(node->next[input]);
				}
				generateConvergentSubtree(cn->next[input], ot, leaves);
				isLeaf = false;
			}
		}
		if (isLeaf) {
			leaves.emplace_back(node);
		}
	}

	static OTreeS getDivergencePreservingStateCover(const unique_ptr<DFSM>& fsm, const SeparatingSequencesInfo& sepSeq, state_t es) {
		OTreeS ot;
		ot.es = es;
		const auto numInputs = fsm->getNumberOfInputs();
		ot.rn.resize(fsm->getNumberOfStates());
		auto& stateNodes = ot.rn;
		output_t outputState = (fsm->isOutputState()) ? fsm->getOutput(0, STOUT_INPUT) : DEFAULT_OUTPUT;
		auto root = make_shared<TestNodeS>(outputState, 0, fsm->getNumberOfInputs());
		
		stateNodes[0] = make_shared<ConvergentNodeS>(root, true);
		root->convergentNode = stateNodes[0];
		set<state_t> states;// (fsm->getNumberOfStates());
		for (state_t i = 0; i < stateNodes.size(); i++) {
			states.emplace(i);
		}
		auto seq = getSeparatingSequenceFromSplittingTree(fsm, sepSeq.st, 0, states, true);
		appendSequence(root, seq, ot, fsm, false);
		
		queue<shared_ptr<TestNodeS>> fifo, fifoNext;
		fifo.emplace(root);
		do {
			auto numNodes = fifo.size();
			for (size_t i = 0; i < numNodes; i++) {
				auto node = move(fifo.front());
				fifo.pop();
				for (input_t input = 0; input < numInputs; input++) {
					const auto& nn = node->next[input];
					if (nn && !stateNodes[nn->state]) {
						stateNodes[nn->state] = make_shared<ConvergentNodeS>(nn, true);
						nn->convergentNode = stateNodes[nn->state];
						stateNodes[node->state]->next[input] = stateNodes[nn->state];
						auto seq = getSeparatingSequenceFromSplittingTree(fsm, sepSeq.st, nn->state, states, true);
						appendSequence(nn, seq, ot, fsm, false);
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
						auto seq = getSeparatingSequenceFromSplittingTree(fsm, sepSeq.st, nextState, states, true);
						seq.push_front(input);
						appendSequence(node, seq, ot, fsm, false);
						auto& nn = node->next[input];
						stateNodes[nn->state] = make_shared<ConvergentNodeS>(nn, true);
						nn->convergentNode = stateNodes[nn->state];
						stateNodes[node->state]->next[input] = stateNodes[nn->state];
						fifoNext.emplace(nn);
					}
				}
			}
			fifo.swap(fifoNext);
		} while (!fifo.empty());

		// make state cover divergence preserving
		for (const auto& sn : stateNodes) {
			states.clear();
			const auto& refNode = sn->convergent.front();
			for (state_t i = 0; i < stateNodes.size(); i++) {
				if ((sn->state == i) || !areNodesDifferent(refNode, stateNodes[i]->convergent.front())) {
					states.emplace(i);
				}
			}
			while (states.size() > 1) {
				auto seq = getSeparatingSequenceFromSplittingTree(fsm, sepSeq.st, sn->state, states, true);
				appendSequence(refNode, seq, ot, fsm, false);
				auto refOut = fsm->getOutputAlongPath(sn->state, seq);
				for (auto it = states.begin(); it != states.end();) {
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
							it = states.erase(it);
						}
						else ++it;
					}
					else ++it;
				}
			}
		}

		// generate convergent and init domains
		list<shared_ptr<TestNodeS>> leaves;
		generateConvergentSubtree(stateNodes[0], ot, leaves);
		for (auto& node : leaves) {
			updateDomains(node, es == 0);
		}
		return ot;
	}

	static void printTStree(const shared_ptr<TestNodeS>& node, string prefix = "") {
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
	
	static void distinguish(const shared_ptr<ConvergentNodeS>& cn, const list<shared_ptr<ConvergentNodeS>>& nodes,
			const unique_ptr<DFSM>& fsm, const SeparatingSequencesInfo& sepSeq, const OTreeS& ot) {
		list<shared_ptr<ConvergentNodeS>> domain;
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
			set<pair<state_t, ConvergentNodeS*>> closed;
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

	static void distinguishCNs(const shared_ptr<ConvergentNodeS>& cn, const shared_ptr<ConvergentNodeS>& refCN,
		list<shared_ptr<ConvergentNodeS>>& nodes, const unique_ptr<DFSM>& fsm, const SeparatingSequencesInfo& sepSeq,
			int depth, const OTreeS& ot) {
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

	static sequence_set_t getSequences(const shared_ptr<TestNodeS>& node, const unique_ptr<DFSM>& fsm) {
		sequence_set_t TS;
		stack<pair<shared_ptr<TestNodeS>, sequence_in_t>> lifo;
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
			if (!hasSucc) {
				TS.emplace(move(p.second));
			}
		}
		return TS;
	}

	sequence_set_t S_method(const unique_ptr<DFSM>& fsm, int extraStates) {
		RETURN_IF_UNREDUCED(fsm, "FSMtesting::S_method", sequence_set_t());
		if (extraStates < 0) {
			return sequence_set_t();
		}
		if (fsm->getNumberOfStates() == 1) {
			return getTraversalSet(fsm, extraStates);
		}
		SeparatingSequencesInfo sepSeq;
		sepSeq.st = getSplittingTree(fsm, true, false);
		//sepSeq.sepSeq = getSeparatingSequences(fsm);

		auto ot = getDivergencePreservingStateCover(fsm, sepSeq, extraStates);
		// stateNodes are initialized divergence-preserving state cover
		auto& stateNodes = ot.rn;

		auto travSeqs = getLongestTraversalSequences(fsm->getNumberOfInputs(), extraStates);

		using tran_t = tuple<shared_ptr<ConvergentNodeS>, input_t, shared_ptr<ConvergentNodeS>>;
		list<tran_t> transitions;
		for (const auto& sn : stateNodes) {
			for (input_t i = 0; i < sn->next.size(); i++) {
				if ((!sn->next[i] || !sn->next[i]->isRN) &&	(fsm->getNextState(sn->state, i) != NULL_STATE)) {
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
			list<shared_ptr<ConvergentNodeS>> tmp;
			distinguishCNs(ncn, proveConvergence ? nextStateCN : nullptr, tmp, fsm, sepSeq, extraStates, ot);

			if (proveConvergence && (startCN->next[input] != nextStateCN)) {// already merged in case of no ES
				startCN->next[input] = nextStateCN;
				mergeCN(ncn, nextStateCN, tmp, ot.es == 0);
				processIdentified(tmp, ot.es == 0);
			}			
		}		
		// obtain TS
		//printTStree(root);
		for (auto& sn : stateNodes) {
			sn->next.clear();
		}
		return getSequences(stateNodes[0]->convergent.front(), fsm);
	}
}

/*
	static void appendSequence(shared_ptr<TestNodeS> node, const sequence_in_t& seq, const unique_ptr<DFSM>& fsm) {
		auto it = seq.begin();
		for (; it != seq.end(); ++it) {
			if (*it == STOUT_INPUT) continue;
			if (node->next[*it]) {
				node = node->next[*it];
			}
			else break;
		}
		for (; it != seq.end(); ++it) {
			if (*it == STOUT_INPUT) continue;
			const auto& input = *it;
			state_t state = fsm->getNextState(node->state, input);
			output_t outputState = (state == NULL_STATE) ? WRONG_OUTPUT :
				(fsm->isOutputState()) ? fsm->getOutput(state, STOUT_INPUT) : DEFAULT_OUTPUT;
			output_t outputTransition = (state == NULL_STATE) ? WRONG_OUTPUT :
				(fsm->isOutputTransition()) ? fsm->getOutput(node->state, input) : DEFAULT_OUTPUT;
			auto nextNode = make_shared<TestNodeS>(node, input,
				outputTransition, outputState, state, fsm->getNumberOfInputs());
				//node->accessSequence.size() + 1, outputTransition, outputState, state, fsm->getNumberOfInputs());
			node->next[input] = nextNode;
			if (state == NULL_STATE) continue;
			node = move(nextNode);
		}
	}

	static inline bool isLeaf(const shared_ptr<TestNodeS>& node) {
		for (const auto& nn : node->next) {
			if (nn) {
				return false;
			}
		}
		return true;
	}

	static inline bool hasLeaf(const shared_ptr<ConvergentNodeS>& cn) {
		for (const auto& n : cn->convergent) {
			if (isLeaf(n)) {
				return true;
			}
		}
		return false;
	}

	static seq_len_t getLenCost(const shared_ptr<ConvergentNodeS>& cn, const sequence_in_t& seq) {
		seq_len_t minCost = seq.size() + cn->convergent.front()->accessSequence.size() + 1;
		auto cost = seq.size() + 1;
		auto currCN = cn;
		for (auto input : seq) {
			cost--;
			if (input == STOUT_INPUT) continue;
			for (const auto& node : currCN->convergent) {
				if (isLeaf(node)) {
					minCost = cost;
					break;
				}
			}
			if (currCN->next[input]) {
				currCN = currCN->next[input];
			}
			else {
				currCN = nullptr;
				break;
			}
		}
		if (currCN) {
			minCost = 0;
		}
		return minCost;
	}

	static void mapNodeToCN(shared_ptr<TestNodeS> node, shared_ptr<ConvergentNodeS> cn, 
			const sequence_in_t& seq, const set<ConvergentNodeS*>& allStatesDomain) {
		for (auto input : seq) {
			if (input == STOUT_INPUT) continue;
			node = node->next[input];
			if (!cn->next[input]) {
				cn->next[input] = make_shared<ConvergentNodeS>(node);
				cn->next[input]->domain = allStatesDomain;
			}
			else {
				auto& ncn = cn->next[input]->convergent;
				if (node->accessSequence.size() < ncn.front()->accessSequence.size()) {
					ncn.emplace_front(node);
				}
				else {
					ncn.emplace_back(node);
				}
			}
			cn = cn->next[input];
			node->convergentNode = cn;
		}
	}

	static void addSequence(const shared_ptr<ConvergentNodeS>& cn, sequence_in_t seq, 
			const unique_ptr<DFSM>& fsm, const set<ConvergentNodeS*>& allStatesDomain) {
		auto cost = getLenCost(cn, seq);
		if (cost > seq.size()) {
			appendSequence(cn->convergent.front(), seq, fsm);
			mapNodeToCN(cn->convergent.front(), cn, seq, allStatesDomain);
		}
		else if (cost > 0) {
			auto currCN = cn;
			while (cost != seq.size()) {
				if (seq.front() != STOUT_INPUT) {
					currCN = currCN->next[seq.front()];
				}
				seq.pop_front();
			}
			for (const auto& node : currCN->convergent) {
				if (isLeaf(node)) {
					appendSequence(node, seq, fsm);
					mapNodeToCN(node, currCN, seq, allStatesDomain);
					currCN = nullptr;// for test purposes
					break;
				}
			}
			if (currCN) {
				throw "";
			}
		}
	}

	typedef pair<seq_len_t, state_t> cost_t;// (num of needed symbols, num of distinguished)

	static inline bool cmpCosts(const cost_t& c1, const cost_t& c2) {
		if ((c1.second == 0) || (c2.second == 0))
			return (c1.second != 0) || ((c2.second == 0) && (c1.first < c2.first));
		if (c1.second >= c2.second)
			return (c1.first < (c1.second - c2.second + 1) * c2.first);
		return ((c2.second - c1.second + 1) * c1.first < c2.first);
			//((c1.second > c2.second) || ((c1.second == c2.second) && (c1.first < c2.first)));
			//((c1.second >= c2.second) && ((c2.second == 0) || (c1.first < (c1.second - c2.second + 1) * c2.first))) ||
			//((c1.second < c2.second) && (c1.second != 0) && ((c2.second - c1.second + 1) * c1.first < c2.first));
			//(c1.first + c2.second < c1.second + c2.first) && ((c1.first <= c2.first) || (c1.second >= c2.second)));
	}

	static inline bool hasStatePairDifferentStateOutput(state_t statePairIdx, const SeparatingSequencesInfo& sepSeq, const unique_ptr<DFSM>& fsm) {
		if (sepSeq.sepSeq[statePairIdx].minLen != 1) return false;
		auto p = getStatesOfStatePairIdx(statePairIdx);
		return (fsm->getOutput(p.first, STOUT_INPUT) != fsm->getOutput(p.second, STOUT_INPUT));
	}

	static sequence_in_t getShortestSepSeq(state_t statePairIdx, const SeparatingSequencesInfo& sepSeq, const unique_ptr<DFSM>& fsm) {
		sequence_in_t seq;
		input_t P = fsm->getNumberOfInputs();
		while (true) {
			if (fsm->isOutputState()) {
				seq.push_back(STOUT_INPUT);
				if (hasStatePairDifferentStateOutput(statePairIdx, sepSeq, fsm)) {
					return seq;
				}
			}
			for (input_t i = 0; i < P; i++) {
				const auto& nextIdx = sepSeq.sepSeq[statePairIdx].next[i];
				if (nextIdx == NULL_STATE) continue;
				if (nextIdx == statePairIdx) {
					seq.push_back(i);
					return seq;
				}
				if (sepSeq.sepSeq[nextIdx].minLen == sepSeq.sepSeq[statePairIdx].minLen - 1) {
					seq.push_back(i);
					statePairIdx = nextIdx;
					break;
				}
			}
		}
	}

	static sequence_in_t getShortestSepSeq(state_t state, set<state_t>& diffStates,
		SeparatingSequencesInfo& sepSeq, const unique_ptr<DFSM>& fsm) {
		set<state_t> statePairs;
		for (const auto& s : diffStates) {
			statePairs.insert(getStatePairIdx(state, s));
		}
		auto it = sepSeq.bestSeq.find(statePairs);
		if (it != sepSeq.bestSeq.end()) {
			return it->second;
		}
		if (statePairs.size() == 1) {
			auto seq = getShortestSepSeq(*statePairs.begin(), sepSeq, fsm);
			sepSeq.bestSeq.emplace(move(statePairs), seq);
			return seq;
		}
		cost_t minCost(seq_len_t(fsm->getNumberOfStates()), 0);// max separating sequence and no distinguished states
		sequence_in_t bestSepSeq;
		using seqTuple = tuple<cost_t, sequence_in_t, set<state_t>>;
		auto tupleComp = [](const seqTuple& e1, const seqTuple& e2) {
			auto gt = cmpCosts(get<0>(e2), get<0>(e1));
			if (!cmpCosts(get<0>(e1), get<0>(e2)) && !gt) {// equal
				if (get<1>(e1).size() == get<1>(e2).size())
					return get<1>(e1) > get<1>(e2);
				return get<1>(e1).size() > get<1>(e2).size();
			}
			return gt; 
		};
		priority_queue<seqTuple, vector<seqTuple>, decltype(tupleComp)> pq(tupleComp);
		pq.emplace(cost_t(0, state_t(statePairs.size())), sequence_in_t(), statePairs);
		while (!pq.empty()) {
			auto p = move(pq.top());
			pq.pop();
			if (!cmpCosts(get<0>(p), minCost)) {
				break;
			}
			auto currStatePairs = move(get<2>(p));
			if (fsm->isOutputState()) {
				for (auto it = currStatePairs.begin(); it != currStatePairs.end();) {
					if (hasStatePairDifferentStateOutput(*it, sepSeq, fsm)) {
						it = currStatePairs.erase(it);
					}
					else ++it;
				}
				auto it = sepSeq.bestSeq.find(currStatePairs);
				if ((it != sepSeq.bestSeq.end()) || (currStatePairs.size() == 1))  {
					sequence_in_t seq = (it != sepSeq.bestSeq.end()) ? it->second : getShortestSepSeq(*currStatePairs.begin(), sepSeq, fsm);
					if (it == sepSeq.bestSeq.end()) sepSeq.bestSeq.emplace(move(currStatePairs), seq);
					get<0>(p).first = get<1>(p).size() + seq.size();
					if (cmpCosts(get<0>(p), minCost)) {
						minCost.swap(get<0>(p));
						bestSepSeq = move(get<1>(p));
						bestSepSeq.splice(bestSepSeq.end(), move(seq));
					}
					continue;
				}
				get<1>(p).emplace_back(STOUT_INPUT);
			}
			for (input_t i = 0; i < fsm->getNumberOfInputs(); i++) {
				set<state_t> nextStatePairs;
				auto cost = get<0>(p);
				cost.first = 0;
				for (const auto& sp : currStatePairs) {
					auto& idx = sepSeq.sepSeq[sp].next[i];
					if (idx == NULL_STATE) {
						cost.second--;
					}
					else if (idx != sp) {// not distinguished
						nextStatePairs.insert(idx);
						if (cost.first < sepSeq.sepSeq[idx].minLen) {
							cost.first = sepSeq.sepSeq[idx].minLen;
						}
					}
				}
				sequence_in_t seq(get<1>(p));
				seq.push_back(i);
				auto it = sepSeq.bestSeq.find(nextStatePairs);
				bool isCached = (it != sepSeq.bestSeq.end());
				if (isCached || (nextStatePairs.size() <= 1)) {
					if (isCached || nextStatePairs.size() == 1) {
						sequence_in_t suffix = isCached ? it->second : getShortestSepSeq(*nextStatePairs.begin(), sepSeq, fsm);
						if (!isCached) sepSeq.bestSeq.emplace(move(nextStatePairs), suffix);
						seq.splice(seq.end(), move(suffix));
					}
					cost.first = seq.size();
					if (cmpCosts(cost, minCost)) {
						minCost.swap(cost);
						bestSepSeq.swap(seq);
					}
				}
				else {
					cost.first += seq.size();
					if (cmpCosts(cost, minCost)) {
						pq.emplace(move(cost), move(seq), move(nextStatePairs));
					}
				}
			}
		}
		sepSeq.bestSeq.emplace(move(statePairs), bestSepSeq);
		return bestSepSeq;
	}

	static vector<shared_ptr<ConvergentNodeS>> createDivPresStateCoverTree(const unique_ptr<DFSM>& fsm, 
		const vector<sequence_in_t>& ADSet, SeparatingSequencesInfo& sepSeq) {
		vector<shared_ptr<ConvergentNodeS>> stateNodes(fsm->getNumberOfStates());
		// root
		output_t outputState = (fsm->isOutputState()) ? fsm->getOutput(0, STOUT_INPUT) : DEFAULT_OUTPUT;
		auto root = make_shared<TestNodeS>(outputState, 0, fsm->getNumberOfInputs());
		if (ADSet.empty()) {
			set<state_t> diffStates;
			for (state_t s = 1; s < fsm->getNumberOfStates(); s++) {
				diffStates.insert(s);
			}
			auto seq = getShortestSepSeq(0, diffStates, sepSeq, fsm);
			appendSequence(root, seq, fsm);
		}
		else {
			appendSequence(root, ADSet[0], fsm);
		}
		stateNodes[0] = make_shared<ConvergentNodeS>(root);
		root->convergentNode = stateNodes[0];
		queue<shared_ptr<TestNodeS>> fifo, fifoNext;
		fifo.emplace(root);
		do {
			auto numNodes = fifo.size();
			for (size_t i = 0; i < numNodes; i++) {
				auto node = move(fifo.front());
				fifo.pop();
				for (input_t input = 0; input < fsm->getNumberOfInputs(); input++) {
					const auto& nn = node->next[input];
					if (nn && !stateNodes[nn->state]) {
						stateNodes[nn->state] = make_shared<ConvergentNodeS>(nn);
						nn->convergentNode = stateNodes[nn->state];
						stateNodes[node->state]->next[input] = stateNodes[nn->state];
						if (ADSet.empty()) {
							set<state_t> diffStates;
							for (state_t s = 0; s < fsm->getNumberOfStates(); s++) {
								if ((nn->state != s) && (!stateNodes[s] || !areNodesDifferent(nn, stateNodes[s]->convergent.front()))) {
									diffStates.insert(s);
								}
							}
							auto seq = getShortestSepSeq(nn->state, diffStates, sepSeq, fsm);
							appendSequence(root, seq, fsm);
						}
						else {
							appendSequence(nn, ADSet[nn->state], fsm);
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
						appendSequence(node, sequence_in_t({input}), fsm);
						auto& nn = node->next[input];
						stateNodes[nn->state] = make_shared<ConvergentNodeS>(nn);
						nn->convergentNode = stateNodes[nn->state];
						stateNodes[node->state]->next[input] = stateNodes[nn->state];
						if (ADSet.empty()) {
							set<state_t> diffStates;
							for (state_t s = 0; s < fsm->getNumberOfStates(); s++) {
								if ((nn->state != s) && (!stateNodes[s] || !areNodesDifferent(nn, stateNodes[s]->convergent.front()))) {
									diffStates.insert(s);
								}
							}
							auto seq = getShortestSepSeq(nn->state, diffStates, sepSeq, fsm);
							appendSequence(root, seq, fsm);
						}
						else {
							appendSequence(nn, ADSet[nextState], fsm);
						}
						fifoNext.emplace(nn);
					}
				}
			}
			fifo.swap(fifoNext);
		} while (!fifo.empty());
		return stateNodes;
	}

	static void generateConvergentSubtree(const shared_ptr<ConvergentNodeS>& cn, const set<ConvergentNodeS*>& allStatesDomain) {
		const auto& node = cn->convergent.front();
		node->convergentNode = cn;
		cn->domain = allStatesDomain;
		for (input_t input = 0; input < cn->next.size(); input++) {
			if (node->next[input]) {
				if (!node->next[input]->convergentNode.lock()) {// not a state node
					cn->next[input] = make_shared<ConvergentNodeS>(node->next[input]);
				}
				generateConvergentSubtree(cn->next[input], allStatesDomain);
			}
		}
	}

	static size_t checkSucc(const shared_ptr<ConvergentNodeS>& cn, const vector<shared_ptr<ConvergentNodeS>>& stateNodes, seq_len_t depth) {
		size_t cost = (cn->domain.size() != 1);
		if (depth > 0) {
			for (input_t i = 0; i < cn->next.size(); i++) {
				if (cn->next[i]) {
					cost += checkSucc(cn->next[i], stateNodes, depth - 1);
				}
				else {
					cost += size_t((1 - pow(cn->next.size(), depth)) / (1 - cn->next.size()));
				}
			}
		}
		return cost;
	}

	static void chooseTransition2(state_t& state, input_t& input, const vector<shared_ptr<ConvergentNodeS>>& stateNodes, 
			const unique_ptr<DFSM>& fsm, seq_len_t extraStates) {
		size_t minCost = size_t(-1);
		for (const auto& cn : stateNodes) {
			for (input_t i = 0; i < cn->next.size(); i++) {
				if (cn->next[i]) {
					if (stateNodes[cn->next[i]->state] != cn->next[i]) {
						auto cost = checkSucc(cn->next[i], stateNodes, extraStates);
						if (minCost > cost) {
							minCost = cost;
							state = cn->state;
							input = i;
						}
					}
				}
				else {
					auto cost = 1 + size_t(((1 - pow(cn->next.size(), extraStates)) / (1 - cn->next.size())));
					if (minCost > cost) {
						minCost = cost;
						state = cn->state;
						input = i;
					}
				}
			}
		}
	}

	
	static void calcCost(cost_t& cost, const sequence_in_t& seq, const sequence_out_t& refOut,
			const list<shared_ptr<ConvergentNodeS>>& nodes, const unique_ptr<DFSM>& fsm) {
		for (const auto& n : nodes) {
			auto out = fsm->getOutputAlongPath(n->convergent.front()->state, seq);
			auto outIt = out.begin();
			auto refOutIt = refOut.begin();
			auto inIt = seq.begin();
			while ((refOutIt != refOut.end()) && (*outIt == *refOutIt)) {
				++outIt;
				++refOutIt;
				++inIt;
			}
			if ((refOutIt == refOut.end()) || (*outIt == WRONG_OUTPUT)) {
				cost.second--;
			}
			else {
				++inIt;
				sequence_in_t diffSeq(seq.begin(), inIt);
				cost.first += getLenCost(n, diffSeq);
			}
		}
	}

	static sequence_in_t chooseSepSeqAsExtension(const shared_ptr<ConvergentNodeS>& cn, cost_t& minCost, cost_t& currCost,
		const list<shared_ptr<ConvergentNodeS>>& nodes, const list<cost_t>& extNodes,
		const unique_ptr<DFSM>& fsm, SeparatingSequencesInfo& sepSeq) {

		sequence_in_t bestSeq;
		for (const auto& node : cn->convergent) {
			if (isLeaf(node)) {
				set<state_t> diffStates;
				for (const auto& n : nodes) {
					diffStates.insert(n->state);
				}
				for (const auto& p : extNodes) {
					diffStates.insert(p.second);
				}
				auto seq = getShortestSepSeq(node->state, diffStates, sepSeq, fsm);
				auto refOut = fsm->getOutputAlongPath(node->state, seq);
				cost_t cost = currCost;
				cost.first += seq.size();// getLenCost(cn, seq);
				calcCost(cost, seq, refOut, nodes, fsm);
				for (const auto& p : extNodes) {
					auto out = fsm->getOutputAlongPath(p.second, seq);
					auto outIt = out.begin();
					auto refOutIt = refOut.begin();
					auto inIt = seq.begin();
					while ((refOutIt != refOut.end()) && (*outIt == *refOutIt)) {
						++outIt;
						++refOutIt;
						++inIt;
					}
					if ((refOutIt == refOut.end()) || (*outIt == WRONG_OUTPUT)) {
						cost.second--;
					}
					else {
						++inIt;
						cost.first += p.first + distance(seq.begin(), inIt);
					}
				}
				if (cmpCosts(cost, minCost)) {
					minCost.swap(cost);
					bestSeq.swap(seq);
				}
				break;
			}
		}
		for (input_t i = 0; i < fsm->getNumberOfInputs(); i++) {
			if (cn->next[i]) {
				auto& nextState = cn->next[i]->state;
				auto refOut = fsm->getOutput(cn->state, i);
				list<shared_ptr<ConvergentNodeS>> succNodes;
				list<cost_t> succExtNodes;
				cost_t succCost = currCost;// (cost so far, distinguished)
				seq_len_t estCost = 0;
				for (const auto& n : nodes) {
					if (n->next[i] && (n->next[i]->state != nextState)) {
						succNodes.emplace_back(n->next[i]);
					}
					else {
						auto succState = fsm->getNextState(n->state, i);
						if (succState == NULL_STATE) {
							succCost.second--;
						}
						else {
							auto succOut = fsm->getOutput(n->state, i);
							if ((refOut != succOut) || (fsm->isOutputState() && 
								(fsm->getOutput(nextState, STOUT_INPUT) != fsm->getOutput(succState, STOUT_INPUT)))) {
								succCost.first += (hasLeaf(n) ? 1 : (n->convergent.front()->accessSequence.size() + 1));
							}
							else if (succState == nextState) {
								succCost.second--;
							}
							else {
								succExtNodes.emplace_back((hasLeaf(n) ? 1 : (n->convergent.front()->accessSequence.size() + 1)), succState);
								estCost += succExtNodes.back().first;
							}
						}
					}
				}
				for (const auto& p : extNodes) {
					auto succState = fsm->getNextState(p.second, i);
					if (succState == NULL_STATE) {
						succCost.second--;
					}
					else {
						auto succOut = fsm->getOutput(p.second, i);
						if ((refOut != succOut) || (fsm->isOutputState() &&
							(fsm->getOutput(nextState, STOUT_INPUT) != fsm->getOutput(succState, STOUT_INPUT)))) {
							succCost.first += p.first + 1;
						}
						else if (succState == nextState) {
							succCost.second--;
						}
						else {
							succExtNodes.emplace_back(p.first + 1, succState);
							estCost += p.first + 1;
						}
					}
				}
				succCost.first += estCost;
				if (((succNodes.size() + succExtNodes.size()) != 0) && !cmpCosts(minCost, succCost)) {
					succCost.first -= estCost;
					auto succMinCost = minCost;
					auto succSeq = chooseSepSeqAsExtension(cn->next[i], succMinCost, succCost, succNodes, succExtNodes, fsm, sepSeq);
					if (cmpCosts(succMinCost, minCost)) {
						minCost.swap(succMinCost);
						bestSeq.swap(succSeq);
						bestSeq.push_front(i);
					}
				}
			}
		}
		return bestSeq;
	}

	static void chooseTransition(state_t& state, input_t& input, const vector<shared_ptr<ConvergentNodeS>>& stateNodes,
			const unique_ptr<DFSM>& fsm, seq_len_t extraStates) {
		size_t minCost = size_t(-1);
		for (const auto& cn : stateNodes) {
			const auto& cost = cn->convergent.front()->accessSequence.size();
			for (input_t i = 0; i < cn->next.size(); i++) {
				if (!cn->next[i] || (stateNodes[cn->next[i]->state] != cn->next[i])) {
					auto nextState = fsm->getNextState(cn->state, i);
					if (nextState == NULL_STATE) continue;
					if (minCost > cost + stateNodes[nextState]->convergent.front()->accessSequence.size()) {
						minCost = cost + stateNodes[nextState]->convergent.front()->accessSequence.size();
						state = cn->state;
						input = i;
					}
				}
			}
		}
	}
	*/

