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
#pragma once

#include "../Sequences/FSMsequence.h"

namespace FSMtesting {// all testing methods require a compact FSM
	//<-- Resettable machines -->// 

	struct ConvergentNode;

	/**
	* Observation Tree node to use both in testing and learning
	*/
	struct OTreeNode {
		sequence_in_t accessSequence;
		output_t incomingOutput;
		output_t stateOutput;
		state_t state;
		weak_ptr<OTreeNode> parent;
		vector<shared_ptr<OTreeNode>> next;
		weak_ptr<ConvergentNode> convergentNode;
		input_t lastQueriedInput;
		set<state_t> domain;

		// learning attributes
		enum condition { NOT_QUERIED, QUERIED_NOT_RN, QUERIED_RN } observationStatus = NOT_QUERIED;
		seq_len_t maxSuffixLen = 0;

		OTreeNode(output_t stateOutput, state_t state, input_t numberOfInputs) :
			incomingOutput(DEFAULT_OUTPUT), stateOutput(stateOutput), state(state),
			next(numberOfInputs), lastQueriedInput(STOUT_INPUT)
		{
		}

		OTreeNode(const shared_ptr<OTreeNode>& parent, input_t input,
			output_t transitionOutput, output_t stateOutput, state_t state, input_t numberOfInputs) :
			accessSequence(parent->accessSequence), incomingOutput(transitionOutput), stateOutput(stateOutput),
			state(state), parent(parent), next(numberOfInputs), lastQueriedInput(STOUT_INPUT)
		{
			accessSequence.push_back(input);
		}
	};

	inline bool CNcompare(const ConvergentNode* ls, const ConvergentNode* rs);

	struct CNcomp {
		bool operator() (const ConvergentNode* ls, const ConvergentNode* rs) const {
			return CNcompare(ls, rs);
		}
	};

	typedef set<ConvergentNode*, CNcomp> cn_set_t;

	/**
	* Convergent node groups OTree nodes that represent the same state and are proven to be convergent
	*/
	struct ConvergentNode {
		list<shared_ptr<OTreeNode>> convergent;
		list<shared_ptr<OTreeNode>> leafNodes;
		vector<shared_ptr<ConvergentNode>> next;
		cn_set_t domain;
		state_t state;
		bool isRN;

		ConvergentNode(const shared_ptr<OTreeNode>& node, bool isRN = false) :
			state(node->state), next(node->next.size()), isRN(isRN) {
			convergent.emplace_back(node);
		}
	};

	inline bool CNcompare(const ConvergentNode* ls, const ConvergentNode* rs) {
		const auto& las = ls->convergent.front()->accessSequence;
		const auto& ras = rs->convergent.front()->accessSequence;
		if (las.size() != ras.size()) return las.size() > ras.size();
		return las < ras;
	}

	/**
	* Observation Tree
	*/
	struct OTree {
		/// reference nodes, or state nodes; the first one represents the initial state
		vector<shared_ptr<ConvergentNode>> rn;
		/// number of extra states assumed
		state_t es;

		virtual ~OTree() {
			for (auto& sn : rn) {
				sn->next.clear();
			}
		}
	};

	/**
	* Groups characterization of states
	*/
	struct StateCharacterization {
		/// Splitting Tree
		unique_ptr<SplittingTree> st;
	};

	/**
	* Designs a test suite in which all transitions are confirmed
	* using appended Preset Distinguishing Sequence.
	* 
	* Source:
	* MastersThesis (soucha2015checking) 
	* Soucha, M. 
	* Checking Experiment Design Methods 
	* Czech Technical Univerzity in Prague, 2015
	*
	* @param fsm - Deterministic FSM
	* @param extraStates - how many extra states shall be considered,
	*		 default is no extra state, needs to be positive or 0
	* @return a Test Suite, or an empty collection if there is no PDS, extraStates is negative, or the FSM is not compact
	*/
	FSMLIB_API sequence_set_t PDS_method(const unique_ptr<DFSM>& fsm, int extraStates = 0);

