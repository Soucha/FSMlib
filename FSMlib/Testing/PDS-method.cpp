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
	bool PDS_method(DFSM* fsm, sequence_set_t & TS, int extraStates) {
		sequence_in_t DS;
		TS.clear();
		if ((extraStates < 0) || !getPresetDistinguishingSequence(fsm, DS)) {
			return false;
		}

		sequence_set_t transitionCover, traversalSet;
		getTransitionCover(fsm, transitionCover);
		getTraversalSet(fsm, traversalSet, extraStates);
		bool startWithStout = false;

		if (fsm->isOutputState()) {
			startWithStout = (DS.front() == STOUT_INPUT);
			auto origDS = DS;
			auto DSit = DS.begin();
			for (auto it = origDS.begin(); it != origDS.end(); it++, DSit++) {
				if (*it == STOUT_INPUT) continue;
				it++;
				if ((it == origDS.end()) || (*it != STOUT_INPUT)) {
					DS.insert(++DSit, STOUT_INPUT);
					DSit--;
				}
				it--;
			}
		}

		FSMlib::PrefixSet pset;
		for (sequence_in_t trSeq : transitionCover) {
			sequence_in_t testSeq(trSeq);
			if (fsm->getEndPathState(0, testSeq) == WRONG_STATE) continue;
			if (startWithStout) {
				testSeq.push_front(STOUT_INPUT);
				testSeq.pop_back();// the last STOUT_INPUT (it will be at the beginning of appended PDS)
			}
			testSeq.insert(testSeq.end(), DS.begin(), DS.end());
			pset.insert(testSeq);
			for (sequence_in_t extSeq : traversalSet) {
				sequence_in_t testSeq(trSeq);
				testSeq.insert(testSeq.end(), extSeq.begin(), extSeq.end());
				if (fsm->getEndPathState(0, testSeq) == WRONG_STATE) continue;
				if (startWithStout) {
					testSeq.push_front(STOUT_INPUT);
					testSeq.pop_back();// the last STOUT_INPUT (it will be at the beginning of appended PDS)
				}
				testSeq.insert(testSeq.end(), DS.begin(), DS.end());
				pset.insert(testSeq);
			}
		}
		pset.getMaximalSequences(TS);
		return true;
	}
}
