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

#include "Testing.h"
#include "../PrefixSet.h"

using namespace FSMsequence;

namespace FSMtesting {
	bool PDS_method(DFSM* fsm, sequence_set_t & TS, int extraStates) {
		sequence_in_t DS;
		TS.clear();
		if (!getPresetDistinguishingSequence(fsm, DS)) {
			return false;
		}

		sequence_set_t transitionCover, traversalSet;
		getTransitionCover(fsm, transitionCover, fsm->isOutputState());
		getTraversalSet(fsm, traversalSet, extraStates, fsm->isOutputState());
		sequence_in_t origDS = DS;

		if (fsm->isOutputState()) {
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
			if (DS.front() == STOUT_INPUT) DS.pop_front();
		}

		FSMlib::PrefixSet pset;
		for (sequence_in_t trSeq : transitionCover) {
			sequence_in_t testSeq(trSeq);
			testSeq.insert(testSeq.end(), DS.begin(), DS.end());
			pset.insert(testSeq);
			for (sequence_in_t extSeq : traversalSet) {
				sequence_in_t testSeq(trSeq);
				testSeq.insert(testSeq.end(), extSeq.begin(), extSeq.end());
				testSeq.insert(testSeq.end(), DS.begin(), DS.end());
				pset.insert(testSeq);
			}
		}
		pset.getMaximalSequences(TS);

		if (origDS.front() == STOUT_INPUT) {
			if (DS.size() > 0) {
				if (pset.contains(DS) == DS.size()) {// DS without \eps is in TS
					sequence_set_t::reverse_iterator tsIt;
					sequence_in_t::const_iterator sIt, tIt;
					for (tsIt = TS.rbegin(); tsIt != TS.rend(); tsIt++) {
						sIt = DS.begin();
						tIt = (*tsIt).begin();
						for (; sIt != DS.end(); sIt++, tIt++) {
							if (*sIt != *tIt) break;
						}
						if (sIt == DS.end()) break;
					}
					DS = *tsIt;
					DS.push_front(STOUT_INPUT);
					sequence_set_t::iterator tmpIt = tsIt.base();
					tmpIt--;
#ifdef DEBUG
					printf("Change: %s with %s - DS: %s\n", FSMmodel::getInSequenceAsString(*tmpIt).c_str(),
						FSMmodel::getInSequenceAsString(DS).c_str(),
						FSMmodel::getInSequenceAsString(DS).c_str());
#endif
					TS.erase(--tsIt.base());
				}
				TS.insert(DS);
			}
			else {// origDS = STOUT_INPUT
				origDS.insert(origDS.end(), (*TS.begin()).begin(), (*TS.begin()).end());
#ifdef DEBUG
				printf("Change: %s with %s - DS: %s\n", FSMmodel::getInSequenceAsString(*TS.begin()).c_str(),
					FSMmodel::getInSequenceAsString(origDS).c_str(),
					FSMmodel::getInSequenceAsString(origDS).c_str());
#endif
				TS.erase(TS.begin());
				TS.insert(origDS);
			}
		}
		return true;
	}
}