	/**
	* Designs a test suite in which all transitions are confirmed
	* using appended Adaptive Distinguishing Sequence.
	*
	* Source:
	* MastersThesis (soucha2015checking) 
	* Soucha, M. 
	* Checking Experiment Design Methods 
	* Czech Technical Univerzity in Prague, 2015
	*
	* @param fsm - Deterministic FSM
	* @param extraStates - how many extra states shall be considered,
	*		 default is no extra state, needs to be positive or 0
	* @return a Test Suite, or an empty collection if there is no ADS, extraStates is negative, or the FSM is not compact
	*/
	FSMLIB_API sequence_set_t ADS_method(const unique_ptr<DFSM>& fsm, int extraStates = 0);
	
	/**
	* Designs a test suite in which all states are confirmed
	* using Verifying Set of State Verifying Sequences of each state
	* and all last transitions are confirmed
	* using appended State Verifying Sequence of the end state, or
	* using its State Characterizing Set if the state has no SVS.
	* SCSet is also in place of SVS in VSet if such SVS does not exist.
	*
	* Source:
	* MastersThesis (soucha2015checking) 
	* Soucha, M. 
	* Checking Experiment Design Methods 
	* Czech Technical Univerzity in Prague, 2015
	*
	* @param fsm - Deterministic FSM
	* @param extraStates - how many extra states shall be considered,
	*		 default is no extra state, needs to be positive or 0
	* @return a Test Suite, or an empty collection if extraStates is negative or the FSM is not compact
	*/
	FSMLIB_API sequence_set_t SVS_method(const unique_ptr<DFSM>& fsm, int extraStates = 0);
	
	/**
	* Designs a test suite in which all states and transitions are confirmed
	* using appended Characterizing Set.
	*
	* Sources:
	* Article (vasilevskii1973failure) 
	* Vasilevskii, M. 
	* Failure diagnosis of automata 
	* Cybernetics and Systems Analysis, Springer, 1973, 9, 653-665 
	*
	* Article (chow1978testing)
	* Chow, T. S. 
	* Testing software design modeled by finite-state machines 
	* Software Engineering, IEEE Transactions on, IEEE, 1978, 178-187
	*
	* @param fsm - Deterministic FSM
	* @param extraStates - how many extra states shall be considered,
	*		 default is no extra state, needs to be positive or 0
	* @param CSet - Characterizing Set of the given DFSM,
	*		 if the parametr is missing, then it is designed by 
	*		 FSMsequence::getCharacterizingSet(fsm, getStatePairsShortestSeparatingSequences, true, reduceCSet_LS_SL)
	* @return a Test Suite, or an empty collection if extraStates is negative or the FSM is not compact
	*/
	FSMLIB_API sequence_set_t W_method(const unique_ptr<DFSM>& fsm, int extraStates = 0,
		sequence_set_t& CSet = sequence_set_t());
	
	/**
	* Designs a test suite in which all transitions are confirmed
	* using appended State Characterizing Set of the end state
	* and all states are confirmed using Characterizing Set
	* that consists of all used SCSets.
	*
	* Source:
	* Article (fujiwara1991test)
	* Fujiwara, S.; Khendek, F.; Amalou, M.; Ghedamsi, A. & others
	* Test selection based on finite state models 
	* Software Engineering, IEEE Transactions on, IEEE, 1991, 17, 591-603
	*
	* @param fsm - Deterministic FSM
	* @param extraStates - how many extra states shall be considered,
	*		 default is no extra state, needs to be positive or 0
	* @param SCSets - State Characterizing Set of all states of the given DFSM,
	*		 if the parametr is missing, then it is designed by 
	*		 FSMsequence::getStatesCharacterizingSets(fsm, getStatePairsShortestSeparatingSequences, true, reduceSCSet_LS_SL)	
	* @return a Test Suite, or an empty collection if extraStates is negative or the FSM is not compact
	*/
	FSMLIB_API sequence_set_t Wp_method(const unique_ptr<DFSM>& fsm, int extraStates = 0,
		vector<sequence_set_t>& SCSets = vector<sequence_set_t>());
	
