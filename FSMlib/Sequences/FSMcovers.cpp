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
	void getStateCover(DFSM * dfsm, sequence_set_t & stateCover) {
		vector<bool> covered(dfsm->getGreatestStateId(), false);
		queue< pair<state_t, sequence_in_t> > fifo;
		pair<state_t, sequence_in_t> current;
		sequence_in_t s;
		state_t nextState;

		stateCover.clear();
		// empty sequence
		stateCover.insert(s);

		covered[0] = true;
		fifo.push(make_pair(0, s));
		while (!fifo.empty()) {
			current = fifo.front();
			fifo.pop();
			for (input_t input = 0; input < dfsm->getNumberOfInputs(); input++) {
				nextState = dfsm->getNextState(current.first, input);
				if ((nextState != NULL_STATE) && !covered[nextState]) {
					covered[nextState] = true;
					sequence_in_t newPath(current.second);
					newPath.push_back(input);
					if (dfsm->isOutputState()) newPath.push_back(STOUT_INPUT);
					stateCover.insert(newPath);
					fifo.push(make_pair(nextState, newPath));
				}
			}
		}
	}

	void getTransitionCover(DFSM * dfsm, sequence_set_t & transitionCover) {
		vector<bool> covered(dfsm->getGreatestStateId(), false);
		queue< pair<state_t, sequence_in_t> > fifo;
		pair<state_t, sequence_in_t> current;
		sequence_in_t s;
		state_t nextState;

		transitionCover.clear();
		// empty sequence
		transitionCover.insert(s);

		covered[0] = true;
		fifo.push(make_pair(0, s));
		while (!fifo.empty()) {
			current = fifo.front();
			fifo.pop();
			for (input_t input = 0; input < dfsm->getNumberOfInputs(); input++) {
				nextState = dfsm->getNextState(current.first, input);
				if (nextState != NULL_STATE) {
					sequence_in_t newPath(current.second);
					newPath.push_back(input);
					if (dfsm->isOutputState()) newPath.push_back(STOUT_INPUT);
					transitionCover.insert(newPath);
					if (!covered[nextState]) {
						covered[nextState] = true;
						fifo.push(make_pair(nextState, newPath));
					}
				}
			}
		}
	}

	void getTraversalSet(DFSM * dfsm, sequence_set_t& traversalSet, seq_len_t depth) {
		traversalSet.clear();
		if (depth <= 0) return;
		queue<sequence_in_t> fifo;
		sequence_in_t seq;
		fifo.push(seq);
		if (dfsm->isOutputState()) depth *= 2; // STOUT_INPUT follows each input
		while (!fifo.empty()) {
			seq = fifo.front();
			fifo.pop();
			for (input_t input = 0; input < dfsm->getNumberOfInputs(); input++) {
				sequence_in_t extSeq(seq);
				extSeq.push_back(input);
				if (dfsm->isOutputState()) extSeq.push_back(STOUT_INPUT);
				traversalSet.insert(extSeq);
				if (extSeq.size() < depth) fifo.push(extSeq);
			}
		}
	}
}
