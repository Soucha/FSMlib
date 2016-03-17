#include "stdafx.h"
#include "CppUnitTest.h"
#include "../FSMlib/FSMlib.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace FSMlibTest
{		
	TEST_CLASS(UnitTest1)
	{
	public:
		
		TEST_METHOD(TestMethod1)
		{
			
			//Moore moore;
			DFSM * fsm = new Moore();
			fsm->generate(5, 2, 3);
			string file = fsm->save("");
			Logger::WriteMessage(file.c_str());
			DFSM * fsm2 = new Moore();
			Logger::WriteMessage(file.c_str());
			try {
				fsm2->load(file);
				Logger::WriteMessage(file.c_str());
				int i = fsm->getOutput(0, 1);
				int i2 = fsm2->getOutput(0, 1);
				Assert::AreEqual(fsm->getOutput(0, 1), fsm2->getOutput(0, 1));
			}
			catch (const char* s) {
				Logger::WriteMessage(s);
			}
			catch (string s) {
				Logger::WriteMessage(s.c_str());
			}
			catch (...) {
				Logger::WriteMessage("shit");
			}
			Logger::WriteMessage(file.c_str());

		}
		/*
		TEST_METHOD(Utils_hashCode) {
			int len = 5;
			string s = Utils::hashCode(len);
			string t = Utils::hashCode(len);
			Assert::AreEqual(int(s.size()), len, L"Hash code has got different lenghts.");
			Assert::AreEqual(int(t.size()), len, L"Hash code has got different lenghts.");
			Assert::AreNotEqual(s, t, L"Hash codes are equal.");
			len = 3;
			s = Utils::hashCode(len);
			Assert::AreEqual(int(s.size()), len, L"Hash code has got different lenghts.");

		}*/

		TEST_METHOD(getSeqStr) {
			sequence_in_t seq = { EPSILON_INPUT, 1, 2, STOUT_INPUT, 3 };
			string s = FSMmodel::getInSequenceAsString(seq);
			string e = EPSILON_SYMBOL;
			e += ",1,2,";
			e += STOUT_SYMBOL;
			e += ",3";
			Assert::AreEqual(e, s, L"Strings are not equal");
		}
	};
}