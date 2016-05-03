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
#include "../UnionFind.h"

using namespace FSMsequence;

namespace FSMtesting {
	struct pq_entry_t {
		state_t from, to;
		seq_len_t len;

		pq_entry_t(state_t from, state_t to, seq_len_t len) :
			from(from), len(len), to(to) {
		}
	};

	struct pq_entry_comp {

		bool operator()(const pq_entry_t& x, const pq_entry_t& y) const {
			return x.len > y.len;
		}
	};

	static bool equalSeqPart(sequence_in_t::iterator first1, sequence_in_t::iterator last1,
		sequence_in_t::iterator first2, sequence_in_t::iterator last2) {
		while ((first1 != last1) && (first2 != last2)) {
			if (!(*first1 == *first2))
				return false;
			++first1;
			++first2;
		}
		return true;
	}

	static sequence_set_t process_Mg(const unique_ptr<DFSM>& fsm, int extraStates, bool resetEnabled) {
		sequence_set_t TS;
		auto d = getAdaptiveDistinguishingSet(fsm);
		if (d.empty()) {
			return TS;
		}
		state_t N = fsm->getNumberOfStates(), P = fsm->getNumberOfInputs();
		sequence_in_t CS;
		auto states = fsm->getStates();

		if (fsm->isOutputState()) {
			for (state_t i = 0; i < d.size(); i++) {
				auto origDS = d[i];
				auto DSit = d[i].begin();
				for (auto it = origDS.begin(); it != origDS.end(); it++, DSit++) {
					if (*it == STOUT_INPUT) continue;
					it++;
					if ((it == origDS.end()) || (*it != STOUT_INPUT)) {
						d[i].insert(++DSit, STOUT_INPUT);
						DSit--;
					}
					it--;
				}
				if (d[i].front() == STOUT_INPUT) d[i].pop_front();
			}
			CS.push_back(STOUT_INPUT);// the first symbol
		}

		seq_len_t adsLen = 0;
		for (const auto& seq : d) {
			if (adsLen < seq.size()) {
				adsLen = seq_len_t(seq.size());
			}
		}

		auto sp = FSMmodel::createAllShortestPaths(fsm);
		sequence_vec_t trSeq(N);
		if (resetEnabled) {
			auto sc = getStateCover(fsm);
			for (const auto& seq : sc) {
				trSeq[fsm->getEndPathState(0, seq)] = seq;
			}
		}

		// create test segments
		state_t counter = 0;
		sequence_vec_t tests;
		for (state_t state = 0; state < N; state++) {
			for (input_t input = 0; input < P; input++) {
				state_t nextState = fsm->getNextState(state, input);
				if (nextState == NULL_STATE) {
					tests.emplace_back(sequence_in_t());
					continue;
				}
				sequence_in_t seq(d[nextState]);
				if (fsm->isOutputState()) seq.push_front(STOUT_INPUT);
				seq.push_front(input);
				tests.emplace_back(move(seq));
				counter++;
			}
		}
		tests.emplace_back(d[0]);

		// compute costs
		state_t Tsize = state_t(tests.size());
		seq_len_t maxCost = adsLen + N + 1;
		vector<vector<seq_len_t>> costs(Tsize);
		priority_queue<pq_entry_t, vector<pq_entry_t>, pq_entry_comp> edges;
		for (state_t i = 0; i < Tsize; i++) {
			costs[i].resize(Tsize, maxCost);
		}
		state_t idx = 0;
		for (state_t state = 0; state < N; state++) {
			for (input_t input = 0; input < P; input++, idx++) {
				auto actState = fsm->getNextState(states[state], input);
				if (actState == NULL_STATE) continue;
				actState = getIdx(states, actState);
				auto it = tests[idx].begin();
				seq_len_t cost = 1;
				for (++it; it != tests[idx].end(); it++, cost++) {
					if (fsm->isOutputState()) {
						it++;
						if (it == tests[idx].end()) break;
					}
					state_t nextIdx = actState * P + (*it);
					if ((costs[idx][nextIdx] == maxCost) && (nextIdx != idx) && !tests[nextIdx].empty()) {
						if (equalSeqPart(it, tests[idx].end(), tests[nextIdx].begin(), tests[nextIdx].end())) {
							costs[idx][nextIdx] = cost;
							edges.emplace(pq_entry_t(idx, nextIdx, cost));
						}
					}
					actState = fsm->getNextState(actState, *it);
					actState = getIdx(states, actState);
				}
				costs[idx][Tsize - 1] = seq_len_t(tests[idx].size());
				for (state_t i = 0; i < Tsize - 1; i++) {
					if ((costs[idx][i] == maxCost) && (idx != i) && !tests[i].empty()) {
						if (actState == i / P) {// actState = end state of tests[idx]; i/P is start state of tests[i]
							costs[idx][i] = seq_len_t(tests[idx].size());
						}
						else if (resetEnabled && (trSeq[i / P].size() + 1 < sp[actState][i / P].first)) {
							costs[idx][i] = seq_len_t(tests[idx].size() + trSeq[i / P].size() + 1);
						}
						else {
							costs[idx][i] = seq_len_t(tests[idx].size() + sp[actState][i / P].first);
						}
						edges.emplace(pq_entry_t(idx, i, costs[idx][i]));
					}
				}
			}
		}
		idx = Tsize - 1;
		state_t actState = 0;
		seq_len_t cost = 0;
		for (auto it = tests[idx].begin(); it != tests[idx].end(); it++, cost++) {
			state_t nextIdx = actState * P + (*it);
			if ((costs[idx][nextIdx] == maxCost) && !tests[nextIdx].empty()) {
				if (equalSeqPart(it, tests[idx].end(), tests[nextIdx].begin(), tests[nextIdx].end())) {
					costs[idx][nextIdx] = cost;
					edges.emplace(pq_entry_t(idx, nextIdx, cost));
				}
			}
			actState = fsm->getNextState(actState, *it);
			actState = getIdx(states, actState);
			if (fsm->isOutputState()) it++;
		}
		for (state_t i = 0; i < Tsize - 1; i++) {
			if ((costs[idx][i] == maxCost) && !tests[i].empty()) {
				if (actState == i / P) {
					costs[idx][i] = seq_len_t(tests[idx].size());
				}
				else if (resetEnabled && (trSeq[i / P].size() + 1 < sp[actState][i / P].first)) {
					costs[idx][i] = seq_len_t(tests[idx].size() + trSeq[i / P].size() + 1);
				}
				else {
					costs[idx][i] = seq_len_t(tests[idx].size() + sp[actState][i / P].first);
				}
				edges.emplace(pq_entry_t(idx, i, costs[idx][i]));
			}
		}

		// create route
		vector<state_t> prev(Tsize, NULL_STATE), next(Tsize, NULL_STATE);
		FSMlib::UnionFind uf(Tsize);
		while (!edges.empty()) {
			auto & e = edges.top();
			if (((counter > 0) && (uf.doFind(e.from) != uf.doFind(e.to))) &&
					(next[e.from] == NULL_STATE) && (prev[e.to] == NULL_STATE)) {
				uf.doUnion(e.from, e.to);
				next[e.from] = e.to;
				prev[e.to] = e.from;
				counter--;
			}
			else if (counter == 0) break;
			edges.pop();
		}

		// create CS
		idx = Tsize - 1;
		for (state_t i = 0; i < Tsize - 1; i++) {
			if (costs[idx][next[idx]] >= tests[idx].size()) {
				CS.insert(CS.end(), tests[idx].begin(), tests[idx].end());
				state_t from = fsm->getEndPathState((idx == Tsize - 1) ? 0 : idx / P, tests[idx]);
				if (from != next[idx] / P) {
					if (resetEnabled && (trSeq[next[idx] / P].size() + 1 < sp[from][next[idx] / P].first)) {
						TS.emplace(move(CS));
						CS.assign(trSeq[next[idx] / P].begin(), trSeq[next[idx] / P].end());
					}
					else {
						auto shortestPath = FSMmodel::getShortestPath(fsm, from, next[idx] / P, sp);
						CS.insert(CS.end(), shortestPath.begin(), shortestPath.end());
					}
				}
			}
			else {
				auto it = tests[idx].begin();
				for (seq_len_t j = 0; j < costs[idx][next[idx]]; j++) {
					it++;
					if (fsm->isOutputState()) it++;
				}
				CS.insert(CS.end(), tests[idx].begin(), it);
			}
			idx = next[idx];
		}
		CS.insert(CS.end(), tests[idx].begin(), tests[idx].end());
		TS.emplace(move(CS));
		return TS;
	}

	sequence_in_t Mg_method(const unique_ptr<DFSM>& fsm, int extraStates) {
		auto TS = process_Mg(fsm, extraStates, false);
		if (TS.empty()) return sequence_in_t();
		return sequence_in_t(TS.begin()->begin(), TS.begin()->end());
	}

	sequence_set_t Mrg_method(const unique_ptr<DFSM>& fsm, int extraStates) {
		return process_Mg(fsm, extraStates, true);
	}
}
