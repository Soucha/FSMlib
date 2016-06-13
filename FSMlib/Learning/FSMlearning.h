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

#include "BlackBoxDFSM.h"
#include "BlackBox.h"
#include "TeacherBB.h"
#include "TeacherDFSM.h"
#include "TeacherRL.h"
#include "Teacher.h"

namespace FSMlearning {
	struct ObservationTable {
		vector<sequence_in_t> S;
		vector<sequence_in_t> E;
		map<sequence_in_t, vector<sequence_out_t>> T;
		unique_ptr<DFSM> conjecture;
	};

	struct dt_node_t {// Discrimination Tree node
		sequence_in_t sequence;// access or distinguishing if state == NULL_STATE
		state_t state;
		map<sequence_out_t, shared_ptr<dt_node_t>> succ;
		weak_ptr<dt_node_t> parent;
		sequence_out_t incomingOutput;
		size_t level;
	};

	enum OP_CEprocessing {
		AllGlobally,
		OneGlobally,
		OneLocally
	};

	/**
	* Original L* counterexample processing that adds all prefixes of CE to set S.
	* A consistency check of OT is then required.
	*
	* Source:
	* Article (angluin1987learning) 
	* Angluin, D. 
	* Learning regular sets from queries and counterexamples 
	* Information and computation, Elsevier, 1987, 75, 87-106 
	*
	* @param counterexample
	* @param Observation Table (S,E,T)
	* @param Teacher
	*/
	FSMLIB_API void addAllPrefixesToS(const sequence_in_t& ce, ObservationTable& ot, const unique_ptr<Teacher>& teacher);

	/**
	* Finds a suffix that reveals a new state using binary search over CE.
	* Aimed to Regular Languages learning where an Output (Membership) Query replies
	* only with the last output symbol. Therefore, it reduces the number of queries.
	* It does not reveal all possible information of CE so CE could be used again.
	*
	* Source:
	* Article (rivest1993inference) 
	* Rivest, R. L. & Schapire, R. E. 
	* Inference of finite automata using homing sequences 
	* Information and Computation, Elsevier, 1993, 103, 299-347 
	*
	* @param counterexample
	* @param Observation Table (S,E,T)
	* @param Teacher
	*/
	FSMLIB_API void addSuffixToE_binarySearch(const sequence_in_t& ce, ObservationTable& ot, const unique_ptr<Teacher>& teacher);

	/**
	* Finds the longest prefix of CE that is not in S and the rest of CE reveal a new state,
	* then adds the suffix of CE to set E as one distinguishing sequence.
	* It does not reveal all possible information of CE so CE could be used again.
	*
	* @param counterexample
	* @param Observation Table (S,E,T)
	* @param Teacher
	*/
	FSMLIB_API void addSuffixAfterLastStateToE(const sequence_in_t& ce, ObservationTable& ot, const unique_ptr<Teacher>& teacher);

	/**
	* Finds the longest prefix of CE that is in S.X (a row of OT),
	* then adds the rest (suffix of CE) and all its suffixes to set E.
	*
	* Source:
	* PhdThesis (shahbaz2008reverse) 
	* Shahbaz, M. 
	* Reverse Engineering Enhanced State Models of Black Box Components to support Integration Testing 
	* PhD thesis, Grenoble Institute of Technology, PhD thesis, Grenoble Institute of Technology, 2008 
	*
	* @param counterexample
	* @param Observation Table (S,E,T)
	* @param Teacher
	*/
	FSMLIB_API void addAllSuffixesAfterLastStateToE(const sequence_in_t& ce, ObservationTable& ot, const unique_ptr<Teacher>& teacher);

	/**
	* Adds suffixes of CE to E unless the OT is not closed.
	* Starts with the shortest one that is not in E and 
	* prepends the related input until the suffix reveals a new state.
	*
	* Source:
	* InProceedings (irfan2010angluin) 
	* Irfan, M. N.; Oriat, C. & Groz, R. 
	* Angluin style finite state machine inference with non-optimal counterexamples 
	* Proceedings of the First International Workshop on Model Inference In Testing, 2010, 11-19 
	*
	* @param counterexample
	* @param Observation Table (S,E,T)
	* @param Teacher
	*/
	FSMLIB_API void addSuffix1by1ToE(const sequence_in_t& ce, ObservationTable& ot, const unique_ptr<Teacher>& teacher);

