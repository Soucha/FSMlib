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
	void W_method(DFSM* fsm, sequence_set_t & TS, int extraStates) {
		TS.clear();
		if (extraStates < 0) {
			return;
		}

		sequence_set_t CSet, transitionCover, traversalSet;
		getTransitionCover(fsm, transitionCover, fsm->isOutputState());
		getTraversalSet(fsm, traversalSet, extraStates, fsm->isOutputState());
		getCharacterizingSet(fsm, CSet, getStatePairsShortestSeparatingSequences, false, reduceCSet_EqualLength);
		bool startWithStout = false;

		if (fsm->isOutputState()) {
			for (auto seq : CSet) {
				if (seq.front() == STOUT_INPUT) {
					startWithStout = true;
					break;
				}
			}
			sequence_set_t tmp;
			for (auto origDS : CSet) {
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
				tmp.insert(seq);
			}
			CSet = tmp;
		}

		FSMlib::PrefixSet pset;
		for (sequence_in_t trSeq : transitionCover) {
			if (fsm->getEndPathState(0, trSeq) == WRONG_STATE) continue;
			for (sequence_in_t cSeq : CSet) {
				sequence_in_t testSeq(trSeq);
				if (startWithStout) {
					testSeq.push_front(STOUT_INPUT);
					testSeq.pop_back();// the last STOUT_INPUT (it will be at the beginning of appended cSeq)
				}
				testSeq.insert(testSeq.end(), cSeq.begin(), cSeq.end());
				pset.insert(testSeq);
			}
			for (sequence_in_t extSeq : traversalSet) {
				for (sequence_in_t cSeq : CSet) {
					sequence_in_t testSeq(trSeq);
					testSeq.insert(testSeq.end(), extSeq.begin(), extSeq.end());
					if (fsm->getEndPathState(0, testSeq) == WRONG_STATE) continue;
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
