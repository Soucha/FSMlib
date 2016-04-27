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

using namespace FSMsequence;

namespace FSMtesting {
	struct ver_seq_t {
		input_t input = STOUT_INPUT;
		ver_seq_t *neighbor = NULL, *child = NULL;

		~ver_seq_t() {
			if (neighbor) delete neighbor;
			if (child) delete child;
		}
	};

	struct TestNodeC {
		bool confirmed = false;
		state_t state;
		output_t stateOutput;
		output_t output;
		input_t input;

		TestNodeC(input_t input, output_t output, state_t state, output_t stateOutput) :
			input(input), output(output), state(state), stateOutput(stateOutput) {
		}
	};

	static vector<vector<bool>> verifiedTransition;
	static vector<int> verifiedState;
	static vector<vector<int>> confirmedNodes;
	static vector<ver_seq_t*> verSeq;
	static vector<TestNodeC*> cs;
	static queue<int> newlyConfirmed;
	static seq_len_t counter, currIdx, lastConfIdx;
	
	static bool equalSeqPart(seq_len_t idx, sequence_in_t::iterator & itE, sequence_in_t::iterator endE) {
		while ((idx != cs.size()) && (itE != endE)) {
			if (cs[idx]->input != *itE)
				return false;
			++idx;
			++itE;
		}
		return true;
	}

	static ver_seq_t* findNext(ver_seq_t* vs, input_t input) {
		auto nextVs = vs->child;
		while ((nextVs != NULL) && (nextVs->input != input)) {
			nextVs = nextVs->neighbor;
		}
		return nextVs;
	}

	static void shortenTo(ver_seq_t* & vs, sequence_in_t seq) {
		if (seq.empty()) {
			delete vs->child;
			vs->child = NULL;
			return;
		}
		auto nextVs = findNext(vs, seq.front());
		if (nextVs == NULL) {
			if (vs->child == NULL) {
				vs->child = new ver_seq_t;
				nextVs = vs->child;
			}
			else {
				nextVs = vs->child;
				while (nextVs->neighbor != NULL) {
					nextVs = nextVs->neighbor;
				}
				nextVs->neighbor = new ver_seq_t;
				nextVs = nextVs->neighbor;
			}
			nextVs->input = seq.front();
		}
		seq.pop_front();
		shortenTo(nextVs, seq);
	}

	static void checkSucc(seq_len_t idx, sequence_in_t & seq) {
		seq_len_t aIdx = idx;
		for (auto input : seq) {
			aIdx++;
			if ((aIdx == cs.size()) || (cs[aIdx]->input != input)) return;
		}
		if (!cs[aIdx]->confirmed) {
			cs[aIdx]->confirmed = true;
			newlyConfirmed.push(aIdx);
		}
	}

	static void update(seq_len_t idx);

	static void processNewlyConfirmed() {
		while (!newlyConfirmed.empty()) {
			seq_len_t idx = newlyConfirmed.front();
			if ((idx > lastConfIdx) && (idx <= currIdx)) lastConfIdx = idx;
			confirmedNodes[cs[idx]->state].push_back(idx);
			auto vs = verSeq[cs[idx]->state];
			for (++idx; idx < cs.size(); idx++) {
				auto nextVs = findNext(vs, cs[idx]->input);
				if (nextVs == NULL) {
					for (; idx < cs.size(); idx++) {
						if (cs[idx]->confirmed) {
							update(newlyConfirmed.front());
							break;
						}
					}
					break;
				}
				if (nextVs->child == NULL) {// leaf
					if (!cs[idx]->confirmed) {
						cs[idx]->confirmed = true;
						newlyConfirmed.push(idx);
					}
					break;
				}
				if (cs[idx]->confirmed) {
					update(newlyConfirmed.front());
					break;
				}
				vs = nextVs;
			}
			newlyConfirmed.pop();
		}
	}

