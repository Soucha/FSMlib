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

	static void printCosts(const vector<vector<seq_len_t>>& costs) {
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

	static bool writeLP(const vector<vector<seq_len_t>>& costs, const string& fileName) {
		FILE * file;
		if (fopen_s(&file, fileName.c_str(), "w") != 0) {
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

	static sequence_set_t process_Mstar(const unique_ptr<DFSM>& fsm, int extraStates, bool resetEnabled) {
		char* gurobiPath;
		size_t sz = 0;
		if (_dupenv_s(&gurobiPath, &sz, "GUROBI_HOME") != 0) {
			ERROR_MESSAGE("Mstar-method needs Gurobi solver to run and GUROBI_HOME is not a system variable!");
			return sequence_set_t();
		}
		
		auto d = getAdaptiveDistinguishingSet(fsm);
		if (d.empty()) {
			return sequence_set_t();
		}
		state_t N = fsm->getNumberOfStates(), P = fsm->getNumberOfInputs();
		sequence_in_t CS;
		bool startWithStout = (d[0].front() == STOUT_INPUT);
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
				if (fsm->isOutputState() && !startWithStout) seq.push_front(STOUT_INPUT);
				seq.push_front(input);
				tests.emplace_back(move(seq));
				counter++;
			}
		}
		if (startWithStout) {
			d[0].pop_front();
			CS.push_back(STOUT_INPUT);// the first symbol
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
				auto actState = fsm->getNextState(state, input);
				if (actState == NULL_STATE) continue;
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
			if (fsm->isOutputState() && (++it == tests[idx].end())) break;
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

		// write LP
		_mkdir(LP_FOLDER);
		string fileName = fsm->getFilename();
		fileName = FSMlib::Utils::getUniqueName(fileName, "lp", string(LP_FOLDER)+"/");

		if (!writeLP(costs, fileName)) return sequence_set_t();
		string gurobiCl = string(gurobiPath) + GUROBI_SOLVER;
		free(gurobiPath);
		string resultFile = "ResultFile=" + fileName + ".sol";
		auto rv = _spawnl(P_WAIT, gurobiCl.c_str(), gurobiCl.c_str(), resultFile.c_str(), fileName.c_str(), NULL);
		if (rv != 0) {
			ERROR_MESSAGE("Mstar-method: Fail in solving LP.\n");
			return sequence_set_t();
		}

		// obtain solution of LP
		fileName += ".sol";
		ifstream fin(fileName);
		if (!fin.is_open()) {
			ERROR_MESSAGE("Mstar-method - unable to open file %s", fileName.c_str());
			return sequence_set_t();
		}
		vector<state_t> next(Tsize);
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
		sequence_set_t TS;
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
						if (fsm->isOutputState() && CS.back() != STOUT_INPUT) CS.push_back(STOUT_INPUT);
						auto shortestPath = FSMmodel::getShortestPath(fsm, from, next[idx] / P, sp);
						CS.insert(CS.end(), shortestPath.begin(), shortestPath.end());
					}
				}
				else if (fsm->isOutputState() && CS.back() != STOUT_INPUT) CS.push_back(STOUT_INPUT);
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

	sequence_in_t Mstar_method(const unique_ptr<DFSM>& fsm, int extraStates) {
		RETURN_IF_UNREDUCED(fsm, "FSMtesting::Mstar_method", sequence_in_t());
		auto TS = process_Mstar(fsm, extraStates, false);
		if (TS.empty()) return sequence_in_t();
		return sequence_in_t(TS.begin()->begin(), TS.begin()->end());
	}

	sequence_set_t Mrstar_method(const unique_ptr<DFSM>& fsm, int extraStates) {
		RETURN_IF_UNREDUCED(fsm, "FSMtesting::Mrstar_method", sequence_set_t());
		return process_Mstar(fsm, extraStates, true);
	}
}
