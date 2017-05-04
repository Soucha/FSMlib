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
		seq_len_t depth;
		output_t incomingOutput;
		output_t stateOutput;
		state_t state;
		vector<shared_ptr<TestNodeS>> next;
		weak_ptr<ConvergentNodeS> convergentNode;

		TestNodeS(seq_len_t accessSeqLen, output_t inOutput, output_t stateOutput, state_t state, input_t numberOfInputs) :
			depth(accessSeqLen), incomingOutput(inOutput), stateOutput(stateOutput), state(state), next(numberOfInputs) {
		}
	};

	struct ConvergentNodeS {
		list<shared_ptr<TestNodeS>> convergent;
		set<ConvergentNodeS*> domain;
		vector<shared_ptr<ConvergentNodeS>> next;
		state_t state;

		ConvergentNodeS(const shared_ptr<TestNodeS>& node) : state(node->state) {
			convergent.emplace_back(node);
			next.resize(node->next.size());
		}
	};

	struct SeparatingSequencesInfo {
		vector<LinkCell> sepSeq;
		map<set<state_t>, sequence_in_t> bestSeq;
	};

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

	static bool areNodesDifferent(const shared_ptr<TestNodeS>& n1, const shared_ptr<TestNodeS>& n2) {
		if (n1->stateOutput != n2->stateOutput) return true;
		for (input_t i = 0; i < n1->next.size(); i++) {
			if ((n1->next[i]) && (n2->next[i]) && ((n1->next[i]->incomingOutput != n2->next[i]->incomingOutput)
				|| areNodesDifferent(n1->next[i], n2->next[i])))
				return true;
		}
		return false;
	}

	static bool areConvergentNodesDistinguished(const shared_ptr<ConvergentNodeS>& cn1, const shared_ptr<ConvergentNodeS>& cn2) {
		if ((cn1 == cn2) || (cn1->state == cn2->state)) return false;
		if (cn1->convergent.front()->stateOutput != cn2->convergent.front()->stateOutput) return true;
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
			auto nextNode = make_shared<TestNodeS>(node->depth + 1, outputTransition, outputState, state, fsm->getNumberOfInputs());
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
		seq_len_t minCost = seq.size() + cn->convergent.front()->depth + 1;
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
				if (node->depth < ncn.front()->depth) {
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

	static inline bool hasStatePairDifferentStateOutput(state_t statePairIdx, SeparatingSequencesInfo& sepSeq, const unique_ptr<DFSM>& fsm) {
		if (sepSeq.sepSeq[statePairIdx].minLen != 1) return false;
		auto p = getStatesOfStatePairIdx(statePairIdx);
		return (fsm->getOutput(p.first, STOUT_INPUT) != fsm->getOutput(p.second, STOUT_INPUT));
	}

	static sequence_in_t getShortestSepSeq(state_t statePairIdx, SeparatingSequencesInfo& sepSeq, const unique_ptr<DFSM>& fsm) {
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
		auto root = make_shared<TestNodeS>(0, DEFAULT_OUTPUT, outputState, 0, fsm->getNumberOfInputs());
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
		/*for (auto it = cn->domain.begin(); it != cn->domain.end();) {
			if (areConvergentNodesDistinguished(cn, stateNodes[(*it)->state])) {
				it = cn->domain.erase(it);
			}
			else ++it;
		}*/
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

	static void chooseTransition(state_t& state, input_t& input, const vector<shared_ptr<ConvergentNodeS>>& stateNodes,
			const unique_ptr<DFSM>& fsm, seq_len_t extraStates) {
		size_t minCost = size_t(-1);
		for (const auto& cn : stateNodes) {
			const auto& cost = cn->convergent.front()->depth;
			for (input_t i = 0; i < cn->next.size(); i++) {
				if (!cn->next[i] || (stateNodes[cn->next[i]->state] != cn->next[i])) {
					auto nextState = fsm->getNextState(cn->state, i);
					if (nextState == NULL_STATE) continue;
					if (minCost > cost + stateNodes[nextState]->convergent.front()->depth) {
						minCost = cost + stateNodes[nextState]->convergent.front()->depth;
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
								succCost.first += (hasLeaf(n) ? 1 : (n->convergent.front()->depth + 1));
							}
							else if (succState == nextState) {
								succCost.second--;
							}
							else {
								succExtNodes.emplace_back((hasLeaf(n) ? 1 : (n->convergent.front()->depth + 1)), succState);
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

	static void distinguish(const shared_ptr<ConvergentNodeS>& cn, const list<shared_ptr<ConvergentNodeS>>& nodes,
		const unique_ptr<DFSM>& fsm, SeparatingSequencesInfo& sepSeq, const set<ConvergentNodeS*>& allStatesDomain) {
		list<shared_ptr<ConvergentNodeS>> domain;
		auto state = cn->state;
		//auto refCN = cn;
		for (auto dIt = cn->domain.begin(); dIt != cn->domain.end();) {
			const auto& node = (*dIt)->convergent.front();
			if (state == node->state) {
				//refCN = node->convergentNode.lock();
				++dIt;
			}
			else if (areConvergentNodesDistinguished(node->convergentNode.lock(), cn))  {
				dIt = cn->domain.erase(dIt);
			} else {
				domain.emplace_back(node->convergentNode.lock());
				++dIt;
			}
		}
		for (const auto& np : nodes) {
			const auto& node = np->convergent.front();
			if ((state != node->state) && !areConvergentNodesDistinguished(np, cn))  {
				domain.emplace_back(np);
			}
		}
		while (!domain.empty()) {
			set<state_t> diffStates;
			for (const auto& n : domain) {
				diffStates.insert(n->state);
			}
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
			}
			addSequence(cn, seq, fsm, allStatesDomain);
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
					addSequence((*dIt), diffSeq, fsm, allStatesDomain);
					cn->domain.erase((*dIt).get());
					dIt = domain.erase(dIt);
				}
				else {
					++dIt;
				}
			}
			for (auto dIt = domain.begin(); dIt != domain.end();) {
				if (areConvergentNodesDistinguished(cn, *dIt)) {
					dIt = domain.erase(dIt);
					cn->domain.erase((*dIt).get());
				}
				else {
					++dIt;
				}
			}
		}
	}

	static void distinguishCNs(const shared_ptr<ConvergentNodeS>& cn, const shared_ptr<ConvergentNodeS>& refCN,
		list<shared_ptr<ConvergentNodeS>>& nodes, const unique_ptr<DFSM>& fsm, SeparatingSequencesInfo& sepSeq,
			int depth, const set<ConvergentNodeS*>& allStatesDomain) {
		distinguish(cn, nodes, fsm, sepSeq, allStatesDomain);
		distinguish(refCN, nodes, fsm, sepSeq, allStatesDomain);
		if (depth > 0) {
			nodes.emplace_back(cn);
			if (!allStatesDomain.count(refCN.get())) nodes.emplace_back(refCN);
			for (input_t i = 0; i < fsm->getNumberOfInputs(); i++) {
				if (!cn->next[i]) {
					addSequence(cn, sequence_in_t({ i }), fsm, allStatesDomain);
				}
				if (!refCN->next[i]) {
					addSequence(refCN, sequence_in_t({ i }), fsm, allStatesDomain);
				}
				distinguishCNs(cn->next[i], refCN->next[i], nodes, fsm, sepSeq, depth - 1, allStatesDomain);
			}
			if (!allStatesDomain.count(refCN.get())) nodes.pop_back();
			nodes.pop_back();
		}
	}

	static void mergeCN(const shared_ptr<ConvergentNodeS>& fromCN, const shared_ptr<ConvergentNodeS>& toCN) {
		if (fromCN->convergent.front()->depth < toCN->convergent.front()->depth) {
			toCN->convergent.emplace_front(move(fromCN->convergent.front()));
			fromCN->convergent.pop_front();
			toCN->convergent.front()->convergentNode = toCN;
		}
		for (auto&& n : fromCN->convergent) {
			n->convergentNode = toCN;
			toCN->convergent.emplace_back(n);
		}
		fromCN->convergent.clear();
		for (auto toIt = toCN->domain.begin(); toIt != toCN->domain.end();) {
			if (!fromCN->domain.count(*toIt)) {
				toIt = toCN->domain.erase(toIt);
			}
			else ++toIt;
		}
		fromCN->domain.clear();
		for (input_t i = 0; i < fromCN->next.size(); i++) {
			if (fromCN->next[i]) {
				if (toCN->next[i]) {
					mergeCN(fromCN->next[i], toCN->next[i]);
				}
				else {
					toCN->next[i] = fromCN->next[i];
				}
			}
		}
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
		RETURN_IF_UNREDUCED(fsm, "FSMtesting::SPYH_method", sequence_set_t());
		if (extraStates < 0) {
			return sequence_set_t();
		}
		if (fsm->getNumberOfStates() == 1) {
			return getTraversalSet(fsm, extraStates);
		}
		
		auto ADSet = getAdaptiveDistinguishingSet(fsm, true);
		SeparatingSequencesInfo sepSeq;
		sepSeq.sepSeq = getSeparatingSequences(fsm);
		auto stateNodes = createDivPresStateCoverTree(fsm, ADSet, sepSeq);
		set<ConvergentNodeS*> allStatesDomain;
		for (const auto& sn : stateNodes) {
			allStatesDomain.insert(sn.get());
		}
		generateConvergentSubtree(stateNodes[0], allStatesDomain);
		if (ADSet.empty()) {
			list<shared_ptr<ConvergentNodeS>> tmp;
			for (const auto& sn : stateNodes) {
				distinguish(sn, tmp, fsm, sepSeq, allStatesDomain);
			}
		}
		else {
			for (const auto& sn : stateNodes) {
				sn->domain.clear();
				sn->domain.insert(sn.get());
			}
		}
		// stateNodes are initialized divergence-preserving state cover

		// confirm all transitions -> convergence-preserving transition cover
		state_t state = 0;
		input_t input = 0;
		while (true) {
			// choose transition to verify
			state = NULL_STATE;
			chooseTransition(state, input, stateNodes, fsm, extraStates);
			if (state == NULL_STATE) break;// all transitions confirmed

			// identify next state
			auto ncn = stateNodes[state]->next[input];
			if (!ncn) {
				addSequence(stateNodes[state], sequence_in_t({ input }), fsm, allStatesDomain);
				ncn = stateNodes[state]->next[input];
			}
			const auto& nextStateCN = stateNodes[ncn->state];
			// TODO domain reduction if ES==0
			list<shared_ptr<ConvergentNodeS>> tmp;
			distinguishCNs(ncn, nextStateCN, tmp, fsm, sepSeq, extraStates, allStatesDomain);
			
			stateNodes[state]->next[input] = nextStateCN;
			mergeCN(ncn, nextStateCN);
		}		
		// obtain TS
		//printTStree(root);
		for (auto& sn : stateNodes) {
			sn->next.clear();
		}
		return getSequences(stateNodes[0]->convergent.front(), fsm);
	}
}
