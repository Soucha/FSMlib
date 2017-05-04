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

	struct ConvergentNodeSPYH;

	struct TestNodeSPYH {
		seq_len_t depth;
		output_t incomingOutput;
		output_t stateOutput;
		state_t state;
		vector<shared_ptr<TestNodeSPYH>> next;

		TestNodeSPYH(seq_len_t accessSeqLen, output_t inOutput, output_t stateOutput, state_t state, input_t numberOfInputs) :
			depth(accessSeqLen), incomingOutput(inOutput), stateOutput(stateOutput), state(state), next(numberOfInputs) {
		}
	};

	struct ConvergentNodeSPYH {
		list<shared_ptr<TestNodeSPYH>> convergent;
		vector<shared_ptr<ConvergentNodeSPYH>> next;
		state_t state;
		input_t distinguishingInput;
		bool isReferenceNode = false;

		ConvergentNodeSPYH(const shared_ptr<TestNodeSPYH>& node) : state(node->state) {
			convergent.emplace_back(node);
			next.resize(node->next.size());
		}
	};

	static void printTStree(const shared_ptr<TestNodeSPYH>& node, string prefix = "") {
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
	
	static shared_ptr<ConvergentNodeSPYH> appendSequence(shared_ptr<ConvergentNodeSPYH> cn, shared_ptr<TestNodeSPYH> node, const sequence_in_t& seq, const unique_ptr<DFSM>& fsm) {
		auto it = seq.begin();
		for (; it != seq.end(); ++it) {
			if (*it == STOUT_INPUT) continue;
			if (node->next[*it]) {
				node = node->next[*it];
				cn = cn->next[*it];
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
			auto nextNode = make_shared<TestNodeSPYH>(node->depth + 1, outputTransition, outputState, state, fsm->getNumberOfInputs());
			node->next[input] = nextNode;
			node = move(nextNode);
			if (!cn->next[input]) {
				cn->next[input] = make_shared<ConvergentNodeSPYH>(node);
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
		}
		return cn;
	}

	static inline bool isLeaf(const shared_ptr<TestNodeSPYH>& node) {
		for (const auto& nn : node->next) {
			if (nn) {
				return false;
			}
		}
		return true;
	}

	static inline bool hasLeaf(const shared_ptr<ConvergentNodeSPYH>& cn) {
		for (const auto& n : cn->convergent) {
			if (isLeaf(n)) {
				return true;
			}
		}
		return false;
	}

	static seq_len_t getLenCost(const shared_ptr<ConvergentNodeSPYH>& cn, const sequence_in_t& seq) {
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

	static void addSequence(const shared_ptr<ConvergentNodeSPYH>& cn, sequence_in_t seq, const unique_ptr<DFSM>& fsm) {
		auto cost = getLenCost(cn, seq);
		if (cost > seq.size()) {
			appendSequence(cn, cn->convergent.front(), seq, fsm);
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
					appendSequence(currCN, node, seq, fsm);
					currCN = nullptr;// for test purposes
					break;
				}
			}
			if (currCN) {
				throw "";
			}
		}
	}

	static sequence_in_t getShortestSepSeq(state_t s1, state_t s2, const vector<LinkCell>& sepSeq, const unique_ptr<DFSM>& fsm) {
		sequence_in_t seq;
		const input_t P = fsm->getNumberOfInputs();
		auto statePairIdx = getStatePairIdx(s1, s2);
		while (true) {
			if (fsm->isOutputState()) {
				seq.push_back(STOUT_INPUT);
				if (fsm->getOutput(s1, STOUT_INPUT) != fsm->getOutput(s2, STOUT_INPUT)) {
					return seq;
				}
			}
			for (input_t i = 0; i < P; i++) {
				const auto& nextIdx = sepSeq[statePairIdx].next[i];
				if (nextIdx == NULL_STATE) continue;
				if (nextIdx == statePairIdx) {
					seq.push_back(i);
					return seq;
				}
				if (sepSeq[nextIdx].minLen == sepSeq[statePairIdx].minLen - 1) {
					seq.push_back(i);
					statePairIdx = nextIdx;
					s1 = fsm->getNextState(s1, i);
					s2 = fsm->getNextState(s2, i);
					break;
				}
			}
		}
	}

	static seq_len_t getEstimate(const state_t& idx, const input_t& input, const vector<LinkCell>& sepSeq) {
		auto nextIdx = sepSeq[idx].next[input];
		if (nextIdx == NULL_STATE) return seq_len_t(-1);
		if (nextIdx == idx) return 1;
		return 2 * sepSeq[nextIdx].minLen + 1;
	}

	static seq_len_t getMinLenToDistinguish(const shared_ptr<ConvergentNodeSPYH>& cn1, const shared_ptr<ConvergentNodeSPYH>& cn2,
			const vector<LinkCell>& sepSeq) {
		auto spIdx = getStatePairIdx(cn1->state, cn2->state);
		seq_len_t minVal = 2 * sepSeq[spIdx].minLen;
		bool hasLeaf1 = hasLeaf(cn1);
		bool hasLeaf2 = hasLeaf(cn2);
		if (!hasLeaf1) minVal += cn1->convergent.front()->depth;
		if (!hasLeaf2) minVal += cn2->convergent.front()->depth;
		auto& input = cn1->distinguishingInput;
		input = STOUT_INPUT;
		for (input_t i = 0; i < cn1->next.size(); i++) {
			if (cn1->next[i]) {
				if (cn2->next[i]) {
					if ((spIdx == sepSeq[spIdx].next[i]) ||
						(cn1->next[i]->convergent.front()->stateOutput != cn2->next[i]->convergent.front()->stateOutput)) {
						return 0; //distinguished
					}
					if (cn1->next[i]->state == cn2->next[i]->state) continue;
					auto est = getMinLenToDistinguish(cn1->next[i], cn2->next[i], sepSeq);
					if (est == 0) return 0; //distinguished
					if (minVal >= est) {
						minVal = est;
						input = i;
					}
				}
				else {
					auto est = getEstimate(spIdx, i, sepSeq);
					if (est != seq_len_t(-1)) {
						if (est != 1) {
							if (hasLeaf1) est++;
							else if (!hasLeaf(cn1->next[i])) est += cn1->next[i]->convergent.front()->depth;
						}
						if (!hasLeaf2) est += cn2->convergent.front()->depth;
						if (minVal > est) {
							minVal = est;
							input = i;
						}
					}
				}
			}
			else if (cn2->next[i]) {
				auto est = getEstimate(spIdx, i, sepSeq);
				if (est != seq_len_t(-1)) {
					if (est != 1) {
						if (hasLeaf2) est++;
						else if (!hasLeaf(cn2->next[i])) est += cn2->next[i]->convergent.front()->depth;
					}
					if (!hasLeaf1) est += cn1->convergent.front()->depth;
					if (minVal > est) {
						minVal = est;
						input = i;
					}
				}
			}
		}
		return minVal;
	}

	static void distinguish(const shared_ptr<ConvergentNodeSPYH>& cn, const vector<shared_ptr<ConvergentNodeSPYH>>& nodes,
		const vector<LinkCell>& sepSeq, const unique_ptr<DFSM>& fsm) {

		for (const auto& n : nodes) {
			if ((n != cn) && (n->state != cn->state) && (n->convergent.front()->stateOutput == cn->convergent.front()->stateOutput)
					&& (getMinLenToDistinguish(cn, n, sepSeq) > 0)) {
				auto cn1 = cn;
				auto cn2 = n;
				sequence_in_t seq;
				while (cn1->distinguishingInput != STOUT_INPUT) {
					const auto& input = cn1->distinguishingInput;
					seq.emplace_back(input);
					if (!cn1->next[input]) {
						auto it2 = cn2->convergent.begin();
						while (!(*it2)->next[input]) {
							++it2;
						}
						if (fsm->getOutput(cn1->state, input) == (*it2)->next[input]->incomingOutput) {
							seq.splice(seq.end(), getShortestSepSeq(fsm->getNextState(cn1->state, input),
								cn2->next[input]->state, sepSeq, fsm));
						}
						break;
					}
					if (!cn2->next[input]) {
						auto it1 = cn1->convergent.begin();
						while (!(*it1)->next[input]) {
							++it1;
						}
						if ((*it1)->next[input]->incomingOutput == fsm->getOutput(cn2->state, input)) {
							seq.splice(seq.end(), getShortestSepSeq(cn1->next[input]->state,
								fsm->getNextState(cn2->state, input), sepSeq, fsm));
						}
						break;
					}
					cn2 = cn2->next[cn1->distinguishingInput];
					cn1 = cn1->next[cn1->distinguishingInput];
				}
				if (cn1->distinguishingInput == STOUT_INPUT)
					seq.splice(seq.end(), getShortestSepSeq(cn1->state, cn2->state, sepSeq, fsm));
				addSequence(cn, seq, fsm);
				addSequence(n, move(seq), fsm);
			}
		}
	}

	static void distinguishCNs(const shared_ptr<ConvergentNodeSPYH>& cn, const shared_ptr<ConvergentNodeSPYH>& refCN,
		vector<shared_ptr<ConvergentNodeSPYH>>& nodes, int depth, const vector<LinkCell>& sepSeq, const unique_ptr<DFSM>& fsm) {
		
		distinguish(cn, nodes, sepSeq, fsm);
		if (!refCN->isReferenceNode) distinguish(refCN, nodes, sepSeq, fsm);
		if (depth > 0) {
			nodes.emplace_back(cn);
			if (!refCN->isReferenceNode) nodes.emplace_back(refCN);
			for (input_t i = 0; i < fsm->getNumberOfInputs(); i++) {
				if (!cn->next[i]) {
					addSequence(cn, sequence_in_t({ i }), fsm);
				}
				if (!refCN->next[i]) {
					addSequence(refCN, sequence_in_t({ i }), fsm);
				}
				distinguishCNs(cn->next[i], refCN->next[i], nodes, depth - 1, sepSeq, fsm);
			}
			if (!refCN->isReferenceNode) nodes.pop_back();
			nodes.pop_back();
		}
	}

	static void mergeCN(const shared_ptr<ConvergentNodeSPYH>& fromCN, const shared_ptr<ConvergentNodeSPYH>& toCN) {
		if (fromCN->convergent.front()->depth < toCN->convergent.front()->depth) {
			toCN->convergent.emplace_front(move(fromCN->convergent.front()));
			fromCN->convergent.pop_front();
		}
		for (auto&& n : fromCN->convergent) {
			toCN->convergent.emplace_back(n);
		}
		fromCN->convergent.clear();
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

	static sequence_set_t getSequences(const shared_ptr<TestNodeSPYH>& node, const unique_ptr<DFSM>& fsm) {
		sequence_set_t TS;
		stack<pair<shared_ptr<TestNodeSPYH>, sequence_in_t>> lifo;
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

	sequence_set_t SPYH_method(const unique_ptr<DFSM>& fsm, int extraStates) {
		RETURN_IF_UNREDUCED(fsm, "FSMtesting::SPYH_method", sequence_set_t());
		if (extraStates < 0) {
			return sequence_set_t();
		}
		if (fsm->getNumberOfStates() == 1) {
			return getTraversalSet(fsm, extraStates);
		}
		auto sepSeq = getSeparatingSequences(fsm);
		vector<shared_ptr<ConvergentNodeSPYH>> stateNodes(fsm->getNumberOfStates());
		stateNodes.reserve(fsm->getNumberOfStates() + 2 * extraStates);
		
		output_t outputState = (fsm->isOutputState()) ? fsm->getOutput(0, STOUT_INPUT) : DEFAULT_OUTPUT;
		auto root = make_shared<TestNodeSPYH>(0, DEFAULT_OUTPUT, outputState, 0, fsm->getNumberOfInputs());
		auto cn = make_shared<ConvergentNodeSPYH>(root);
		auto SC = getStateCover(fsm, true);
		for (const auto& seq : SC) {
			auto refCN = appendSequence(cn, root, seq, fsm);
			refCN->isReferenceNode = true;
			stateNodes[refCN->state] = refCN;
		}
		using tran_t = tuple<shared_ptr<ConvergentNodeSPYH>, input_t, shared_ptr<ConvergentNodeSPYH>>;
		list<tran_t> transitions;
		for (const auto& sn : stateNodes) {
			distinguish(sn, stateNodes, sepSeq, fsm);
			for (input_t i = 0; i < sn->next.size(); i++) {
				if ((!sn->next[i] || !sn->next[i]->isReferenceNode) && 
					(fsm->getNextState(sn->state, i) != NULL_STATE)) {
					transitions.emplace_back(make_tuple(sn, i, stateNodes[fsm->getNextState(sn->state, i)]));
				}
			}
		}
		// stateNodes are initialized divergence-preserving state cover
		
		transitions.sort([](const tran_t& t1, const tran_t& t2){
			return (get<0>(t1)->convergent.front()->depth + get<2>(t1)->convergent.front()->depth)
				< (get<0>(t2)->convergent.front()->depth + get<2>(t2)->convergent.front()->depth);
		});

		// confirm all transitions -> convergence-preserving transition cover
		while (!transitions.empty()) {
			auto startCN = move(get<0>(transitions.front()));
			auto input = get<1>(transitions.front());
			auto nextStateCN = move(get<2>(transitions.front()));
			transitions.pop_front();

			// identify next state
			auto ncn = startCN->next[input];
			if (!ncn) {
				ncn = appendSequence(startCN, startCN->convergent.front(), sequence_in_t({ input }), fsm);
			}
			distinguishCNs(ncn, nextStateCN, stateNodes, extraStates, sepSeq, fsm);

			startCN->next[input] = nextStateCN;
			mergeCN(ncn, nextStateCN);
		}
		// obtain TS
		//printTStree(root);
		// clean so convergent nodes can be destroyed
		for (auto& sn : stateNodes) {
			sn->next.clear();
		}
		return getSequences(stateNodes[0]->convergent.front(), fsm);
	}
}

