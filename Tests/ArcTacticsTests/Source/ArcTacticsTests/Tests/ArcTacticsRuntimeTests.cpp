#include "CQTest.h"

#include "Decision/ArcTacticsInstance.h"
#include "Fixtures/ArcTacticsSlotTestHelpers.h"
#include "Actions/ArcTacticsSequenceAction.h"
#include "Mass/ArcTacticsOption.h"

TEST_CLASS(ArcTacticsRuntimeTests, "ArcTactics.Runtime")
{
	TEST_METHOD(ResetForDecision_ClearsCandidatesAndSelection)
	{
		FArcTacticsCompiledData Compiled;
		Compiled.Version = FArcTacticsCompiledData::CurrentVersion;
		Compiled.Revision = 3;
		Compiled.bIsValid = true;
		FArcTacticsInstanceSlot Instance;
		ArcTacticsSlotTestHelpers::InitializeSlot(Instance, Compiled, FArcTacticsDecisionSettings(), FMassEntityHandle(9, 1), 12);
		Instance.Candidates.AddDefaulted();
		Instance.SelectedAction.Action = FArcTacticsNodeHandle(0);
		Instance.SelectedAction.ActionData.InitializeAs<FArcTacticsSequenceActionInstanceData>();
		ASSERT_THAT(IsTrue(Instance.HasSelectedAction()));

		Instance.ResetForDecision(FArcTQSFunctionContext());

		ASSERT_THAT(AreEqual(0, Instance.Candidates.Num(),
			TEXT("A new decision must not retain candidates from the previous evaluation")));
		ASSERT_THAT(IsFalse(Instance.HasSelectedAction(),
			TEXT("A new decision must not retain the previous selection")));
	}

	TEST_METHOD(OptionIdentity_RequiresActionAndStableValue)
	{
		FArcTacticsOption First;
		First.Action = FArcTacticsNodeHandle(1);
		First.Identity.Type = EArcTacticsValueIdentity::Entity;
		First.Identity.Entity = FMassEntityHandle(12, 3);
		FArcTacticsOption Same = First;
		FArcTacticsOption Different = First;
		Different.Identity.Entity = FMassEntityHandle(12, 4);
		FArcTacticsOption DifferentAction = First;
		DifferentAction.Action = FArcTacticsNodeHandle(2);

		ASSERT_THAT(IsTrue(First.HasSameIdentity(Same)));
		ASSERT_THAT(IsFalse(First.HasSameIdentity(DifferentAction),
			TEXT("Different action definitions must not share active identity")));
		ASSERT_THAT(IsFalse(First.HasSameIdentity(Different),
			TEXT("Entity generation changes must invalidate active identity")));
	}

	TEST_METHOD(InstanceClearActiveAction_DropsSnapshot)
	{
		FArcTacticsInstanceSlot Instance;
		Instance.ActiveAction.Action = FArcTacticsNodeHandle(4);
		Instance.ActiveAction.Identity.Type = EArcTacticsValueIdentity::Vector;
		Instance.ActiveAction.Identity.Vector = FVector(1.0f, 2.0f, 3.0f);
		Instance.ActiveAction.ActionData.InitializeAs<FArcTacticsSequenceActionInstanceData>();
		ASSERT_THAT(IsTrue(Instance.ActiveAction.IsSet()));

		Instance.ClearActiveAction();

		ASSERT_THAT(IsFalse(Instance.ActiveAction.IsSet(),
			TEXT("Cancellation and free must release the active action snapshot")));
	}
};
