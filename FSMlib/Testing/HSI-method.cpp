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
#include "../PrefixSet.h"

using namespace FSMsequence;

namespace FSMtesting {
	void HSI_method(DFSM* fsm, sequence_set_t & TS, int extraStates) {
		TS.clear();
		if (extraStates < 0) {
			return;
		}
		auto states = fsm->getStates();
		vector<sequence_set_t> H;
		sequence_set_t transitionCover, traversalSet;
		getTransitionCover(fsm, transitionCover, fsm->isOutputState());
		getTraversalSet(fsm, traversalSet, extraStates, fsm->isOutputState());
		getHarmonizedStateIdentifiers(fsm, H);
		bool startWithStout = false;

		if (fsm->isOutputState()) {
			for (auto seq : H[0]) {
				if (seq.front() == STOUT_INPUT) {
					startWithStout = true;
					break;
				}
			}
			for (state_t i = 0; i < H.size(); i++) {
				sequence_set_t tmp;
				for (auto origDS : H[i]) {
					sequence_in_t seq = origDS;
					auto DSit = seq.begin();
					for (auto it = origDS.begin(); it != origDS.end(); it++, DSit++) {
						if (*it == STOUT_INPUT) continue;
						it++;
						if ((it == origDS.end()) || (*it != STOUT_INPUT)) {
							seq.insert(++DSit, STOUT_INPUT);
							DSit--;
						}
						it--;
					}
					if (startWithStout) {
						if (seq.front() != STOUT_INPUT) seq.push_front(STOUT_INPUT);
					}
					else if (seq.front() == STOUT_INPUT) seq.pop_front();
					tmp.insert(seq);
				}
				H[i] = tmp;
			}
		}

		FSMlib::PrefixSet pset;
		for (sequence_in_t trSeq : transitionCover) {
			state_t state = fsm->getEndPathState(0, trSeq);
			if (state == WRONG_STATE) continue;
			state = getIdx(states, state);
			for (sequence_in_t cSeq : H[state]) {
				sequence_in_t testSeq(trSeq);
				if (startWithStout) {
					testSeq.push_front(STOUT_INPUT);
					testSeq.pop_back();// the last STOUT_INPUT (it will be at the beginning of appended cSeq)
				}
				testSeq.insert(testSeq.end(), cSeq.begin(), cSeq.end());
				pset.insert(testSeq);
			}
			for (sequence_in_t extSeq : traversalSet) {
				state_t endState = fsm->getEndPathState(state, extSeq);
				if (endState == WRONG_STATE) continue;
				endState = getIdx(states, endState);
				for (sequence_in_t cSeq : H[endState]) {
					sequence_in_t testSeq(trSeq);
					testSeq.insert(testSeq.end(), extSeq.begin(), extSeq.end());
					if (startWithStout) {
						testSeq.push_front(STOUT_INPUT);
						testSeq.pop_back();// the last STOUT_INPUT (it will be at the beginning of appended cSeq)
					}
					testSeq.insert(testSeq.end(), cSeq.begin(), cSeq.end());
					pset.insert(testSeq);
				}
			}
		}
		pset.getMaximalSequences(TS);
	}
}