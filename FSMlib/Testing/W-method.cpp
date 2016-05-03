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
	sequence_set_t W_method(const unique_ptr<DFSM>& fsm, int extraStates) {
		if (extraStates < 0) {
			return sequence_set_t();
		}

		auto transitionCover = getTransitionCover(fsm);
		auto traversalSet = getTraversalSet(fsm, extraStates);
		auto CSet = getCharacterizingSet(fsm, getStatePairsShortestSeparatingSequences, false, reduceCSet_EqualLength);
		bool startWithStout = false;

		if (fsm->isOutputState()) {
			for (const auto& seq : CSet) {
				if (seq.front() == STOUT_INPUT) {
					startWithStout = true;
					break;
				}
			}
			sequence_set_t tmp;
			for (const auto& origDS : CSet) {
				sequence_in_t seq(origDS);
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
				tmp.emplace(move(seq));
			}
			CSet.swap(tmp);
		}

		FSMlib::PrefixSet pset;
		for (const auto& trSeq : transitionCover) {
			if (fsm->getEndPathState(0, trSeq) == WRONG_STATE) continue;
			for (const auto& cSeq : CSet) {
				sequence_in_t testSeq(trSeq);
				if (startWithStout) {
					testSeq.push_front(STOUT_INPUT);
					testSeq.pop_back();// the last STOUT_INPUT (it will be at the beginning of appended cSeq)
				}
				testSeq.insert(testSeq.end(), cSeq.begin(), cSeq.end());
				pset.insert(move(testSeq));
			}
			for (const auto& extSeq : traversalSet) {
				for (const auto& cSeq : CSet) {
					sequence_in_t testSeq(trSeq);
					testSeq.insert(testSeq.end(), extSeq.begin(), extSeq.end());
					if (fsm->getEndPathState(0, testSeq) == WRONG_STATE) continue;
					if (startWithStout) {
						testSeq.push_front(STOUT_INPUT);
						testSeq.pop_back();// the last STOUT_INPUT (it will be at the beginning of appended cSeq)
					}
					testSeq.insert(testSeq.end(), cSeq.begin(), cSeq.end());
					pset.insert(move(testSeq));
				}
			}
		}
		return pset.getMaximalSequences();
	}
}
