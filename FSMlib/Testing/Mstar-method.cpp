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

#include <sys/stat.h>
#include <direct.h>
#include <process.h>
#include "FSMtesting.h"
#include "../UnionFind.h"

using namespace FSMsequence;

namespace FSMtesting {

#define GUROBI_SOLVER string("/bin/gurobi_cl")
#define LP_FOLDER "lp"

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

	static void printCosts(vector<vector<seq_len_t> >& costs) {
		state_t Tsize = state_t(costs.size());
		printf("\t");
		for (state_t i = 0; i < Tsize; i++) {
			printf("%u\t", i);
		}
		printf("\n");
		for (state_t i = 0; i < Tsize; i++) {
			printf("%u\t", i);
			for (state_t j = 0; j < Tsize; j++) {
				printf("%u\t", costs[i][j]);
			}
			printf("\n");
		}
	}

	static bool writeLP(vector<vector<seq_len_t> >& costs, string fileName) {
		FILE * file = NULL;
		if (!(file = fopen(fileName.c_str(), "w"))) {
			ERROR_MESSAGE("Mstar-method: Unable to create a file for LP.");
			return false;
		}
		state_t numTests = state_t(costs.size());
		fprintf(file, "Minimize\n\t");
		for (state_t i = 0; i < numTests - 1; i++) {
			for (state_t j = 0; j < numTests; j++) {
				fprintf(file, "%u x_%u_%u + ", costs[i][j], i, j);
			}
			fprintf(file, "\n\t");
		}
		for (state_t j = 0; j < numTests - 1; j++) {
			fprintf(file, "%u x_%u_%u + ", costs[numTests - 1][j], numTests - 1, j);
		}
		fprintf(file, "%u x_%u_%u\n", costs[numTests - 1][numTests - 1], numTests - 1, numTests - 1);

		fprintf(file, "Subject To\n\t");
		//enter once
		for (state_t j = 0; j < numTests; j++) {
			for (state_t i = 0; i < numTests - 1; i++) {
				fprintf(file, "x_%u_%u + ", i, j);
			}
			fprintf(file, "x_%u_%u = 1\n\t", numTests - 1, j);
		}
		//leave once
		for (state_t i = 0; i < numTests; i++) {
			for (state_t j = 0; j < numTests - 1; j++) {
				fprintf(file, "x_%u_%u + ", i, j);
			}
			fprintf(file, "x_%u_%u = 1\n\t", i, numTests - 1);
		}
		// cycle indivisibility
		for (state_t i = 0; i < numTests; i++) {
			for (state_t j = 0; j < numTests - 1; j++) {
				fprintf(file, "o_%u - o_%u - %u x_%u_%u >= %d\n\t", j, i, 2 * numTests, i, j, (1 - 2 * numTests));
			}
		}
		//fprintf(file, "o_%d = 1\n", numTests - 1);

		fprintf(file, "Binaries\n\t");
		for (state_t i = 0; i < numTests; i++) {
			for (state_t j = 0; j < numTests; j++) {
				fprintf(file, "x_%u_%u ", i, j);
			}
		}
		fprintf(file, "\nEnd\n");
		fflush(file);
		fclose(file);
		return true;
	}

