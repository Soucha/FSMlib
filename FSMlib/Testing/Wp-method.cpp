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
	sequence_set_t Wp_method(const unique_ptr<DFSM>& fsm, int extraStates) {
		RETURN_IF_NONCOMPACT(fsm, "FSMtesting::Wp_method", sequence_set_t());
		if (extraStates < 0) {
			return sequence_set_t();
		}
		auto stateCover = getStateCover(fsm);
		auto traversalSet = getTraversalSet(fsm, extraStates);
		traversalSet.emplace(sequence_in_t());
		auto SCSets = getStatesCharacterizingSets(fsm, getStatePairsShortestSeparatingSequences, false, reduceSCSet_EqualLength);
		bool startWithStout = false;
		FSMlib::PrefixSet pset;

		if (fsm->isOutputState()) {
			for (const auto& seq : SCSets[0]) {
				if (seq.front() == STOUT_INPUT) {
					startWithStout = true;
					break;
				}
			}
			for (state_t i = 0; i < SCSets.size(); i++) {
				sequence_set_t tmp;
				for (const auto& origDS : SCSets[i]) {
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
					else if (seq.front() == STOUT_INPUT) seq.pop_front();
					tmp.emplace(seq);
					// CSet design
					pset.insert(move(seq));
				}
				SCSets[i].swap(tmp);
			}
			extraStates *= 2; // STOUT_INPUT follows each input in traversalSet
		}
		else {
			// CSet design
			for (const auto& SCSet : SCSets) {
				for (const auto& seq : SCSet) {
					pset.insert(seq);
				}
			}	
		}
		auto CSet = pset.getMaximalSequences();
		pset.clear();
	
		for (const auto& trSeq : stateCover) {
			for (const auto& extSeq : traversalSet) {
				sequence_in_t transferSeq(trSeq);
				transferSeq.insert(transferSeq.end(), extSeq.begin(), extSeq.end());
				state_t state = fsm->getEndPathState(0, transferSeq);
				if (state == WRONG_STATE) continue;
				for (const auto& cSeq : CSet) {
					sequence_in_t testSeq(transferSeq);
					if (startWithStout) {
						testSeq.push_front(STOUT_INPUT);
						testSeq.pop_back();// the last STOUT_INPUT (it will be at the beginning of appended cSeq)
					}
					testSeq.insert(testSeq.end(), cSeq.begin(), cSeq.end());
					pset.insert(move(testSeq));
				}
				if (extSeq.size() == extraStates) {// check outcoming transitions
					//state_t state = getIdx(states, fsm->getEndPathState(0, transferSeq));
					for (input_t input = 0; input < fsm->getNumberOfInputs(); input++) {
						// SCSet is sufficient for transition verification
						state_t nextState = fsm->getNextState(state, input);
						if (nextState == NULL_STATE) continue;
						for (const auto& cSeq : SCSets[nextState]) {
							sequence_in_t testSeq(transferSeq);
							testSeq.push_back(input);
							if (startWithStout) {
								testSeq.push_front(STOUT_INPUT);
							}
							else if (fsm->isOutputState()) {
								testSeq.push_back(STOUT_INPUT);
							}
							testSeq.insert(testSeq.end(), cSeq.begin(), cSeq.end());
							pset.insert(move(testSeq));
						}
					}
				}
			}
		}
		return pset.getMaximalSequences();
	}
}
