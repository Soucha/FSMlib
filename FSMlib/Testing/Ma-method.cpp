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
	static bool equalSeqPart(sequence_in_t::iterator first1, sequence_in_t::iterator last1,
		sequence_in_t::iterator & first2, sequence_in_t::iterator last2) {
		while ((first1 != last1) && (first2 != last2)) {
			if (!(*first1 == *first2))
				return false;
			++first1;
			++first2;
		}
		return true;
	}

	static sequence_set_t process_Ma(const unique_ptr<DFSM>& fsm, int extraStates, bool resetEnabled) {
		sequence_set_t TS;
		auto E = getAdaptiveDistinguishingSet(fsm);
		if (E.empty()) {
			return TS;
		}
		state_t N = fsm->getNumberOfStates(), P = fsm->getNumberOfInputs(), currState, nextState;
		sequence_in_t CS;
		sequence_set_t stateCover;
		if (resetEnabled) {
			stateCover = getStateCover(fsm);
		}
		
		/* // Example from simao2009checking
		E.clear();
		E[0].push_back(0);
		E[0].push_back(1);
		E[0].push_back(0);
		E[1].push_back(0);
		E[1].push_back(1);
		E[1].push_back(0);
		E[2].push_back(0);
		E[2].push_back(1);
		E[3].push_back(0);
		E[3].push_back(1);
		E[4].push_back(0);
		E[4].push_back(1);
		//*/
		auto states = fsm->getStates();
		int counter = N * fsm->getNumberOfInputs();
		vector < vector<bool>> verifiedTransition(N);
		vector<input_t> verifiedState(N, P);
		for (state_t i = 0; i < N; i++) {
			verifiedTransition[i].assign(P, false);
		}
		sequence_in_t::iterator currInput, nextInput;
		input_t input;
		
		if (fsm->isOutputState()) {
			for (state_t i = 0; i < E.size(); i++) {
				auto origDS = E[i];
				auto DSit = E[i].begin();
				for (auto it = origDS.begin(); it != origDS.end(); it++, DSit++) {
					if (*it == STOUT_INPUT) continue;
					it++;
					if ((it == origDS.end()) || (*it != STOUT_INPUT)) {
						E[i].insert(++DSit, STOUT_INPUT);
						DSit--;
					}
					it--;
				}
				if (E[i].front() == STOUT_INPUT) E[i].pop_front();
			}
			CS.push_back(STOUT_INPUT);// the first symbol
		}
		CS.insert(CS.end(), E[0].begin(), E[0].end());

		currState = 0;
		currInput = CS.begin();
		if (fsm->isOutputState()) currInput++;

		while (counter > 0) {
			//printf("%u %p %p %s\n", currState, currInput, CS.end(), FSMmodel::getInSequenceAsString(CS).c_str());
			if (currInput != CS.end()) {
				input = *currInput;
				currInput++;
				if (fsm->isOutputState()) currInput++;
				nextState = fsm->getNextState(states[currState], input);
				nextState = getIdx(states, nextState);
				if (!verifiedTransition[currState][input]) {
					nextInput = E[nextState].begin();
					if (equalSeqPart(currInput, CS.end(), nextInput, E[nextState].end())) {
						if (nextInput != E[nextState].end()) {
							if (currInput == CS.end()) {
								currInput--;
								CS.insert(CS.end(), nextInput, E[nextState].end());
								currInput++;
							}
							else {
								CS.insert(CS.end(), nextInput, E[nextState].end());
							}
						}
						verifiedTransition[currState][input] = true;
						verifiedState[currState]--;
						counter--;
					}
				}
				currState = nextState;
			}
			else if (verifiedState[currState] > 0) {
				for (input = 0; input < P; input++) {
					if (!verifiedTransition[currState][input]) {
						nextState = fsm->getNextState(states[currState], input);
						nextState = getIdx(states, nextState);
						if (currInput == CS.begin()){
							CS.push_back(input);
							currInput = CS.begin();
						}
						else {
							currInput--;
							CS.push_back(input);
							currInput++;
						}
						if (fsm->isOutputState()) CS.push_back(STOUT_INPUT);
						CS.insert(CS.end(), E[nextState].begin(), E[nextState].end());
						currInput++;
						if (fsm->isOutputState()) currInput++;
						verifiedTransition[currState][input] = true;
						verifiedState[currState]--;
						counter--;
						currState = nextState;
						break;
					}
				}
			}
			else {// find unverified transition
				vector<bool> covered(N, false);
				list< pair<state_t, sequence_in_t> > fifo;
				sequence_in_t s;

				covered[currState] = true;
				fifo.push_back(make_pair(currState, s));
				while (!fifo.empty()) {
					auto current = fifo.front();
					fifo.pop_front();
					for (input = 0; input < fsm->getNumberOfInputs(); input++) {
						nextState = fsm->getNextState(states[current.first], input);
						if (nextState == WRONG_STATE) continue;
						nextState = getIdx(states, nextState);
						if (verifiedState[nextState] > 0) {
							if (resetEnabled) {
								for (auto trSeq : stateCover) {
									if (trSeq.size() >= current.second.size()) {
										CS.insert(CS.end(), current.second.begin(), current.second.end());
										CS.push_back(input);
										if (fsm->isOutputState()) CS.push_back(STOUT_INPUT);
										break;
									}
									currState = fsm->getEndPathState(0, trSeq);
									currState = getIdx(states, currState);
									if (verifiedState[currState] > 0) {
										nextState = currState;
										sequence_in_t seq(CS);
										TS.insert(seq);
										CS.clear();
										CS.assign(trSeq.begin(), trSeq.end());
										break;
									}
								}
							}
							else {
								CS.insert(CS.end(), current.second.begin(), current.second.end());
								CS.push_back(input);
								if (fsm->isOutputState()) CS.push_back(STOUT_INPUT);
							}
							currState = nextState;
							currInput = CS.end();
							fifo.clear();
							break;
						}
						if (!covered[nextState]) {
							covered[nextState] = true;
							sequence_in_t newPath(current.second);
							newPath.push_back(input);
							if (fsm->isOutputState()) newPath.push_back(STOUT_INPUT);
							fifo.push_back(make_pair(nextState, newPath));
						}
					}
				}

			}
		}
		TS.insert(CS);
		return TS;
	}

	sequence_in_t Ma_method(const unique_ptr<DFSM>& fsm, int extraStates) {
		auto TS = process_Ma(fsm, extraStates, false);
		if (TS.empty()) return sequence_in_t();
		return sequence_in_t(TS.begin()->begin(), TS.begin()->end());
	}

	sequence_set_t Mra_method(const unique_ptr<DFSM>& fsm, int extraStates) {
		return process_Ma(fsm, extraStates, true);
	}
}