	/**
	* The L* algorithm using an Observation Table
	*
	* Source:
	* Article (angluin1987learning) 
	* Angluin, D. 
	* Learning regular sets from queries and counterexamples 
	* Information and computation, Elsevier, 1987, 75, 87-106
	*
	* Semantic Suffix Closedness is proposed in:
	* InCollection (steffen2011introduction) 
	* Steffen, B.; Howar, F. & Merten, M. 
	* Introduction to active automata learning from a practical perspective 
	* Formal Methods for Eternal Networked Software Systems, Springer, 2011, 256-296
	*
	* @param Teacher
	* @param processCounterexample - a function proccessing an obtained counterexample, see examples above.
	*							The function should extend set S and/or E and make the OT unclosed.
	*							If the function extends S, then all prefixes of a new string needs to be included in S before the string.
	* @param provideTentativeModel - a function that is called if any change occurs to conjectured model.
	*							If the function returns false, then the learning stops immediately.
	* @param checkConsistency - the OT is checked for inconsistency (S contains indistinguished states) if sets to true 
	*							(needed for addAllPrefixesToS as processCounterexample function)
	* @param checkSemanticSuffixClosedness - the suffix set E is extended by suffixes that make the conjecture minimal if sets to true
	* @return A learned model
	*/
	FSMLIB_API unique_ptr<DFSM> Lstar(const unique_ptr<Teacher>& teacher,
		function<void(const sequence_in_t& ce, ObservationTable& ot, const unique_ptr<Teacher>& teacher)> processCounterexample,
		function<bool(const unique_ptr<DFSM>& conjecture)> provideTentativeModel = nullptr,
		bool checkConsistency = false, bool checkSemanticSuffixClosedness = false);

	/**
	* The learning algorithm based on a Discrimination Tree
	*
	* Source:
	* Book (kearns1994introduction) 
	* Kearns, M. & Vazirani, U. V. 
	* An introduction to computational learning theory 
	* The MIT Press, 1994
	*
	* @param Teacher
	* @param provideTentativeModel - a function that is called if any change occurs to conjectured model.
	*							If the function returns false, then the learning stops immediately.
	* @return A learned model
	*/
	FSMLIB_API unique_ptr<DFSM> DiscriminationTreeAlgorithm(const unique_ptr<Teacher>& teacher,
		function<bool(const unique_ptr<DFSM>& conjecture)> provideTentativeModel = nullptr);

	/**
	* The Observation Pack algorithm combining a Discrimination Tree with Observation Tables
	*
	* Source:
	* Article (howar2012active)
	* Howar, F. M. 
	* Active learning of interface programs 
	* 2012 
	*
	* @param Teacher
	* @param provideTentativeModel - a function that is called if any change occurs to conjectured model.
	*							If the function returns false, then the learning stops immediately.
	* @return A learned model
	*/
	FSMLIB_API unique_ptr<DFSM> ObservationPackAlgorithm(const unique_ptr<Teacher>& teacher,
		OP_CEprocessing processCounterexample = OneLocally,
		function<bool(const unique_ptr<DFSM>& conjecture)> provideTentativeModel = nullptr);

	/**
	* The TTT algorithm using a spanning Tree, a Discrimination Tree and a Discriminator Trie
	*
	* Source:
	* InProceedings (isberner2014ttt) 
	* Isberner, M.; Howar, F. & Steffen, B. 
	* The TTT algorithm: a redundancy-free approach to active automata learning 
	* Runtime Verification, 2014, 307-322
	*
	* @param Teacher
	* @param provideTentativeModel - a function that is called if any change occurs to conjectured model.
	*							If the function returns false, then the learning stops immediately.
	* @return A learned model
	*/
	FSMLIB_API unique_ptr<DFSM> TTT(const unique_ptr<Teacher>& teacher,
		//function<void(const sequence_in_t& ce, ObservationPack& op, const unique_ptr<Teacher>& teacher)> processCounterexample,
		function<bool(const unique_ptr<DFSM>& conjecture)> provideTentativeModel = nullptr);

	/**
	* The learning algorithm based on testing theory (W-method).
	*
	* Source:
	* InProceedings (petrenko2014inferring) 
	* Petrenko, A.; Li, K.; Groz, R.; Hossen, K. & Oriat, C. 
	* Inferring approximated models for systems engineering 
	* High-Assurance Systems Engineering (HASE), 2014 IEEE 15th International Symposium on, 2014, 249-253 
	*
	* The algorithm generalizes the Direct Hypothesis Construction (DHC) algorithm:
	* InCollection (steffen2011introduction) 
	* Steffen, B.; Howar, F. & Merten, M. 
	* Introduction to active automata learning from a practical perspective 
	* Formal Methods for Eternal Networked Software Systems, Springer, 2011, 256-296
	*
	* @param Teacher
	* @param provideTentativeModel - a function that is called if any change occurs to conjectured model.
	*							If the function returns false, then the learning stops immediately.
	* @return A learned model
	*/
	FSMLIB_API unique_ptr<DFSM> QuotientAlgorithm(const unique_ptr<Teacher>& teacher,
		function<bool(const unique_ptr<DFSM>& conjecture)> provideTentativeModel = nullptr);

}
