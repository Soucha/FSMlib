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

	/**
	* Original L* counterexample processing that adds all prefixes of CE to set S.
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
	* Finds the longest prefix of CE that is in S.X (a row of OT),
	* then adds the rest (suffix of CE) to set E as one distinguishing sequence.
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
	* The L* algorithm
	*
	* Source:
	* Article (angluin1987learning) 
	* Angluin, D. 
	* Learning regular sets from queries and counterexamples 
	* Information and computation, Elsevier, 1987, 75, 87-106 
	*
	* @param Teacher
	* @param processCounterexample - a function proccessing an obtained counterexample, see examples above.
	*							The function should extend set S and/or E and make the OT unclosed.
	*							If the function extends S, then all prefixes of a new string needs to be included in S before the string.
	* @param provideTentativeModel - a function that is called if any change occurs to conjectured model.
	*							If the function returns false, then the learning stops immediately.
	* @param checkConsistency - the OT is checked for inconsistency (S contains indistinguished states) if sets to true 
	*							(needed for addAllPrefixesToS as processCounterexample function)
	* @return A learned model
	*/
	FSMLIB_API unique_ptr<DFSM> Lstar(const unique_ptr<Teacher>& teacher,
		function<void(const sequence_in_t& ce, ObservationTable& ot, const unique_ptr<Teacher>& teacher)> processCounterexample,
		function<bool(const unique_ptr<DFSM>& conjecture)> provideTentativeModel = nullptr,
		bool checkConsistency = false);
}