	/**
	* Designs a test suite in which all states and transitions are confirmed
	* using appended Harmonized State Identifier of the related state.
	*
	* Source:
	* InProceedings (petrenko1991checking) 
	* Petrenko, A. 
	* Checking experiments with protocol machines 
	* Proceedings of the IFIP TC6/WG6. 1 Fourth International Workshop on Protocol Test Systems IV, 1991, 83-94
	*
	* @param fsm - Deterministic FSM
	* @param extraStates - how many extra states shall be considered,
	*		 default is no extra state, needs to be positive or 0
	* @param HSI - Harmonized State Identifiers of the given DFSM,
	*		 it is designed by FSMsequence::getHarmonizedStateIdentifiers(fsm) if the parametr is missing
	* @return a Test Suite, or an empty collection if extraStates is negative or the FSM is not compact
	*/
	FSMLIB_API sequence_set_t HSI_method(const unique_ptr<DFSM>& fsm, int extraStates = 0,
		vector<sequence_set_t>& HSI = vector<sequence_set_t>());

	/**
	* Designs a test suite in which all state pairs are distinguished and thus confirmed
	* using a separating sequence that is chosen adaptively to minimize the length of TS.
	* Transitions are confirmed in the same manner, i.e. end states are distinguished from all states.
	*
	* Source:
	* InCollection (dorofeeva2005improved)
	* Dorofeeva, R.; El-Fakih, K. & Yevtushenko, N. 
	* An improved conformance testing method 
	* Formal Techniques for Networked and Distributed Systems-FORTE 2005, Springer, 2005, 204-218
	*
	* @param fsm - Deterministic FSM
	* @param extraStates - how many extra states shall be considered,
	*		 default is no extra state, needs to be positive or 0
	* @return a Test Suite, or an empty collection if extraStates is negative or the FSM is not compact
	*/
	FSMLIB_API sequence_set_t H_method(const unique_ptr<DFSM>& fsm, int extraStates = 0);

	/**
	* Designs a test suite in which all states and transitions are confirmed
	* using Harmonized State Identifiers that are appended to already confirmed states
	*
	* Source:
	* Article (simao2012reducing)
	* Simao, A.; Petrenko, A. & Yevtushenko, N.
	* On reducing test length for fsms with extra states 
	* Software Testing, Verification and Reliability, Wiley Online Library, 2012, 22, 435-454
	*
	* @param fsm - Deterministic FSM
	* @param extraStates - how many extra states shall be considered,
	*		 default is no extra state, needs to be positive or 0
	* @param HSI - Harmonized State Identifiers of the given DFSM,
	*		 it is designed by FSMsequence::getHarmonizedStateIdentifiers(fsm) if the parametr is missing
	* @return a Test Suite, or an empty collection if extraStates is negative or the FSM is not compact
	*/
	FSMLIB_API sequence_set_t SPY_method(const unique_ptr<DFSM>& fsm, int extraStates = 0,
		vector<sequence_set_t>& HSI = vector<sequence_set_t>());

	/// TODO
	/**
	*
	* Source:
	* InProceedings (petrenko1992test) 
	* Petrenko, A. & Yevtushenko, N. 
	* Test suite generation from a fsm with a given type of implementation errors 
	* Proceedings of the IFIP TC6/WG6. 1 Twelth International Symposium on Protocol Specification, Testing and Verification XII, 1992, 229-243
	*
	* @param fsm - Deterministic FSM
	* @param extraStates - how many extra states shall be considered,
	*		 default is no extra state, needs to be positive or 0
	* @return a Test Suite, or an empty collection if extraStates is negative or the FSM is not compact
	*/
	//FSMLIB_API sequence_set_t FF_method(const unique_ptr<DFSM>& fsm, int extraStates = 0);
	
	/**
	*
	* Source:
	* Article (petrenko2005testing) 
	* Petrenko, A. & Yevtushenko, N. 
	* Testing from partial deterministic FSM specifications 
	* Computers, IEEE Transactions on, IEEE, 2005, 54, 1154-1165
	*
	* @param fsm - Deterministic FSM
	* @param extraStates - how many extra states shall be considered,
	*		 default is no extra state, needs to be positive or 0
	* @return a Test Suite, or an empty collection if extraStates is negative or the FSM is not compact
	*/
	//FSMLIB_API sequence_set_t SC_method(const unique_ptr<DFSM>& fsm, int extraStates = 0);
	
