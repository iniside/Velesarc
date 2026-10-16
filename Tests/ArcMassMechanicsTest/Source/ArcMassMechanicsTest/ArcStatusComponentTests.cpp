// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcMechTestSupport.h"
#include "ArcStatusTestHarness.h"
#include "ArcMassMechanics/ArcMassMechanicsTags.h"
#include "Components/ActorTestSpawner.h"
#include "Effects/ArcEffectComponent.h"
#include "Effects/ArcEffectDefinition.h"
#include "Effects/ArcEffectFunctions.h"

namespace ArcStatusComponentTestsPrivate
{
	UArcEffectDefinition* LoadStatus(const TCHAR* Name)
	{
		return ArcMechTest::LoadTestAsset<UArcEffectDefinition>(*FString::Printf(TEXT("Statuses/%s"), Name));
	}

	bool HasRemoveEffects(const UArcEffectDefinition* Definition)
	{
		return Definition && Definition->Components.ContainsByPredicate([](const FInstancedStruct& Component)
		{
			return Component.GetPtr<FArcEffectComp_RemoveEffects>() != nullptr;
		});
	}
}

// Cure-style removal of real statuses through the AMA Remove Effects component (query on the effect's identity tags).
TEST_CLASS(ArcStatus_RemoveEffects, "ArcMassMechanics.Statuses.RemoveEffects")
{
	FActorTestSpawner Spawner;
	ArcStatusTest::FStatusHarness Harness;
	FMassEntityHandle Target;
	UArcEffectDefinition* BurningFirst = nullptr;
	UArcEffectDefinition* Bleeding = nullptr;
	UArcEffectDefinition* Hex = nullptr;
	UArcEffectDefinition* BurningLast = nullptr;

	BEFORE_EACH()
	{
		ArcStatusTest::SuppressTestLogs();
		ASSERT_THAT(IsTrue(Harness.Initialize(Spawner)));
		Target = Harness.CreateEntity();
		BurningFirst = ArcStatusComponentTestsPrivate::LoadStatus(TEXT("ED_Status_Burning"));
		ASSERT_THAT(IsNotNull(BurningFirst));
		Bleeding = ArcStatusComponentTestsPrivate::LoadStatus(TEXT("ED_Status_Bleeding"));
		ASSERT_THAT(IsNotNull(Bleeding));
		Hex = ArcStatusComponentTestsPrivate::LoadStatus(TEXT("ED_Status_Hex"));
		ASSERT_THAT(IsNotNull(Hex));
		BurningLast = ArcStatusComponentTestsPrivate::LoadStatus(TEXT("ED_Status_Burning_B"));
		ASSERT_THAT(IsNotNull(BurningLast));

		ArcEffects::TryApplyEffect(*Harness.EntityManager, Target, BurningFirst, FMassEntityHandle());
		ArcEffects::TryApplyEffect(*Harness.EntityManager, Target, Bleeding, FMassEntityHandle());
		ArcEffects::TryApplyEffect(*Harness.EntityManager, Target, Hex, FMassEntityHandle());
		ArcEffects::TryApplyEffect(*Harness.EntityManager, Target, BurningLast, FMassEntityHandle());
		ASSERT_THAT(AreEqual(4, Harness.NumActive(Target)));
		ASSERT_THAT(AreEqual(2, BurningTags()));
		ASSERT_THAT(AreEqual(1, BleedingTags()));
	}

	bool ApplyRemover(const TCHAR* Name)
	{
		UArcEffectDefinition* Remover = ArcStatusComponentTestsPrivate::LoadStatus(Name);
		if (!ArcStatusComponentTestsPrivate::HasRemoveEffects(Remover))
		{
			return false;
		}
		ArcEffects::TryApplyEffect(*Harness.EntityManager, Target, Remover, FMassEntityHandle());
		return true;
	}

	bool IsActive(const UArcEffectDefinition* Definition) const
	{
		return Harness.FindActiveByDefinition(Target, Definition) != nullptr;
	}

	int32 BurningTags()
	{
		return ArcMechTest::GetOwnedTagCount(*Harness.EntityManager, Target, ArcMassMechanicsTags::Condition_Burning);
	}

	int32 BleedingTags()
	{
		return ArcMechTest::GetOwnedTagCount(*Harness.EntityManager, Target, ArcMassMechanicsTags::Condition_Bleeding);
	}

	TEST_METHOD(ConditionQuery_RemovesEveryCondition_AndTheirTags_HexSurvives)
	{
		ASSERT_THAT(IsTrue(ApplyRemover(TEXT("ED_RemoveStatuses"))));

		ASSERT_THAT(AreEqual(1, Harness.NumActive(Target)));
		ASSERT_THAT(IsTrue(IsActive(Hex)));
		ASSERT_THAT(AreEqual(0, BurningTags()));
		ASSERT_THAT(AreEqual(0, BleedingTags()));
	}

	TEST_METHOD(ConditionAndBurningQuery_RemovesOnlyBurning)
	{
		ASSERT_THAT(IsTrue(ApplyRemover(TEXT("ED_RemoveStatuses_Burning"))));

		ASSERT_THAT(AreEqual(2, Harness.NumActive(Target)));
		ASSERT_THAT(IsTrue(IsActive(Bleeding)));
		ASSERT_THAT(IsTrue(IsActive(Hex)));
		ASSERT_THAT(AreEqual(0, BurningTags()));
		ASSERT_THAT(AreEqual(1, BleedingTags()));
	}

	TEST_METHOD(MaxCountNewestFirst_RemovesNewestMatch)
	{
		ASSERT_THAT(IsTrue(ApplyRemover(TEXT("ED_RemoveStatuses_Newest1"))));

		ASSERT_THAT(AreEqual(3, Harness.NumActive(Target)));
		ASSERT_THAT(IsFalse(IsActive(BurningLast)));
		ASSERT_THAT(IsTrue(IsActive(BurningFirst)));
		ASSERT_THAT(IsTrue(IsActive(Bleeding)));
		ASSERT_THAT(AreEqual(1, BurningTags()));
	}

	TEST_METHOD(MaxCountOldestFirst_RemovesOldestMatches)
	{
		ASSERT_THAT(IsTrue(ApplyRemover(TEXT("ED_RemoveStatuses_Oldest2"))));

		ASSERT_THAT(AreEqual(2, Harness.NumActive(Target)));
		ASSERT_THAT(IsFalse(IsActive(BurningFirst)));
		ASSERT_THAT(IsFalse(IsActive(Bleeding)));
		ASSERT_THAT(IsTrue(IsActive(BurningLast)));
		ASSERT_THAT(IsTrue(IsActive(Hex)));
		ASSERT_THAT(AreEqual(1, BurningTags()));
		ASSERT_THAT(AreEqual(0, BleedingTags()));
	}

	TEST_METHOD(EmptyQuery_RemovesNothing)
	{
		ASSERT_THAT(IsTrue(ApplyRemover(TEXT("ED_RemoveStatuses_EmptyQuery"))));

		ASSERT_THAT(AreEqual(4, Harness.NumActive(Target)));
		ASSERT_THAT(AreEqual(2, BurningTags()));
	}

	TEST_METHOD(StoredRemoverMatchingItsOwnQuery_IsNeverRemoved)
	{
		UArcEffectDefinition* Remover = ArcStatusComponentTestsPrivate::LoadStatus(TEXT("ED_RemoveStatuses_Stored"));
		ASSERT_THAT(IsNotNull(Remover));
		ASSERT_THAT(IsTrue(ArcStatusComponentTestsPrivate::HasRemoveEffects(Remover)));

		const FArcEffectApplyResult Result = ArcEffects::TryApplyEffect(*Harness.EntityManager, Target, Remover, FMassEntityHandle());

		ASSERT_THAT(IsTrue(Result.Handle.IsValid()));
		ASSERT_THAT(IsNotNull(Harness.FindActive(Target, Result.Handle)));
		ASSERT_THAT(AreEqual(2, Harness.NumActive(Target)));
		ASSERT_THAT(IsTrue(IsActive(Hex)));
	}
};