	static void update(seq_len_t idx) {
		sequence_in_t seq;
		seq_len_t actIdx = idx;
		do {
			actIdx++;
			seq.push_back(cs[actIdx]->input);
		} while (!cs[actIdx]->confirmed);
		if ((seq.size() == 1) && (!verifiedTransition[cs[idx]->state][seq.front()])) {// a transition verified
			verifiedTransition[cs[idx]->state][seq.front()] = true;
			verifiedState[cs[idx]->state]--;
			counter--;
		}
		shortenTo(verSeq[cs[idx]->state], seq);
		for (auto nIdx : confirmedNodes[cs[idx]->state]) {
			checkSucc(nIdx, seq);
		}
	}

	static void getCS(sequence_in_t& CS, bool stoutUsed) {
		CS.clear();
		for (auto n : cs) {
			CS.push_back(n->input);
			if (stoutUsed) CS.push_back(STOUT_INPUT);
		}
		CS.pop_front();
	}

	static int equalLength(sequence_out_t::iterator f, sequence_out_t::iterator s, int maxLen = -1) {
		int len = 0;
		while ((maxLen - len != 0) && (*f == *s)) {
			f++;
			s++;
			len++;
		}
		return len;
	}

	bool C_method(DFSM* fsm, sequence_in_t& CS, int extraStates) {
		AdaptiveDS* ADS;
		CS.clear();
		if (!getAdaptiveDistinguishingSequence(fsm, ADS)) {
			return false;
		}
		auto states = fsm->getStates();
		state_t N = fsm->getNumberOfStates(), P = fsm->getNumberOfInputs(), currState, nextState;
		sequence_vec_t E(N);

		getADSet(fsm, ADS, E);
		delete ADS;
		/* // Example from simao2009checking
		E[0].push_back(0);
		E[0].push_back(1);
		E[0].push_back(0);
		E[1].push_back(0);
		E[1].push_back(1);
		E[1].push_back(0);
		E[2].push_back(0);
		E[2].push_back(1);
		E[3].push_back(0);
		E[3].push_back(1);
		E[4].push_back(0);
		E[4].push_back(1);
		//*/
		counter = N * fsm->getNumberOfInputs();
		verifiedTransition.clear();
		verifiedTransition.resize(N);
		verifiedState.clear();
		verifiedState.resize(N, P);
		verSeq.clear();
		verSeq.resize(N);
		for (state_t i = 0; i < N; i++) {
			verifiedTransition[i].assign(P, false);
			verSeq[i] = new ver_seq_t;
		}
		confirmedNodes.clear();
		confirmedNodes.resize(N);
		sequence_in_t::iterator nextInput;
		if (fsm->isOutputState()) {
			for (state_t i = 0; i < N; i++) {
				sequence_in_t seq;
				for (auto input : E[i]) {
					if (input == STOUT_INPUT) continue;
					seq.push_back(input);
				}
				E[i] = seq;
			}
		}
		output_t outputState = (fsm->isOutputState()) ? fsm->getOutput(0, STOUT_INPUT) : DEFAULT_OUTPUT;
		output_t outputTransition;
		TestNodeC* node = new TestNodeC(STOUT_INPUT, DEFAULT_OUTPUT, 0, outputState);
		cs.clear();
		cs.push_back(node);
		currState = 0;
		for (auto input : E[0]) {
			nextState = fsm->getNextState(currState, input);
			outputState = (fsm->isOutputState()) ? fsm->getOutput(nextState, STOUT_INPUT) : DEFAULT_OUTPUT;
			outputTransition = (fsm->isOutputTransition()) ? fsm->getOutput(currState, input) : DEFAULT_OUTPUT;
			node = new TestNodeC(input, outputTransition, getIdx(states, nextState), outputState);
			cs.push_back(node);
			currState = nextState;
		}
		confirmedNodes[0].push_back(0);
		currIdx = 1;
		lastConfIdx = 0;
		currState = 0;

		while (counter > 0) {
			//getCS(CS, fsm->isOutputState());
			//printf("%u/%u %u %s\n", currIdx, cs.size(), lastConfIdx, FSMmodel::getInSequenceAsString(CS).c_str());
			if (cs.back()->confirmed) {
				currIdx = seq_len_t(cs.size());
				lastConfIdx = currIdx - 1;
			}
			if (currIdx < cs.size()) {
				if (!cs[currIdx]->confirmed) {
					nextState = cs[currIdx]->state;
					nextInput = E[nextState].begin();
					if (equalSeqPart(currIdx + 1, nextInput, E[nextState].end())) {
						currState = cs.back()->state;
						for (; nextInput != E[cs[currIdx]->state].end(); nextInput++) {
							nextState = fsm->getNextState(states[currState], *nextInput);
							outputState = (fsm->isOutputState()) ? fsm->getOutput(nextState, STOUT_INPUT) : DEFAULT_OUTPUT;
							outputTransition = (fsm->isOutputTransition()) ? fsm->getOutput(states[currState], *nextInput) : DEFAULT_OUTPUT;
							nextState = getIdx(states, nextState);
							node = new TestNodeC(*nextInput, outputTransition, nextState, outputState);
							cs.push_back(node);
							currState = nextState;
						}
						cs[currIdx]->confirmed = true;
						newlyConfirmed.push(currIdx);
						update(lastConfIdx);
						processNewlyConfirmed();
					}
				}
				else {
					lastConfIdx = currIdx;
				}
				currIdx++;
			}
			else if (verifiedState[cs.back()->state] > 0) {
				currState = cs.back()->state;
				for (input_t input = 0; input < P; input++) {
					if (!verifiedTransition[currState][input]) {
						nextState = fsm->getNextState(states[currState], input);
						outputState = (fsm->isOutputState()) ? fsm->getOutput(nextState, STOUT_INPUT) : DEFAULT_OUTPUT;
						outputTransition = (fsm->isOutputTransition()) ? fsm->getOutput(states[currState], input) : DEFAULT_OUTPUT;
						nextState = getIdx(states, nextState);
						node = new TestNodeC(input, outputTransition, nextState, outputState);
						node->confirmed = true;
						cs.push_back(node);
						newlyConfirmed.push(currIdx); //cs.size()-1

						sequence_in_t seqE(E[nextState]);
						// output-confirmed
						if (!E[currState].empty() && input == E[currState].front()) {
							sequence_in_t suf(E[currState]);
							suf.pop_front();
							auto outSuf = fsm->getOutputAlongPath(states[nextState], suf);
							auto outE = fsm->getOutputAlongPath(states[nextState], E[nextState]);
							//printf("(%d,%d,%d) %s/%s %s\n", currState, input, nextState,
							//      FSMmodel::getInSequenceAsString(suf).c_str(),
							//    FSMmodel::getOutSequenceAsString(outSuf).c_str(),
							//  FSMmodel::getOutSequenceAsString(outE).c_str());
							int lenE = 0; //outE.size();
							for (state_t i = 0; i < N; i++) {
								if (i != nextState) {
									auto outSufI = fsm->getOutputAlongPath(states[i], suf);
									auto osl = equalLength(outSuf.begin(), outSufI.begin(), outSuf.size());
									if (osl != outSuf.size()) {
										bool outConfirmed = false;
										auto sufIt = suf.begin();
										while (osl-- >= 0) sufIt++;
										for (auto cnIdx : confirmedNodes[i]) {
											auto sufBeginIt = suf.begin();
											if (equalSeqPart(cnIdx + 1, sufBeginIt, sufIt)) {
												outConfirmed = true;
												break;
											}
										}

										if (outConfirmed) {
											continue;
											/*
											outConfirmed = false;
											for (auto cnIdx : confirmedNodes[nextState]) {
											auto sufBeginIt = suf.begin();
											if (equalSeqPart(cnIdx, sufBeginIt, sufIt)) {
											outConfirmed = true;
											break;
											}
											}
											if (outConfirmed) {
											continue;
											}
											*/
										}
									}
									auto outI = fsm->getOutputAlongPath(states[i], E[nextState]);
									auto oel = 1 + equalLength(outE.begin(), outI.begin());
									//printf("%s/%s x %s %d %d-%d\n", 
									//      FSMmodel::getInSequenceAsString(E[nextState]).c_str(),
									//    FSMmodel::getOutSequenceAsString(outE).c_str(),
									//  FSMmodel::getOutSequenceAsString(outI).c_str(),
									//oel, lenE, outE.size());
									if (oel > lenE) {
										lenE = oel;
										if (lenE == outE.size()) {// entire E is needed
											break;
										}
									}
								}
							}
							// adjust E
							for (; lenE < outE.size(); lenE++) {
								seqE.pop_back();
							}
						}
						currState = nextState;
						for (auto input : seqE) {
							nextState = fsm->getNextState(states[currState], input);
							outputState = (fsm->isOutputState()) ? fsm->getOutput(nextState, STOUT_INPUT) : DEFAULT_OUTPUT;
							outputTransition = (fsm->isOutputTransition()) ? fsm->getOutput(states[currState], input) : DEFAULT_OUTPUT;
							nextState = getIdx(states, nextState);
							node = new TestNodeC(input, outputTransition, nextState, outputState);
							cs.push_back(node);
							currState = nextState;
						}
						update(currIdx - 1);
						processNewlyConfirmed();
						break;
					}
				}
			}
			else {// find unverified transition
				vector<bool> covered(N, false);
				list< pair<state_t, sequence_in_t> > fifo;
				sequence_in_t s;
				currState = cs.back()->state;
				covered[currState] = true;
				fifo.push_back(make_pair(currState, s));
				while (!fifo.empty()) {
					auto current = fifo.front();
					fifo.pop_front();
					for (input_t input = 0; input < P; input++) {
						nextState = fsm->getNextState(states[current.first], input);
						if (nextState == WRONG_STATE) continue;
						nextState = getIdx(states, nextState);
						if (verifiedState[nextState] > 0) {
							for (nextInput = current.second.begin(); nextInput != current.second.end(); nextInput++) {
								nextState = fsm->getNextState(states[currState], *nextInput);
								outputState = (fsm->isOutputState()) ? fsm->getOutput(nextState, STOUT_INPUT) : DEFAULT_OUTPUT;
								outputTransition = (fsm->isOutputTransition()) ? fsm->getOutput(states[currState], *nextInput) : DEFAULT_OUTPUT;
								nextState = getIdx(states, nextState);
								node = new TestNodeC(*nextInput, outputTransition, nextState, outputState);
								node->confirmed = true;
								cs.push_back(node);
								currState = nextState;
							}
							nextState = fsm->getNextState(states[currState], input);
							outputState = (fsm->isOutputState()) ? fsm->getOutput(nextState, STOUT_INPUT) : DEFAULT_OUTPUT;
							outputTransition = (fsm->isOutputTransition()) ? fsm->getOutput(states[currState], input) : DEFAULT_OUTPUT;
							nextState = getIdx(states, nextState);
							node = new TestNodeC(input, outputTransition, nextState, outputState);
							node->confirmed = true;
							lastConfIdx = seq_len_t(cs.size());
							cs.push_back(node);
							currIdx = seq_len_t(cs.size());
							fifo.clear();
							break;
						}
						if (!covered[nextState]) {
							covered[nextState] = true;
							sequence_in_t newPath(current.second);
							newPath.push_back(input);
							fifo.push_back(make_pair(nextState, newPath));
						}
					}
				}

			}
		}
		getCS(CS, fsm->isOutputState());
		for (auto vs : verSeq) {
			delete vs;
		}
		verSeq.clear();
		for (auto tn : cs) {
			delete tn;
		}
		cs.clear();
		return true;
	}

}