	/**
	*
	* Source:
	* Article (simao2009fault) 
	* Simão, A. & Petrenko, A. 
	* Fault coverage-driven incremental test generation 
	* The Computer Journal, Br Computer Soc, 2009, bxp073 
	*
	* @param fsm - Deterministic FSM
	* @param extraStates - how many states shall be considered relatively to the number of states,
	* @return a Test Suite, or an empty collection if the FSM is not compact
	*/
	//FSMLIB_API sequence_set_t P_method(const unique_ptr<DFSM>& fsm, int extraStates = 0);

	/**
	* Designs a test suite in which all states and transitions are confirmed
	* using separating sequences chosen on the fly that
	* are distributed over states reached by already proven convergent sequences.
	* States are distinguished pairwise as in the H-method.
	*
	* Based on source:
	* Article (simao2012reducing)
	* Simao, A.; Petrenko, A. & Yevtushenko, N.
	* On reducing test length for fsms with extra states
	* Software Testing, Verification and Reliability, Wiley Online Library, 2012, 22, 435-454
	*
	* InCollection (dorofeeva2005improved)
	* Dorofeeva, R.; El-Fakih, K. & Yevtushenko, N. 
	* An improved conformance testing method 
	* Formal Techniques for Networked and Distributed Systems-FORTE 2005, Springer, 2005, 204-218
	*
	* @param fsm - Deterministic FSM
	* @param extraStates - how many extra states shall be considered,
	*		 default is no extra state, needs to be positive or 0
	* @return a Test Suite, or an empty collection if extraStates is negative or the FSM is not compact
	*/
	FSMLIB_API sequence_set_t SPYH_method(const unique_ptr<DFSM>& fsm, int extraStates = 0);
	
	/**
	* Designs a test suite in which all states and transitions are confirmed
	* using separating sequences (chosen on the fly from the splitting tree) that
	* are distributed over states reached by already proven convergent sequences.
	*
	* Based on source:
	* Article (simao2012reducing)
	* Simao, A.; Petrenko, A. & Yevtushenko, N.
	* On reducing test length for fsms with extra states
	* Software Testing, Verification and Reliability, Wiley Online Library, 2012, 22, 435-454
	*
	* @param fsm - Deterministic FSM
	* @param extraStates - how many extra states shall be considered,
	*		 default is no extra state, needs to be positive or 0
	* @return a Test Suite, or an empty collection if extraStates is negative or the FSM is not compact
	*/
	FSMLIB_API sequence_set_t S_method(const unique_ptr<DFSM>& fsm, int extraStates = 0);
	
	/**
	* Extends the given test suite such that it becomes complete for the given number of extra states.
	* All states and transitions are confirmed using separating sequences (chosen on the fly from the splitting tree)
	* that are distributed over states reached by already proven convergent sequences.
	* Splitting tree of the given machine can be provided.
	* 
	*
	* Based on source:
	* Article (simao2012reducing)
	* Simao, A.; Petrenko, A. & Yevtushenko, N.
	* On reducing test length for fsms with extra states
	* Software Testing, Verification and Reliability, Wiley Online Library, 2012, 22, 435-454
	*
	* @param fsm - Deterministic FSM
	* @param ot - a partial test suite that should be extended and the number of extra states that shall be considered
	* @param sc - a splitting tree of the given machine
	* @return a set of sequences that extend the given test suite,
	*		or an empty collection if extraStates is negative or the FSM is not compact
	*/
	FSMLIB_API sequence_set_t S_method_ext(const unique_ptr<DFSM>& fsm, OTree& ot, StateCharacterization& sc);

	/**
	* Designs a checking sequence by appending an adaptive distinguishing sequence
	* such that each transition and state is verified by ADS in resulting CS.
	* In the process of design, if an unverified transition is too far from the current state,
	* reset is applied and thus a test suite of several sequences can be produced.
	*
	* Source:
	* MastersThesis (soucha2015checking)
	* Soucha, M.
	* Checking Experiment Design Methods
	* Czech Technical Univerzity in Prague, 2015
	*
	* @param fsm - Deterministic FSM
	* @param extraStates - how many extra states shall be considered,
	*		 default is no extra state, needs to be positive or 0
	* @return a Test Suite, or an empty collection if there is no ADS, extraStates is negative, or the FSM is not compact
	*/
	FSMLIB_API sequence_set_t Mra_method(const unique_ptr<DFSM>& fsm, int extraStates = 0);
	
