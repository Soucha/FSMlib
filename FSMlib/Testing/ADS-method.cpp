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
	sequence_set_t ADS_method(const unique_ptr<DFSM>& fsm, int extraStates) {
		auto ADSet = getAdaptiveDistinguishingSet(fsm);
		if ((extraStates < 0) || ADSet.empty()) {
			return sequence_set_t();
		}
		auto states = fsm->getStates();
		auto transitionCover = getTransitionCover(fsm);
		auto traversalSet = getTraversalSet(fsm, extraStates);
		bool startWithStout = false;

		if (fsm->isOutputState()) {
			startWithStout = (ADSet[0].front() == STOUT_INPUT);
			for (state_t i = 0; i < ADSet.size(); i++) {
				auto origDS = ADSet[i];
				auto DSit = ADSet[i].begin();
				for (auto it = origDS.begin(); it != origDS.end(); it++, DSit++) {
					if (*it == STOUT_INPUT) continue;
					it++;
					if ((it == origDS.end()) || (*it != STOUT_INPUT)) {
						ADSet[i].insert(++DSit, STOUT_INPUT);
						DSit--;
					}
					it--;
				}
			}
		}

		FSMlib::PrefixSet pset;
		for (const auto& trSeq : transitionCover) {
			sequence_in_t testSeq(trSeq);
			state_t state = fsm->getEndPathState(0, testSeq);
			if (state == WRONG_STATE) continue;
			state = getIdx(states, state);
			if (startWithStout) {
				testSeq.push_front(STOUT_INPUT);
				testSeq.pop_back();// the last STOUT_INPUT (it will be at the beginning of appended ADS)
			}
			testSeq.insert(testSeq.end(), ADSet[state].begin(), ADSet[state].end());
			pset.insert(move(testSeq));
			for (const auto& extSeq : traversalSet) {
				sequence_in_t testSeq(trSeq);
				testSeq.insert(testSeq.end(), extSeq.begin(), extSeq.end());
				state_t state = fsm->getEndPathState(0, testSeq);
				if (state == WRONG_STATE) continue;
				state = getIdx(states, state);
				if (startWithStout) {
					testSeq.push_front(STOUT_INPUT);
					testSeq.pop_back();// the last STOUT_INPUT (it will be at the beginning of appended ADS)
				}
				testSeq.insert(testSeq.end(), ADSet[state].begin(), ADSet[state].end());
				pset.insert(move(testSeq));
			}
		}
		return pset.getMaximalSequences();
	}
}
