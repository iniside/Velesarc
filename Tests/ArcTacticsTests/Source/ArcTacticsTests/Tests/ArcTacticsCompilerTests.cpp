#include "CQTest.h"

#include "Actions/ArcTacticsAction.h"
#include "Decision/ArcTacticsCompiledData.h"
#include "Decision/ArcTacticsCompiler.h"
#include "Decision/ArcTacticsConfig.h"
#include "Mass/ArcTacticsOption.h"

TEST_CLASS(ArcTacticsCompilerTests, "ArcTactics.CompiledData")
{
	TEST_METHOD(CompiledData_Reset_IsNotExecutable)
	{
		UArcTacticsConfig* Config = NewObject<UArcTacticsConfig>(GetTransientPackage());
		Config->ContextID = FGuid(1, 2, 3, 4);
		Config->EnsureValidNodeIDs();
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		FArcTacticsCompiledData Data;
		ASSERT_THAT(IsTrue(FArcTacticsCompiler::Compile(*Config, 1, Data, Diagnostics),
			TEXT("A minimal valid config must compile to genuinely executable data")));
		ASSERT_THAT(IsTrue(Data.IsExecutable()));

		Data.Reset();
		ASSERT_THAT(IsFalse(Data.IsExecutable(), TEXT("Reset compiled data must invalidate executable state")));
		ASSERT_THAT(AreEqual(0u, Data.Revision));
	}

	TEST_METHOD(CandidateIdentity_DifferentValuesDoNotAlias)
	{
		FArcTacticsCandidateIdentity First;
		First.Type = EArcTacticsValueIdentity::Vector;
		First.Vector = FVector(10.0f, 20.0f, 30.0f);
		FArcTacticsCandidateIdentity Second = First;
		Second.Vector.X = 11.0f;

		ASSERT_THAT(IsFalse(First == Second, TEXT("Different stable candidate values must not share identity")));
	}
};