	/**
	* Designs a checking sequence using an adaptive distinguishing sequence
	* such that each transition and state is verified by ADS in resulting CS.
	* A Test Segment, i.e. a transition followed by ADS, is created for each transition.
	* Possible overlapping is calculated for each pair of test segments and
	* according to these costs test segments are connected to create a sequence.
	* Therefore, the length of CS is optimized globally.
	* In the process of connecting test segment, reset can be applied
	* if the reacheable sequence is shorter than the shortest sequence
	* from end state of the first test segment to start state of the second segment.
	*
	* Source:
	* MastersThesis (soucha2015checking)
	* Soucha, M.
	* Checking Experiment Design Methods
	* Czech Technical Univerzity in Prague, 2015
	*
	* @param fsm - Deterministic FSM
	* @param extraStates - how many extra states shall be considered,
	*		 default is no extra state, needs to be positive or 0
	* @return a Test Suite, or an empty collection if there is no ADS, extraStates is negative, or the FSM is not compact
	*/
	FSMLIB_API sequence_set_t Mrg_method(const unique_ptr<DFSM>& fsm, int extraStates = 0);

	/**
	* Designs a checking sequence using an adaptive distinguishing sequence
	* such that each transition and state is verified by ADS in resulting CS.
	* A Test Segment, i.e. a transition followed by ADS, is created for each transition.
	* Order of test segments in resulting CS is obtained by solving Travelling Salesman Problem (TSP)
	* using GUROBI solver. A test segment corresponds to a node in TSP and
	* a cost of edge is possible overlapping of two related test segments.
	* If two consecutive segments do not overlap, reset can be used if a shorter CS is produced.
	*
	* Source:
	* MastersThesis (soucha2015checking)
	* Soucha, M.
	* Checking Experiment Design Methods
	* Czech Technical Univerzity in Prague, 2015
	*
	* @param fsm - Deterministic FSM
	* @param extraStates - how many extra states shall be considered,
	*		 default is no extra state, needs to be positive or 0
	* @return a Test Suite, or an empty collection if there is no ADS, extraStates is negative,
	*		 an error occured due to use of GUROBI, or the FSM is not compact
	*/
	FSMLIB_API sequence_set_t Mrstar_method(const unique_ptr<DFSM>& fsm, int extraStates = 0);
	
	//<-- Checking sequence methods -->//

	/// TODO
	/**
	*
	* Source:
	* Article (hsieh1971checking) 
	* Hsieh, E. 
	* Checking Experiments for Sequential Machines 
	* Computers, IEEE Transactions on, IEEE, 1971, 100, 1152-1166
	*
	* @param fsm - Deterministic FSM
	* @param extraStates - NOT SUPPORTED YET(how many extra states shall be considered,
	*		default is no extra state, needs to be positive or 0)
	* @return a Checking Sequence, or an empty sequence if there is no ADS or the FSM is not compact
	*/
	//FSMLIB_API sequence_in_t HrADS_method(const unique_ptr<DFSM>& fsm, int extraStates = 0);
	
	/**
	*
	* Essential source:
	* InProceedings (hennie1964fault) 
	* Hennie, F. 
	* Fault detecting experiments for sequential circuits 
	* Switching Circuit Theory and Logical Design, 1964 Proceedings of the Fifth Annual Symposium on, 1964, 95-110
	*
	* Further sources: gonenc1970method, ural1997minimizing, hierons2006optimizing,... (see soucha2015checking)
	*
	* @param fsm - Deterministic FSM
	* @param extraStates - NOT SUPPORTED YET(how many extra states shall be considered,
	*		default is no extra state, needs to be positive or 0)
	* @return a Checking Sequence, or an empty sequence if there is no PDS or the FSM is not compact
	*/
	//FSMLIB_API sequence_in_t D_method(const unique_ptr<DFSM>& fsm, int extraStates = 0);
	