	static bool process_Mstar(const unique_ptr<DFSM>& fsm, sequence_set_t& TS, int extraStates, bool resetEnabled) {
		TS.clear();
		char* gurobiPath;
		gurobiPath = getenv("GUROBI_HOME");
		if (gurobiPath == NULL) {
			ERROR_MESSAGE("Mstar-method needs Gurobi solver to run and GUROBI_HOME is not a system variable!");
			return false;
		}
		
		auto d = getAdaptiveDistinguishingSet(fsm);
		if (d.empty()) {
			return false;
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
		for (auto seq : d) {
			if (adsLen < d.size()) {
				adsLen = seq_len_t(d.size());
			}
		}

		auto sp = FSMmodel::createAllShortestPaths(fsm);
		sequence_vec_t trSeq(N);
		if (resetEnabled) {
			auto sc = getStateCover(fsm); 
			for (auto seq : sc) {
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
					sequence_in_t seq;
					tests.push_back(seq);
					continue;
				}
				sequence_in_t seq(d[nextState]);
				if (fsm->isOutputState()) seq.push_front(STOUT_INPUT);
				seq.push_front(input);
				tests.push_back(seq);
				counter++;
			}
		}
		tests.push_back(d[0]);

		// compute costs
		state_t Tsize = state_t(tests.size());
		seq_len_t maxCost = adsLen + N + 1;
		vector<vector<seq_len_t> > costs(Tsize);
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
							pq_entry_t en(idx, nextIdx, cost);
							edges.push(en);
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
						pq_entry_t en(idx, i, costs[idx][i]);
						edges.push(en);
					}
				}
			}
		}
		idx = Tsize - 1;
		auto it = tests[idx].begin();
		state_t actState = 0;
		seq_len_t cost = 0;
		for (; it != tests[idx].end(); it++, cost++) {
			state_t nextIdx = actState * P + (*it);
			if ((costs[idx][nextIdx] == maxCost) && !tests[nextIdx].empty()) {
				if (equalSeqPart(it, tests[idx].end(), tests[nextIdx].begin(), tests[nextIdx].end())) {
					costs[idx][nextIdx] = cost;
					pq_entry_t en(idx, nextIdx, cost);
					edges.push(en);
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
				pq_entry_t en(idx, i, costs[idx][i]);
				edges.push(en);
			}
		}

		// write LP
		_mkdir(LP_FOLDER);
		string fileName = fsm->getFilename();
		fileName = FSMlib::Utils::getUniqueName(fileName, "lp", string(LP_FOLDER)+"/");

		if (!writeLP(costs, fileName)) return false;
		string gurobiCl = string(gurobiPath) + GUROBI_SOLVER;
		string resultFile = "ResultFile=" + fileName + ".sol";
		auto rv = _spawnl(P_WAIT, gurobiCl.c_str(), gurobiCl.c_str(), resultFile.c_str(), fileName.c_str(), NULL);
		if (rv != 0) {
			ERROR_MESSAGE("Mstar-method: Fail in solving LP.\n");
			return false;
		}

		// obtain solution of LP
		vector<state_t> next(Tsize);
		fileName += ".sol";
		ifstream fin(fileName);
		if (!fin.is_open()) {
			ERROR_MESSAGE("Mstar-method - unable to open file %s", fileName.c_str());
			return false;
		}
		string s;
		int pos;
		getline(fin, s);
		for (state_t i = 0; i < Tsize; i++) {
			for (state_t j = 0; j < Tsize; j++) {
				fin >> s >> pos;
				if (pos) {
					next[i] = j;
				}
			}
		}

		// create CS
		idx = Tsize - 1;
		for (state_t i = 0; i < Tsize - 1; i++) {
			if (costs[idx][next[idx]] >= tests[idx].size()) {
				CS.insert(CS.end(), tests[idx].begin(), tests[idx].end());
				state_t from = fsm->getEndPathState((idx == Tsize - 1) ? 0 : idx / P, tests[idx]);
				if (from != next[idx] / P) {
					if (resetEnabled && (trSeq[next[idx] / P].size() + 1 < sp[from][next[idx] / P].first)) {
						sequence_in_t seq(CS);
						TS.insert(seq);
						CS.clear();
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
		TS.insert(CS);
		return true;
	}

	bool Mstar_method(const unique_ptr<DFSM>& fsm, sequence_in_t& CS, int extraStates) {
		sequence_set_t TS;
		CS.clear();
		if (!process_Mstar(fsm, TS, extraStates, false)) return false;
		CS.insert(CS.end(), TS.begin()->begin(), TS.begin()->end());
		return true;
	}

	bool Mrstar_method(const unique_ptr<DFSM>& fsm, sequence_set_t& TS, int extraStates) {
		return process_Mstar(fsm, TS, extraStates, true);
	}

}
