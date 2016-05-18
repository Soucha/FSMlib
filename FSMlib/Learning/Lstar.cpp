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

namespace FSMlearning {

	static size_t areDistinguished(const sequence_in_t& l1, const sequence_in_t& l2, ObservationTable& ot) {
		const auto& row1 = ot.T[l1];
		const auto& row2 = ot.T[l2];
		for (size_t i = 0; i < row1.size(); i++) {
			if (row1[i] != row2[i]) return i;
		}
		return -1;
	}

	static void fillOTonE(const unique_ptr<Teacher>& teacher, ObservationTable& ot) {
		for (auto& p : ot.T) {
			for (auto i = p.second.size(); i < ot.E.size(); i++) {
				teacher->resetAndOutputQuery(p.first);
				p.second.emplace_back(teacher->outputQuery(ot.E[i]));
			}
		}
	}

	static void fillOTonS(const unique_ptr<Teacher>& teacher, ObservationTable& ot, sequence_in_t& newRowSeq) {
		for (input_t i = 0; i < ot.conjecture->getNumberOfInputs(); i++) {
			newRowSeq.push_back(i);
			vector<sequence_out_t> row;
			for (const auto& seq : ot.E) {
				teacher->resetAndOutputQuery(newRowSeq);
				row.emplace_back(teacher->outputQuery(seq));
			}
			ot.T.emplace(newRowSeq, move(row));
			newRowSeq.pop_back();
		}		
	}

	static void extendOTbyNewInputs(const unique_ptr<Teacher>& teacher, ObservationTable& ot) {
		for (const auto& stateSeq : ot.S) {
			for (input_t i = ot.conjecture->getNumberOfInputs(); i < teacher->getNumberOfInputs(); i++) {
				sequence_in_t nextState(stateSeq);
				nextState.push_back(i);
				vector<sequence_out_t> row;
				for (const auto& seq : ot.E) {
					teacher->resetAndOutputQuery(nextState);
					row.emplace_back(teacher->outputQuery(seq));
				}
				ot.T.emplace(move(nextState), move(row));
			}
		}
	}

	static void makeConsistent(const unique_ptr<Teacher>& teacher, ObservationTable& ot) {
		for (state_t s1 = 0; s1 < ot.S.size(); s1++) {
			for (state_t s2 = s1 + 1; s2 < ot.S.size(); s2++) {
				if (areDistinguished(ot.S[s1], ot.S[s2], ot) == -1) {
					sequence_in_t ns1(ot.S[s1]), ns2(ot.S[s2]);
					for (input_t input = 0; input < ot.conjecture->getNumberOfInputs(); input++) {
						ns1.push_back(input); ns2.push_back(input);
						auto idx = areDistinguished(ns1, ns2, ot);
						if (idx != -1) {
							sequence_in_t dist(ot.E[idx]);
							dist.push_front(input);
							ot.E.emplace_back(move(dist));
							fillOTonE(teacher, ot);
							break;
						}
						ns1.pop_back(); ns2.pop_back();
					}
				}
			}
		}
	}

	static bool isClosed(ObservationTable& ot) {
		for (state_t state = 0; state < ot.S.size(); state++) {
			for (input_t input = 0; input < ot.conjecture->getNumberOfInputs(); input++) {
				sequence_in_t nextStateSeq(ot.S[state]);
				nextStateSeq.push_back(input);
				state_t nextState = NULL_STATE;
				for (state_t refState = 0; refState < ot.S.size(); refState++) {
					if (areDistinguished(nextStateSeq, ot.S[refState], ot) == -1) {
						nextState = refState;
						break;
					}
				}
				if (nextState == NULL_STATE) return false;
			}
		}
		return true;
	}

	static void checkNumberOfOutputs(const unique_ptr<Teacher>& teacher, const unique_ptr<DFSM>& conjecture) {
		if (conjecture->getNumberOfOutputs() != teacher->getNumberOfOutputs()) {
			conjecture->incNumberOfOutputs(teacher->getNumberOfOutputs() - conjecture->getNumberOfOutputs());
		}
	}

	static void addTransition(const unique_ptr<DFSM>& conjecture, state_t& from, input_t& input, state_t& to, ObservationTable& ot) {
		output_t output = DEFAULT_OUTPUT;
		if (conjecture->isOutputTransition()) {
			if (conjecture->getNextState(from, input) == NULL_STATE) {
				for (size_t i = input + conjecture->isOutputState(); i < ot.E.size(); i++) {
					if (ot.E[i].front() == input) {
						output = ot.T[ot.S[from]][i].front();
						break;
					}
				}
			}
			else output = conjecture->getOutput(from, input);
		}
		conjecture->setTransition(from, input, to, output);
	}