	/**
	*
	* Similar to the D-method but it uses ADS instead of PDS.
	*
	* Sources:
	* Article (boute1974distinguishing) 
	* Boute, R. 
	* Distinguishing sets for optimal state identification in checking experiments 
	* Computers, IEEE Transactions on, IEEE, 1974, 100, 874-877 
	*
	* InProceedings (hierons2009checking) 
	* Hierons, R. M.; Jourdan, G.-V.; Ural, H. & Yenigun, H. 
	* Checking sequence construction using adaptive and preset distinguishing sequences 
	* Software Engineering and Formal Methods, 2009 Seventh IEEE International Conference on, 2009, 157-166
	*
	* @param fsm - Deterministic FSM
	* @param extraStates - NOT SUPPORTED YET(how many extra states shall be considered,
	*		default is no extra state, needs to be positive or 0)
	* @return a Checking Sequence, or an empty sequence if there is no ADS or the FSM is not compact
	*/
	//FSMLIB_API sequence_in_t AD_method(const unique_ptr<DFSM>& fsm, int extraStates = 0);
	
	/**
	*
	* Source:
	* InProceedings (hennie1964fault) 
	* Hennie, F. 
	* Fault detecting experiments for sequential circuits 
	* Switching Circuit Theory and Logical Design, 1964 Proceedings of the Fifth Annual Symposium on, 1964, 95-110
	*
	* @param fsm - Deterministic FSM
	* @param extraStates - NOT SUPPORTED YET(how many extra states shall be considered,
	*		default is no extra state, needs to be positive or 0)
	* @return a Checking Sequence, or an empty sequence if the FSM is not compact
	*/
	//FSMLIB_API sequence_in_t DW_method(const unique_ptr<DFSM>& fsm, int extraStates = 0);
	
	/**
	*
	* Source:
	* InCollection (porto2013generation) 
	* Porto, F. R.; Endo, A. T. & Simao, A. 
	* Generation of Checking Sequences Using Identification Sets 
	* Formal Methods and Software Engineering, Springer, 2013, 115-130
	*
	* @param fsm - Deterministic FSM
	* @param extraStates - NOT SUPPORTED YET(how many extra states shall be considered,
	*		default is no extra state, needs to be positive or 0)
	* @return a Checking Sequence, or an empty sequence if the FSM is not compact
	*/
	//FSMLIB_API sequence_in_t DWp_method(const unique_ptr<DFSM>& fsm, int extraStates = 0);
	
	/**
	*
	* Sources:
	* Article (sabnani1985new) 
	* Sabnani, K. & Dahbura, A. 
	* A new technique for generating protocol test 
	* ACM SIGCOMM Computer Communication Review, ACM, 1985, 15, 36-43
	*
	* Article (chan1989improved) 
	* Chan, W. Y.; Vuong, C. & Otp, M. 
	* An improved protocol test generation procedure based on UIOs 
	* ACM SIGCOMM Computer Communication Review, ACM, 1989, 19, 283-294
	*
	* and many others... see soucha2015checking
	*
	* @param fsm - Deterministic FSM
	* @param extraStates - NOT SUPPORTED YET(how many extra states shall be considered,
	*		default is no extra state, needs to be positive or 0)
	* @return a Checking Sequence, or an empty sequence if there is a state without SVS or the FSM is not compact
	*/
	//FSMLIB_API sequence_in_t UIO_method(const unique_ptr<DFSM>& fsm, int extraStates = 0);
	
	/**
	*
	* Source:
	* InProceedings (vuong1990novel) 
	* Vuong, S. T. & Ko, K. C. 
	* A novel approach to protocol test sequence generation 
	* Global Telecommunications Conference, 1990, and Exhibition. 'Communications: Connecting the Future', GLOBECOM'90., IEEE, 1990, 1880-1884 
	*
	* @param fsm - Deterministic FSM
	* @param extraStates - NOT SUPPORTED YET(how many extra states shall be considered,
	*		default is no extra state, needs to be positive or 0)
	* @return a Checking Sequence, or an empty sequence if there is no PDS or the FSM is not compact (? TODO ?)
	*/
	//FSMLIB_API sequence_in_t CSP_method(const unique_ptr<DFSM>& fsm, int extraStates = 0);

