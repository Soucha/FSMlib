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

	static size_t areDistinguished(const sequence_in_t& l1, const sequence_in_t& l2, map<sequence_in_t, vector<sequence_out_t>>& ot) {
		const auto& row1 = ot[l1];
		const auto& row2 = ot[l2];
		for (size_t i = 0; i < row1.size(); i++) {
			if (row1[i] != row2[i]) return i;
		}
		return -1;
	}

	static void fillOTonE(const unique_ptr<Teacher>& teacher, map<sequence_in_t, vector<sequence_out_t>>& ot, const vector<sequence_in_t>& E) {	
		for (auto& p : ot) {
			for (auto i = p.second.size(); i < E.size(); i++) {
				teacher->resetAndOutputQuery(p.first);
				p.second.emplace_back(teacher->outputQuery(E[i]));
			}
		}
	}

	static void fillOTonS(const unique_ptr<Teacher>& teacher, map<sequence_in_t, vector<sequence_out_t>>& ot, 
			const vector<sequence_in_t>& E, sequence_in_t& newRowSeq) {
		vector<sequence_out_t> row;
		for (const auto& seq : E) {
			teacher->resetAndOutputQuery(newRowSeq);
			row.emplace_back(teacher->outputQuery(seq));
		}
		ot.emplace(newRowSeq, move(row));
	}

	static void extendOTbyNewInputs(const unique_ptr<Teacher>& teacher, map<sequence_in_t, vector<sequence_out_t>>& ot,
			const vector<sequence_in_t>& S, const vector<sequence_in_t>& E, const input_t& numInputs) {
		for (const auto& stateSeq : S) {
			for (input_t i = numInputs; i < teacher->getNumberOfInputs(); i++) {
				sequence_in_t nextState(stateSeq);
				nextState.push_back(i);
				vector<sequence_out_t> row;
				for (const auto& seq : E) {
					teacher->resetAndOutputQuery(nextState);
					row.emplace_back(teacher->outputQuery(seq));
				}
				ot.emplace(move(nextState), move(row));
			}
		}
	}

	void addSuffixToE(const sequence_in_t& ce, const map<sequence_in_t, vector<sequence_out_t>>& ot,
		vector<sequence_in_t>& S, vector<sequence_in_t>& E) {
		sequence_in_t prefix, suffix(ce);
		for (const auto& input : ce) {
			prefix.push_back(input);
			if (!ot.count(prefix)) break;
			suffix.pop_front();
		}
		E.emplace_back(move(suffix));
	}

	unique_ptr<DFSM> Lstar(const unique_ptr<Teacher>& teacher, 
		function<void(const sequence_in_t& ce, const map<sequence_in_t, vector<sequence_out_t>>& ot,
			vector<sequence_in_t>& S, vector<sequence_in_t>& E)> processCounterexample, 
			function<bool(const unique_ptr<DFSM>& conjecture)> provideTentativeModel, bool checkConsistency) {
		if (!teacher->isBlackBoxResettable()) {
			ERROR_MESSAGE("FSMlearning::Lstar - the Black Box needs to be resettable");
			return nullptr;
		}
		auto numberOfInputs = teacher->getNumberOfInputs();
		//auto numberOfOutputs = teacher->getNumberOfOutputs();
		auto conjecture = FSMmodel::createFSM(teacher->getBlackBoxModelType(), 1, numberOfInputs);
		auto numberOfOutputs = conjecture->getNumberOfOutputs();
		/// counterexample
		sequence_in_t ce;
		/// set S - states
		vector<sequence_in_t> S;
		/// set E - distinguishing sequences
		vector<sequence_in_t> E;
		/// T - observation table
		map<sequence_in_t,vector<sequence_out_t>> ot;
		
		S.emplace_back(sequence_in_t());
		ot.emplace(sequence_in_t(), vector<sequence_out_t>());
		for (input_t i = 0; i < numberOfInputs; i++) {
			ot.emplace(sequence_in_t({ i }), vector<sequence_out_t>());
		}
		if (!conjecture->isOutputState()) {
			for (input_t i = 0; i < numberOfInputs; i++) {
				E.emplace_back(sequence_in_t({ i }));
			}
		}
		else {
			E.emplace_back(sequence_in_t({ STOUT_INPUT }));
		}
		fillOTonE(teacher, ot, E);
		if (conjecture->isOutputState()) {
			if (numberOfOutputs != teacher->getNumberOfOutputs()) {
				conjecture->incNumberOfOutputs(teacher->getNumberOfOutputs() - numberOfOutputs);
				numberOfOutputs = teacher->getNumberOfOutputs();
			}
			conjecture->setOutput(0, ot[sequence_in_t()][0].front());
		}
		bool unlearned = true;
		while (unlearned) {
			if (checkConsistency) {
				for (state_t s1 = 0; s1 < S.size(); s1++) {
					for (state_t s2 = s1 + 1; s2 < S.size(); s2++) {
						if (areDistinguished(S[s1], S[s2], ot) == -1) {
							sequence_in_t ns1(S[s1]), ns2(S[s2]);
							for (input_t input = 0; input < numberOfInputs; input++) {
								ns1.push_back(input); ns2.push_back(input);
								auto idx = areDistinguished(ns1, ns2, ot);
								if (idx != -1) {
									sequence_in_t dist(E[idx]);
									dist.push_front(input);
									E.emplace_back(move(dist));
									fillOTonE(teacher, ot, E);
									break;
								}
								ns1.pop_back(); ns2.pop_back();
							}
						}
					}
				}
			}
			// make closed
			for (state_t state = 0; state < S.size(); state++) {
				for (input_t input = 0; input < numberOfInputs; input++) {
					sequence_in_t nextStateSeq(S[state]);
					nextStateSeq.push_back(input);
					state_t nextState = NULL_STATE;
					for (state_t refState = 0; refState < S.size(); refState++) {
						if (areDistinguished(nextStateSeq, S[refState], ot) == -1) {
							nextState = refState;
							break;
						}
					}
					if (nextState == NULL_STATE) {
						for (input_t i = 0; i < numberOfInputs; i++) {
							nextStateSeq.push_back(i);
							fillOTonS(teacher, ot, E, nextStateSeq);
							nextStateSeq.pop_back();
						}
						// conjecture
						if (numberOfOutputs != teacher->getNumberOfOutputs()) {
							conjecture->incNumberOfOutputs(teacher->getNumberOfOutputs() - numberOfOutputs);
							numberOfOutputs = teacher->getNumberOfOutputs();
						}
						auto id = conjecture->addState(conjecture->isOutputState() ? ot[nextStateSeq][0].front() : DEFAULT_OUTPUT);
						output_t output = DEFAULT_OUTPUT;
						if (conjecture->isOutputTransition()) {
							if (conjecture->getNextState(state, input) == NULL_STATE) {
								teacher->resetAndOutputQuery(S[state]);
								output = teacher->outputQuery(input);
								if (numberOfOutputs != teacher->getNumberOfOutputs()) {
									conjecture->incNumberOfOutputs(teacher->getNumberOfOutputs() - numberOfOutputs);
									numberOfOutputs = teacher->getNumberOfOutputs();
								}
							}
							else output = conjecture->getOutput(state, input);
						}
						conjecture->setTransition(state, input, id, output);

						S.emplace_back(move(nextStateSeq));
						if (provideTentativeModel) {
							unlearned = provideTentativeModel(conjecture);
						}
					}
					else if (conjecture->getNextState(state, input) == NULL_STATE) {
						output_t output = DEFAULT_OUTPUT;
						if (conjecture->isOutputTransition()) {
							teacher->resetAndOutputQuery(S[state]);
							output = teacher->outputQuery(input);
							if (numberOfOutputs != teacher->getNumberOfOutputs()) {
								conjecture->incNumberOfOutputs(teacher->getNumberOfOutputs() - numberOfOutputs);
								numberOfOutputs = teacher->getNumberOfOutputs();
							}
						}
						conjecture->setTransition(state, input, nextState, output);
						if (provideTentativeModel) {
							unlearned = provideTentativeModel(conjecture);
						}
					}
					else if (conjecture->getNextState(state, input) != nextState) {
						conjecture->setTransition(state, input, nextState, 
							conjecture->isOutputTransition() ? conjecture->getOutput(state, input) : DEFAULT_OUTPUT);
						if (provideTentativeModel) {
							unlearned = provideTentativeModel(conjecture);
						}
					}
				}
			}
			if (numberOfInputs != teacher->getNumberOfInputs()) {
				extendOTbyNewInputs(teacher, ot, S, E, numberOfInputs);
				conjecture->incNumberOfInputs(teacher->getNumberOfInputs() - numberOfInputs); 
				numberOfInputs = teacher->getNumberOfInputs();
			}
			else {// EQ -> CE or stop
				ce = teacher->equivalenceQuery(conjecture);
				if (ce.empty()) unlearned = false;
				else {
					if (conjecture->isOutputState() && !conjecture->isOutputTransition() // Moore and DFA
						&& (ce.back() == STOUT_INPUT)) ce.pop_back();
					// processCE
					auto ssize = S.size();
					processCounterexample(ce, ot, S, E);

					if (ssize != S.size()) {
						for (auto i = ssize; i < S.size(); i++) {
							fillOTonS(teacher, ot, E, S[i]);
						}
					}
					if (ot.begin()->second.size() != E.size())
						fillOTonE(teacher, ot, E);
				}
			}
		}
		return conjecture;
	}
}
