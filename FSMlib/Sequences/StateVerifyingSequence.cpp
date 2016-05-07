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
	typedef set<state_t> block_t;

	struct svs_node_t {
		block_t undistinguishedStates;
		sequence_in_t svs;
		state_t state;

		svs_node_t(block_t states, sequence_in_t svs, state_t state) :
			undistinguishedStates(states), svs(svs), state(state) {
		}
	};

	extern int MAX_CLOSED;
#if SEQUENCES_PERFORMANCE_TEST
	extern string testOut;
	extern int closedCount, openCount;
#endif // SEQUENCES_PERFORMANCE_TEST
	
	sequence_in_t getStateVerifyingSequence(const unique_ptr<DFSM>& fsm, state_t state) {
		RETURN_IF_NONCOMPACT(fsm, "FSMsequence::getStateVerifyingSequence", sequence_in_t());
		sequence_in_t outSVS;
		block_t states;
		output_t output;
		sequence_in_t s;
		if (fsm->isOutputState()) {
			output = fsm->getOutput(state, STOUT_INPUT);
			for (state_t state = 0; state < fsm->getNumberOfStates(); state++) {
				if (fsm->getOutput(state, STOUT_INPUT) == output) {
					states.insert(state);
				}
			}
			// has state unique output?
			if (states.size() == 1) {
				outSVS.push_back(STOUT_INPUT);
				return outSVS;
			}
			// some states may be filtered by eps
			if (states.size() < fsm->getNumberOfStates()) {
				s.push_back(STOUT_INPUT);
			}
		}
		else {// TYPE_MEALY
			// all states are initially undistinguished
			for (state_t state = 0; state < fsm->getNumberOfStates(); state++) {
				states.emplace(state);
			}
		}
		queue<unique_ptr<svs_node_t>> fifo;
		set< pair<block_t, state_t> > used;
		state_t nextState;
		bool badInput, stoutUsed = false;

		fifo.emplace(make_unique<svs_node_t>(states, s, state));
		used.emplace(move(states), state);
		while (!fifo.empty()) {
			auto act = move(fifo.front());
			fifo.pop();
			for (input_t input = 0; input < fsm->getNumberOfInputs(); input++) {
				block_t states;
				nextState = fsm->getNextState(act->state, input);
				output = fsm->getOutput(act->state, input);
				badInput = false;
				for (const auto& state : act->undistinguishedStates) {
					if (output == fsm->getOutput(state, input)) {
						// is state undistinguishable from fixed state?
						if ((nextState == fsm->getNextState(state, input))
								// holds even if there is no transition
								&& (state != act->state)) {
							// other state goes to the same state under this input
							badInput = true;
							break;
						}
						states.emplace(fsm->getNextState(state, input));
					}
				}
				if (badInput) {
					continue;
				}
				stoutUsed = false;
				if (fsm->isOutputState() && (states.size() > 1)) {
					block_t tmp;
					output = fsm->getOutput(nextState, STOUT_INPUT);
					for (const auto& i : states) {
						if (fsm->getOutput(i, STOUT_INPUT) == output) {
							tmp.emplace(i);
						}
					}
					// is reference state distinguished by appending STOUT_INPUT?
					if (tmp.size() != states.size()) {
						states.swap(tmp);
						stoutUsed = true;
					}
				}
				// is SVS found?
				if (states.size() == 1) {
					outSVS.swap(act->svs);
					outSVS.push_back(input);
					if (stoutUsed) outSVS.push_back(STOUT_INPUT);
#if SEQUENCES_PERFORMANCE_TEST
					openCount += fifo.size();
					closedCount += used.size();
#endif // SEQUENCES_PERFORMANCE_TEST
					return outSVS;
				}
				// has already the same group of states analysed?
				if (used.find(make_pair(states, nextState)) == used.end()) {
					sequence_in_t s(act->svs);
					s.push_back(input);
					if (stoutUsed) s.push_back(STOUT_INPUT);
					fifo.emplace(make_unique<svs_node_t>(states, s, nextState));
					used.emplace(move(states), nextState);
				}
			}
		}
#if SEQUENCES_PERFORMANCE_TEST
		openCount += fifo.size();
		closedCount += used.size();
#endif // SEQUENCES_PERFORMANCE_TEST
		return outSVS;
	}

	sequence_vec_t getVerifyingSet(const unique_ptr<DFSM>& fsm) {
		RETURN_IF_NONCOMPACT(fsm, "FSMsequence::getVerifyingSet", sequence_vec_t());
		sequence_vec_t outVSet;
#if SEQUENCES_PERFORMANCE_TEST
		openCount = closedCount = 0;
#endif // SEQUENCES_PERFORMANCE_TEST
		for (state_t state : fsm->getStates()) {
			outVSet.emplace_back(getStateVerifyingSequence(fsm, state));
		}
#if SEQUENCES_PERFORMANCE_TEST
		stringstream ss;
		ss << closedCount << ';' << openCount << ';';
		testOut += ss.str();
		//printf("%d;%d;", closedCount, openCount);
#endif // SEQUENCES_PERFORMANCE_TEST
		return outVSet;
	}
}