	void addAllPrefixesToS(const sequence_in_t& ce, ObservationTable& ot, const unique_ptr<Teacher>& teacher) {
		sequence_in_t prefix;
		bool add = false;
		for (const auto& input : ce) {
			if (input == STOUT_INPUT) continue;
			prefix.emplace_back(input);
			if (add) {
				ot.S.emplace_back(prefix);
			}
			else {
				if (find(ot.S.begin(), ot.S.end(), prefix) == ot.S.end()) {
					add = true;
					ot.S.emplace_back(prefix);
				}
			}
		}
	}

	void addSuffixToE_binarySearch(const sequence_in_t& ce, ObservationTable& ot, const unique_ptr<Teacher>& teacher) {
		auto refOutput = ot.conjecture->getOutput(ot.conjecture->getEndPathState(0, ce), STOUT_INPUT);
		sequence_in_t suffix(ce), prefix;
		size_t len = ce.size() / 2;
		size_t mod = ce.size() % 2;
		for (size_t i = 0; i < len; i++) {
			prefix.push_back(suffix.front());
			suffix.pop_front();
		}
		teacher->resetAndOutputQuery(ot.S[ot.conjecture->getEndPathState(0, prefix)]);
		auto output = teacher->outputQuery(suffix);
		bool sameOutput = (output.back() == refOutput);
		while (len + mod > 1) {
			if (sameOutput) {
				if (len == 1) break;
				mod = len % 2;
				len /= 2;
				for (size_t i = 0; i < len; i++) {
					suffix.push_front(prefix.back());
					prefix.pop_back();
				}
			}
			else {
				len += mod;
				mod = len % 2;
				len /= 2;
				for (size_t i = 0; i < len; i++) {
					prefix.push_back(suffix.front());
					suffix.pop_front();
				}
			}
			teacher->resetAndOutputQuery(ot.S[ot.conjecture->getEndPathState(0, prefix)]);
			auto output = teacher->outputQuery(suffix);
			sameOutput = (output.back() == refOutput);
		}
		if (!sameOutput) suffix.pop_front();
		ot.E.emplace_back(move(suffix));
	}

	void addSuffixAfterLastStateToE(const sequence_in_t& ce, ObservationTable& ot, const unique_ptr<Teacher>& teacher) {
		sequence_in_t prefix, suffix(ce);
		for (const auto& input : ce) {
			prefix.push_back(input);
			if (!ot.T.count(prefix)) break;
			suffix.pop_front();
		}
		ot.E.emplace_back(move(suffix));
	}

	void addAllSuffixesAfterLastStateToE(const sequence_in_t& ce, ObservationTable& ot, const unique_ptr<Teacher>& teacher) {
		sequence_in_t prefix, suffix(ce);
		for (const auto& input : ce) {
			if (input != STOUT_INPUT) {
				prefix.push_back(input);
				if (!ot.T.count(prefix)) break;
			}
			suffix.pop_front();
		}
		while (!suffix.empty() && (find(ot.E.begin(), ot.E.end(), suffix) == ot.E.end())) {
			ot.E.emplace_back(suffix);
			suffix.pop_front();
		}
	}

	void addSuffix1by1ToE(const sequence_in_t& ce, ObservationTable& ot, const unique_ptr<Teacher>& teacher) {
		sequence_in_t suffix;
		bool add = false;
		for (auto input = ce.rbegin(); input != ce.rend(); input++) {
			suffix.emplace_front(*input);
			if (!add && (find(ot.E.begin(), ot.E.end(), suffix) == ot.E.end())) {
				add = true;
			}
			if (add) {
				ot.E.emplace_back(suffix);
				fillOTonE(teacher, ot);
				if (!isClosed(ot)) break;
			}
		}
	}

