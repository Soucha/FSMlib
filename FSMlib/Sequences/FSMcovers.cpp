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

#include "FSMsequence.h"

namespace FSMsequence {
	sequence_set_t getStateCover(DFSM * dfsm) {
		sequence_set_t stateCover;
		vector<bool> covered(dfsm->getGreatestStateId(), false);
		queue< pair<state_t, sequence_in_t> > fifo;
		
		// empty sequence
		stateCover.emplace(sequence_in_t());

		covered[0] = true;
		fifo.emplace(0, sequence_in_t());
		while (!fifo.empty()) {
			auto current = fifo.front();
			fifo.pop();
			for (input_t input = 0; input < dfsm->getNumberOfInputs(); input++) {
				auto nextState = dfsm->getNextState(current.first, input);
				if ((nextState != NULL_STATE) && !covered[nextState]) {
					covered[nextState] = true;
					sequence_in_t newPath(current.second);
					newPath.push_back(input);
					if (dfsm->isOutputState()) newPath.push_back(STOUT_INPUT);
					stateCover.emplace(newPath);
					fifo.emplace(nextState, newPath);
				}
			}
		}
		return stateCover;
	}

	sequence_set_t getTransitionCover(DFSM * dfsm) {
		sequence_set_t transitionCover;
		vector<bool> covered(dfsm->getGreatestStateId(), false);
		queue< pair<state_t, sequence_in_t> > fifo;
		
		// empty sequence
		transitionCover.emplace(sequence_in_t());

		covered[0] = true;
		fifo.emplace(0, sequence_in_t());
		while (!fifo.empty()) {
			auto current = fifo.front();
			fifo.pop();
			for (input_t input = 0; input < dfsm->getNumberOfInputs(); input++) {
				auto nextState = dfsm->getNextState(current.first, input);
				if (nextState != NULL_STATE) {
					sequence_in_t newPath(current.second);
					newPath.push_back(input);
					if (dfsm->isOutputState()) newPath.push_back(STOUT_INPUT);
					transitionCover.emplace(newPath);
					if (!covered[nextState]) {
						covered[nextState] = true;
						fifo.emplace(nextState, newPath);
					}
				}
			}
		}
		return transitionCover;
	}

	sequence_set_t getTraversalSet(DFSM * dfsm, seq_len_t depth) {
		sequence_set_t traversalSet;
		if (depth <= 0) return traversalSet;
		queue<sequence_in_t> fifo;
		fifo.emplace(sequence_in_t());
		if (dfsm->isOutputState()) depth *= 2; // STOUT_INPUT follows each input
		while (!fifo.empty()) {
			auto seq = fifo.front();
			fifo.pop();
			for (input_t input = 0; input < dfsm->getNumberOfInputs(); input++) {
				sequence_in_t extSeq(seq);
				extSeq.push_back(input);
				if (dfsm->isOutputState()) extSeq.push_back(STOUT_INPUT);
				traversalSet.emplace(extSeq);
				if (extSeq.size() < depth) fifo.emplace(extSeq);
			}
		}
		return traversalSet;
	}
}