	/**
	* Designs a checking sequence by appending an adaptive distinguishing sequence
	* to confirmed states so another state/transition is verified.
	*
	* Sources:
	* InCollection (simao2008generating)
	* Simão, A. & Petrenko, A.
	* Generating checking sequences for partial reduced finite state machines
	* Testing of Software and Communicating Systems, Springer, 2008, 153-168
	*
	* InProceedings (simao2009checking)
	* Simão, A. & Petrenko, A.
	* Checking sequence generation using state distinguishing subsequences
	* Software Testing, Verification and Validation Workshops, 2009. ICSTW'09. International Conference on, 2009, 48-56
	*
	* @param fsm - Deterministic FSM
	* @param extraStates - NOT SUPPORTED YET (how many extra states shall be considered,
	*		 default is no extra state, needs to be positive or 0)
	* @return a Checking Sequence, or an empty sequence if there is no ADS or the FSM is not compact
	*/
	FSMLIB_API sequence_in_t C_method(const unique_ptr<DFSM>& fsm, int extraStates = 0);
	
	/**
	*
	*
	* Sources:
	* Article (kapus2010better) 
	* Kapus-Kolar, M. 
	* A better procedure and a stronger state-recognition pattern for checking sequence construction 
	* Jožef Stefan Institute Technical Report, 2010, 10574
	*
	* @param fsm - Deterministic FSM
	* @param extraStates - NOT SUPPORTED YET (how many extra states shall be considered,
	*		 default is no extra state, needs to be positive or 0)
	* @return a Checking Sequence, or an empty sequence if there is no ADS or the FSM is not compact
	*/
	//FSMLIB_API sequence_in_t K_method(const unique_ptr<DFSM>& fsm, int extraStates = 0);

	// Attempts
	//FSMLIB_API sequence_in_t MHSI_method(const unique_ptr<DFSM>& fsm, int extraStates = 0);// is it correct?

	/**
	* Designs a checking sequence by appending an adaptive distinguishing sequence
	* such that each transition and state is verified by ADS in resulting CS.
	*
	* Source:
	* MastersThesis (soucha2015checking) 
	* Soucha, M. 
	* Checking Experiment Design Methods 
	* Czech Technical Univerzity in Prague, 2015
	*
	* @param fsm - Deterministic FSM
	* @param extraStates - NOT SUPPORTED YET (how many extra states shall be considered,
	*		 default is no extra state, needs to be positive or 0)
	* @return a Checking Sequence, or an empty sequence if there is no ADS or the FSM is not compact
	*/
	FSMLIB_API sequence_in_t Ma_method(const unique_ptr<DFSM>& fsm, int extraStates = 0);
	
	/**
	* Designs a checking sequence using an adaptive distinguishing sequence
	* such that each transition and state is verified by ADS in resulting CS.
	* A Test Segment, i.e. a transition followed by ADS, is created for each transition.
	* Possible overlapping is calculated for each pair of test segments and
	* according to these costs test segments are connected to create a sequence.
	* Therefore, the length of CS is optimized globally.
	*
	* Source:
	* MastersThesis (soucha2015checking)
	* Soucha, M.
	* Checking Experiment Design Methods
	* Czech Technical Univerzity in Prague, 2015
	*
	* @param fsm - Deterministic FSM
	* @param extraStates - NOT SUPPORTED YET (how many extra states shall be considered,
	*		 default is no extra state, needs to be positive or 0)
	* @return a Checking Sequence, or an empty sequence if there is no ADS or the FSM is not compact
	*/
	FSMLIB_API sequence_in_t Mg_method(const unique_ptr<DFSM>& fsm, int extraStates = 0);
	
	/**
	* Designs a checking sequence using an adaptive distinguishing sequence
	* such that each transition and state is verified by ADS in resulting CS.
	* A Test Segment, i.e. a transition followed by ADS, is created for each transition.
	* Order of test segments in resulting CS is obtained by solving Travelling Salesman Problem (TSP)
	* using GUROBI solver. A test segment corresponds to a node in TSP and
	* a cost of edge is possible overlapping of two related test segments.
	*
	* Source:
	* MastersThesis (soucha2015checking)
	* Soucha, M.
	* Checking Experiment Design Methods
	* Czech Technical Univerzity in Prague, 2015
	*
	* @param fsm - Deterministic FSM
	* @param extraStates - NOT SUPPORTED YET (how many extra states shall be considered,
	*		 default is no extra state, needs to be positive or 0)
	* @return a Checking Sequence, or an empty sequence if there is no ADS, an error occured due to use of GUROBI or the FSM is not compact
	*/
	FSMLIB_API sequence_in_t Mstar_method(const unique_ptr<DFSM>& fsm, int extraStates = 0);
}