	unique_ptr<DFSM> Lstar(const unique_ptr<Teacher>& teacher,
		function<void(const sequence_in_t& ce, ObservationTable& ot, const unique_ptr<Teacher>& teacher)> processCounterexample,
			function<bool(const unique_ptr<DFSM>& conjecture)> provideTentativeModel, bool checkConsistency) {
		if (!teacher->isBlackBoxResettable()) {
			ERROR_MESSAGE("FSMlearning::Lstar - the Black Box needs to be resettable");
			return nullptr;
		}
		/// counterexample
		sequence_in_t ce;
		/// observation table (S,E,T)
		ObservationTable ot;
		// numberOfOutputs can produce error message
		ot.conjecture = FSMmodel::createFSM(teacher->getBlackBoxModelType(), 1, teacher->getNumberOfInputs(), teacher->getNumberOfOutputs());
		auto& conjecture = ot.conjecture;
		ot.S.emplace_back(sequence_in_t());
		ot.T.emplace(sequence_in_t(), vector<sequence_out_t>());
		for (input_t i = 0; i < conjecture->getNumberOfInputs(); i++) {
			ot.T.emplace(sequence_in_t({ i }), vector<sequence_out_t>());
		}
		if (conjecture->isOutputState()) {
			ot.E.emplace_back(sequence_in_t({ STOUT_INPUT }));
		}
		if (conjecture->isOutputTransition()) {
			for (input_t i = 0; i < conjecture->getNumberOfInputs(); i++) {
				ot.E.emplace_back(sequence_in_t({ i }));
			}
		}
		fillOTonE(teacher, ot);
		if (conjecture->isOutputState()) {
			checkNumberOfOutputs(teacher, conjecture);
			conjecture->setOutput(0, ot.T[sequence_in_t()][0].front());
		}
		bool unlearned = true;
		while (unlearned) {
			if (checkConsistency) {
				makeConsistent(teacher, ot);
			}
			// make closed
			for (state_t state = 0; state < ot.S.size(); state++) {
				for (input_t input = 0; input < conjecture->getNumberOfInputs(); input++) {
					sequence_in_t nextStateSeq(ot.S[state]);
					nextStateSeq.push_back(input);
					state_t nextState = NULL_STATE;
					for (state_t refState = 0; refState < ot.S.size(); refState++) {
						if (areDistinguished(nextStateSeq, ot.S[refState], ot) == -1) {
							nextState = refState;
							break;
						}
					}
					if (nextState == NULL_STATE) {// new state
						fillOTonS(teacher, ot, nextStateSeq);
						// conjecture
						checkNumberOfOutputs(teacher, conjecture);
						auto id = conjecture->addState(conjecture->isOutputState() ? ot.T[nextStateSeq][0].front() : DEFAULT_OUTPUT);
						addTransition(conjecture, state, input, id, ot);

						ot.S.emplace_back(move(nextStateSeq));
						if (provideTentativeModel) {
							unlearned = provideTentativeModel(conjecture);
						}
					}
					else if (conjecture->getNextState(state, input) != nextState) {// set/update transition
						addTransition(conjecture, state, input, nextState, ot);
						if (provideTentativeModel) {
							unlearned = provideTentativeModel(conjecture);
						}
					}
					if (!unlearned) break;
				}
				if (!unlearned) break;
			}
			if (!unlearned) break;
			if (conjecture->getNumberOfInputs() != teacher->getNumberOfInputs()) {
				extendOTbyNewInputs(teacher, ot);
				if (conjecture->isOutputTransition()) {
					for (input_t i = conjecture->getNumberOfInputs(); i < teacher->getNumberOfInputs(); i++) {
						ot.E.emplace_back(sequence_in_t({ i }));
					}
					fillOTonE(teacher, ot);
				}
				conjecture->incNumberOfInputs(teacher->getNumberOfInputs() - conjecture->getNumberOfInputs());
			}
			else {// EQ -> CE or stop
				ce = teacher->equivalenceQuery(conjecture);
				if (ce.empty()) unlearned = false;
				else {
					if (conjecture->isOutputState() && !conjecture->isOutputTransition() // Moore and DFA
						&& (ce.back() == STOUT_INPUT)) ce.pop_back();
					// processCE
					auto ssize = ot.S.size();
					processCounterexample(ce, ot, teacher);
					if (ssize != ot.S.size()) {
						for (auto i = ssize; i < ot.S.size(); i++) {// increasing order of prefixes is supposed
							fillOTonS(teacher, ot, ot.S[i]);
							//conjecture is updated in making OT closed
							conjecture->addState(conjecture->isOutputState() ? ot.T[ot.S[i]][0].front() : DEFAULT_OUTPUT);
						}
					}
					if (ot.T.begin()->second.size() != ot.E.size())
						fillOTonE(teacher, ot);
				}
			}
		}
		conjecture->minimize();
		if (provideTentativeModel) {
			provideTentativeModel(conjecture);
		}
		return move(ot.conjecture);
	}
}
