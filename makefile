# Copyright (c) Michal Soucha, 2017
#
# This file is part of FSMlib
#
# FSMlib is free software: you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation, either version 3 of the License, or (at your option) any later
# version.
#
# FSMlib is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
# A PARTICULAR PURPOSE. See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along with
# FSMlib. If not, see <http://www.gnu.org/licenses/>.
#

VPATH = FSMlib:FSMlib/Model:FSMlib/Sequences:FSMlib/Testing:FSMlib/Learning:FSMdevel
BUILD_DIR = x64
FSMLIB_DIR = FSMlib

FSMLIB_TARGET = $(BUILD_DIR)/FSMlib.exe

$(shell mkdir -p $(BUILD_DIR)/$(FSMLIB_DIR) >/dev/null)

# DistinguishingSequences.o Mstar-method.o
objects = stdafx.o FSMlib.o PrefixSet.o UnionFind.o \
	FSMmodel.o DFSM.o Mealy.o Moore.o DFA.o \
	FSMcovers.o CharacterizingSequences.o SplittingTree.o \
	AdaptiveDistinguishingSequence.o PresetDistinguishingSequence.o \
	StateVerifyingSequence.o HomingSequence.o SynchronizingSequence.o \
	W-method.o Wp-method.o HSI-method.o H-method.o SPY-method.o \
	PDS-method.o ADS-method.o SVS-method.o SPYH-method.o S-method.o \
	C-method.o Ma-method.o Mg-method.o FaultCoverageChecker.o \
	BlackBoxDFSM.o TeacherBB.o TeacherDFSM.o TeacherRL.o \
	Lstar.o DiscriminationTreeAlgorithm.o ObservationPackAlgorithm.o \
	TTT.o GoodSplit.o QuotientAlgorithm.o \
	H-learner.o SPY-learner.o S-learner.o main.o
#	FSMgenerator.o HSIdesignComparison.o MachinesAnalysis.o \
	experimenterTesting.o experimenterLearning.o main.o
OBJ = $(addprefix $(BUILD_DIR)/$(FSMLIB_DIR)/,$(objects))

all: FSMlib
	echo build

CPPLAGS := -Wall -Wextra

$(OBJ): FSMtypes.h

FSMlib: $(OBJ)
	g++ -m64 -std=c++17 -lstdc++fs -o $(FSMLIB_TARGET) $(OBJ)

$(BUILD_DIR)/$(FSMLIB_DIR)/%.o : %.cpp
#%.o : %.cpp $(BUILD_DIR)/$(FSMLIB_DIR)/%.o.d
	g++ -I"FSMlib/" -m64 -std=c++17 -MMD -MP -MF "${@:.o=.d}" -o $@ -c $<

$(BUILD_DIR)/$(FSMLIB_DIR)/%.d: ;
.PRECIOUS: $(BUILD_DIR)/$(FSMLIB_DIR)/%.d

-include $(wildcard $(BUILD_DIR)/$(FSMLIB_DIR)/*.d) # include auto-generated dependencies
#include $(BUILD_DIR)/$(FSMLIB_DIR)/%.d

.PHONY : clean
clean :
	rm $(BUILD_DIR)/$(FSMLIB_DIR)/*.o $(BUILD_DIR)/$(FSMLIB_DIR)/*.d $(FSMLIB_TARGET)