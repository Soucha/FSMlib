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
	state_t SVS_method(DFSM* fsm, sequence_set_t & TS, int extraStates) {
		TS.clear();
		if (extraStates < 0) return NULL_STATE;
		auto states = fsm->getStates();
		bool startWithStout = false;

		auto stateCover = getStateCover(fsm);
		auto traversalSet = getTraversalSet(fsm, extraStates);
		sequence_in_t emptySeq;
		traversalSet.insert(emptySeq);
		auto VSet = getVerifyingSet(fsm);
		auto SCSets = getStatesCharacterizingSets(fsm, getStatePairsShortestSeparatingSequences, false, reduceSCSet_EqualLength);
		
		state_t counter = 0;
		for (int i = 0; i < SCSets.size(); i++) {
			if (VSet[i].empty()) {
				counter++;
			}
			else {
				SCSets[i].clear();
				SCSets[i].insert(VSet[i]);
			}
		}

		if (fsm->isOutputState()) {
			for (auto seq : SCSets[0]) {
				if (seq.front() == STOUT_INPUT) {
					startWithStout = true;
					break;
				}
			}
			for (state_t i = 0; i < SCSets.size(); i++) {
				sequence_set_t tmp;
				for (auto origDS : SCSets[i]) {
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
				SCSets[i] = tmp;
			}
			extraStates *= 2; // STOUT_INPUT follows each input in traversalSet
		}

		FSMlib::PrefixSet pset;
		for (sequence_in_t trSeq : stateCover) {
			for (sequence_in_t extSeq : traversalSet) {
				sequence_in_t transferSeq(trSeq);
				transferSeq.insert(transferSeq.end(), extSeq.begin(), extSeq.end());
				state_t state = fsm->getEndPathState(0, transferSeq);
				if (state == WRONG_STATE) continue;
				state = getIdx(states, state);
				for (sequence_set_t SCSet : SCSets) {// i.e. VSet
					for (sequence_in_t cSeq : SCSet) {// usually only one seq = SVS
						sequence_in_t testSeq(transferSeq);
						if (startWithStout) {
							testSeq.push_front(STOUT_INPUT);
							testSeq.pop_back();// the last STOUT_INPUT (it will be at the beginning of appended cSeq)
						}
						testSeq.insert(testSeq.end(), cSeq.begin(), cSeq.end());
						pset.insert(testSeq);
					}
				}
				if (extSeq.size() == extraStates) {// check outcoming transitions
					//state_t state = getIdx(states, fsm->getEndPathState(0, transferSeq));
					for (input_t input = 0; input < fsm->getNumberOfInputs(); input++) {
						// usually only one seq = SVS is sufficient for transition verification
						//printf("%d-%d ", fsm->getNextState(state, input), SCSets[fsm->getNextState(state, input)].size());
						state_t nextState = fsm->getNextState(state, input);
						if (nextState == NULL_STATE) continue;
						nextState = getIdx(states, nextState);
						for (sequence_in_t cSeq : SCSets[nextState]) {
							sequence_in_t testSeq(transferSeq);
							testSeq.push_back(input);
							if (startWithStout) {
								testSeq.push_front(STOUT_INPUT);
							}
							else if (fsm->isOutputState()) {
								testSeq.push_back(STOUT_INPUT);
							}
							testSeq.insert(testSeq.end(), cSeq.begin(), cSeq.end());
							pset.insert(testSeq);
						}
					}
				}
			}
		}
		pset.getMaximalSequences(TS);
		return counter;
	}
}
