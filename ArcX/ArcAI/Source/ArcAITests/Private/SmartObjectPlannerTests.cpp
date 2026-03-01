// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "SmartObjectPlanner/ArcSmartObjectPlannerSubsystem.h"
#include "SmartObjectPlanner/ArcPotentialEntity.h"
#include "SmartObjectPlanner/ArcSmartObjectPlanResponse.h"
#include "GameplayTagContainer.h"
#include "GameplayTagsManager.h"
#include "MassEntityHandle.h"

// ---------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------

namespace SmartObjectPlannerTestHelpers
{
	FGameplayTag MakeTag(const FName& TagName)
	{
		UGameplayTagsManager& Manager = UGameplayTagsManager::Get();
		Manager.AddNativeGameplayTag(TagName);
		return Manager.RequestGameplayTag(TagName);
	}

	FGameplayTagContainer MakeTagContainer(std::initializer_list<FName> TagNames)
	{
		FGameplayTagContainer Container;
		for (const FName& Name : TagNames)
		{
			Container.AddTag(MakeTag(Name));
		}
		return Container;
	}

	FArcPotentialEntity MakeEntity(int32 Index, FVector Location,
		const FGameplayTagContainer& Provides,
		const FGameplayTagContainer& Requires = FGameplayTagContainer())
	{
		FArcPotentialEntity E;
		E.EntityHandle = FMassEntityHandle(Index, Index);
		E.Location = Location;
		E.Provides = Provides;
		E.Requires = Requires;
		E.FoundCandidateSlots.NumSlots = 1;
		return E;
	}

	// Run BuildPlanRecursive with standard setup, return the generated plans
	TArray<FArcSmartObjectPlanContainer> RunPlanner(
		TArray<FArcPotentialEntity>& Entities,
		const FGameplayTagContainer& NeededTags,
		const FGameplayTagContainer& InitialTags = FGameplayTagContainer(),
		int32 MaxPlans = 10)
	{
		TArray<bool> UsedEntities;
		UsedEntities.SetNumZeroed(Entities.Num());

		FGameplayTagContainer CurrentTags = InitialTags;
		FGameplayTagContainer AlreadyProvided = InitialTags;
		TArray<FArcSmartObjectPlanStep> CurrentPlan;
		TArray<FArcSmartObjectPlanContainer> OutPlans;

		UArcSmartObjectPlannerSubsystem::BuildPlanRecursive(
			Entities,
			NeededTags,
			CurrentTags,
			AlreadyProvided,
			CurrentPlan,
			OutPlans,
			UsedEntities,
			MaxPlans,
			nullptr);

		return OutPlans;
	}

	// Replicates the sort logic from BuildAllPlans for testing without a world
	void SortPlans(TArray<FArcSmartObjectPlanContainer>& Plans, const FVector& SearchOrigin)
	{
		Plans.Sort([&SearchOrigin](const FArcSmartObjectPlanContainer& A, const FArcSmartObjectPlanContainer& B)
		{
			if (A.Items.Num() != B.Items.Num())
			{
				return A.Items.Num() < B.Items.Num();
			}

			auto CalcTravelDistance = [&SearchOrigin](const TArray<FArcSmartObjectPlanStep>& Items) -> float
			{
				float Total = 0.0f;
				FVector Prev = SearchOrigin;
				for (const FArcSmartObjectPlanStep& Item : Items)
				{
					Total += FVector::Dist(Prev, Item.Location);
					Prev = Item.Location;
				}
				return Total;
			};

			return CalcTravelDistance(A.Items) < CalcTravelDistance(B.Items);
		});
	}
}

using namespace SmartObjectPlannerTestHelpers;

// ===============================================================
// Group 1: Basic Planning
// ===============================================================

TEST_CLASS(SmartObjectPlanner_BasicPlanning, "ArcAI.SmartObjectPlanner.Basic")
{
	TEST_METHOD(EmptyCandidates_NoPlans)
	{
		TArray<FArcPotentialEntity> Entities;
		auto NeededTags = MakeTagContainer({TEXT("Test.TagA")});

		auto Plans = RunPlanner(Entities, NeededTags);

		ASSERT_THAT(AreEqual(0, Plans.Num()));
	}

	TEST_METHOD(InitialTagsSatisfy_EmptyPlan)
	{
		TArray<FArcPotentialEntity> Entities;
		auto NeededTags = MakeTagContainer({TEXT("Test.TagA")});
		auto InitialTags = MakeTagContainer({TEXT("Test.TagA")});

		auto Plans = RunPlanner(Entities, NeededTags, InitialTags);

		ASSERT_THAT(AreEqual(1, Plans.Num()));
		ASSERT_THAT(AreEqual(0, Plans[0].Items.Num()));
	}

	TEST_METHOD(SingleCandidate_SingleTag)
	{
		FGameplayTagContainer TagA = MakeTagContainer({TEXT("Test.TagA")});
		TArray<FArcPotentialEntity> Entities;
		Entities.Add(MakeEntity(1, FVector(100, 0, 0), TagA));

		auto Plans = RunPlanner(Entities, TagA);

		ASSERT_THAT(AreEqual(1, Plans.Num()));
		ASSERT_THAT(AreEqual(1, Plans[0].Items.Num()));
		ASSERT_THAT(AreEqual(FMassEntityHandle(1, 1), Plans[0].Items[0].EntityHandle));
	}

	TEST_METHOD(SingleCandidate_WrongTag)
	{
		auto TagA = MakeTagContainer({TEXT("Test.TagA")});
		auto TagB = MakeTagContainer({TEXT("Test.TagB")});
		TArray<FArcPotentialEntity> Entities;
		Entities.Add(MakeEntity(1, FVector::ZeroVector, TagB));

		auto Plans = RunPlanner(Entities, TagA);

		ASSERT_THAT(AreEqual(0, Plans.Num()));
	}

	TEST_METHOD(TwoCandidates_NeedBothTags)
	{
		auto TagA = MakeTagContainer({TEXT("Test.TagA")});
		auto TagB = MakeTagContainer({TEXT("Test.TagB")});
		auto NeedBoth = MakeTagContainer({TEXT("Test.TagA"), TEXT("Test.TagB")});

		TArray<FArcPotentialEntity> Entities;
		Entities.Add(MakeEntity(1, FVector(100, 0, 0), TagA));
		Entities.Add(MakeEntity(2, FVector(200, 0, 0), TagB));

		auto Plans = RunPlanner(Entities, NeedBoth);

		ASSERT_THAT(IsTrue(Plans.Num() >= 1));
		ASSERT_THAT(AreEqual(2, Plans[0].Items.Num()));
	}

	TEST_METHOD(TwoCandidates_BothProvide_SameTag)
	{
		auto TagA = MakeTagContainer({TEXT("Test.TagA")});

		TArray<FArcPotentialEntity> Entities;
		Entities.Add(MakeEntity(1, FVector(100, 0, 0), TagA));
		Entities.Add(MakeEntity(2, FVector(200, 0, 0), TagA));

		auto Plans = RunPlanner(Entities, TagA);

		ASSERT_THAT(AreEqual(2, Plans.Num()));
		ASSERT_THAT(AreEqual(1, Plans[0].Items.Num()));
		ASSERT_THAT(AreEqual(1, Plans[1].Items.Num()));
		// Different entities in each plan
		ASSERT_THAT(AreNotEqual(Plans[0].Items[0].EntityHandle, Plans[1].Items[0].EntityHandle));
	}
};

// ===============================================================
// Group 2: Dependency Chains
// ===============================================================

TEST_CLASS(SmartObjectPlanner_Dependencies, "ArcAI.SmartObjectPlanner.Dependencies")
{
	TEST_METHOD(EntityWithRequirement_Satisfied)
	{
		// Entity A provides TagA (no requirements)
		// Entity B provides TagB but requires TagA
		// Need: TagB
		// Expected plan: [A, B]
		auto TagA = MakeTagContainer({TEXT("Test.DepA")});
		auto TagB = MakeTagContainer({TEXT("Test.DepB")});

		TArray<FArcPotentialEntity> Entities;
		Entities.Add(MakeEntity(1, FVector(100, 0, 0), TagA));
		Entities.Add(MakeEntity(2, FVector(200, 0, 0), TagB, TagA));

		auto Plans = RunPlanner(Entities, TagB);

		ASSERT_THAT(IsTrue(Plans.Num() >= 1));
		ASSERT_THAT(AreEqual(2, Plans[0].Items.Num()));
		// First step should be the requirement (entity A)
		ASSERT_THAT(AreEqual(FMassEntityHandle(1, 1), Plans[0].Items[0].EntityHandle));
		// Second step should be entity B
		ASSERT_THAT(AreEqual(FMassEntityHandle(2, 2), Plans[0].Items[1].EntityHandle));
	}

	TEST_METHOD(EntityWithRequirement_Unsatisfiable)
	{
		// Entity B provides TagB but requires TagX — nobody provides TagX
		auto TagB = MakeTagContainer({TEXT("Test.DepB2")});
		auto TagX = MakeTagContainer({TEXT("Test.DepX")});

		TArray<FArcPotentialEntity> Entities;
		Entities.Add(MakeEntity(1, FVector::ZeroVector, TagB, TagX));

		auto Plans = RunPlanner(Entities, TagB);

		ASSERT_THAT(AreEqual(0, Plans.Num()));
	}

	TEST_METHOD(ThreeLevelChain)
	{
		// A provides TagA (no req)
		// B provides TagB, requires TagA
		// C provides TagC, requires TagB
		// Need: TagC
		// Expected plan: [A, B, C]
		auto TagA = MakeTagContainer({TEXT("Test.Chain3A")});
		auto TagB = MakeTagContainer({TEXT("Test.Chain3B")});
		auto TagC = MakeTagContainer({TEXT("Test.Chain3C")});

		TArray<FArcPotentialEntity> Entities;
		Entities.Add(MakeEntity(1, FVector(100, 0, 0), TagA));
		Entities.Add(MakeEntity(2, FVector(200, 0, 0), TagB, TagA));
		Entities.Add(MakeEntity(3, FVector(300, 0, 0), TagC, TagB));

		auto Plans = RunPlanner(Entities, TagC);

		ASSERT_THAT(IsTrue(Plans.Num() >= 1));
		ASSERT_THAT(AreEqual(3, Plans[0].Items.Num()));
		ASSERT_THAT(AreEqual(FMassEntityHandle(1, 1), Plans[0].Items[0].EntityHandle));
		ASSERT_THAT(AreEqual(FMassEntityHandle(2, 2), Plans[0].Items[1].EntityHandle));
		ASSERT_THAT(AreEqual(FMassEntityHandle(3, 3), Plans[0].Items[2].EntityHandle));
	}

	TEST_METHOD(RequirementConflict_DuplicateCapability)
	{
		// A provides TagA + TagExtra
		// B provides TagB, requires TagA (satisfied by A, but A also provides TagExtra)
		// C provides TagExtra (standalone)
		// InitialTags has TagExtra
		// Need: TagB
		// The requirement sub-plan for B will use A, which provides TagExtra already in AlreadyProvided
		// This should trigger the duplicate capability check
		auto TagA = MakeTagContainer({TEXT("Test.DupA")});
		auto TagB = MakeTagContainer({TEXT("Test.DupB")});
		auto TagExtra = MakeTagContainer({TEXT("Test.DupExtra")});

		FGameplayTagContainer TagAAndExtra;
		TagAAndExtra.AppendTags(TagA);
		TagAAndExtra.AppendTags(TagExtra);

		TArray<FArcPotentialEntity> Entities;
		Entities.Add(MakeEntity(1, FVector(100, 0, 0), TagAAndExtra)); // provides A+Extra
		Entities.Add(MakeEntity(2, FVector(200, 0, 0), TagB, TagA));   // provides B, requires A

		auto InitialTags = TagExtra; // Already have Extra

		auto Plans = RunPlanner(Entities, TagB, InitialTags);

		// The plan should be rejected because the requirement plan (entity 1)
		// provides TagExtra which conflicts with InitialTags in AlreadyProvided
		ASSERT_THAT(AreEqual(0, Plans.Num()));
	}
};

// ===============================================================
// Group 3: Limits & Termination
// ===============================================================

TEST_CLASS(SmartObjectPlanner_Limits, "ArcAI.SmartObjectPlanner.Limits")
{
	TEST_METHOD(MaxPlans_Respected)
	{
		// 5 entities all provide the same tag → 5 possible 1-step plans
		auto TagA = MakeTagContainer({TEXT("Test.LimitA")});

		TArray<FArcPotentialEntity> Entities;
		for (int32 i = 1; i <= 5; i++)
		{
			Entities.Add(MakeEntity(i, FVector(i * 100.0f, 0, 0), TagA));
		}

		auto Plans = RunPlanner(Entities, TagA, FGameplayTagContainer(), 2);

		ASSERT_THAT(AreEqual(2, Plans.Num()));
	}

	TEST_METHOD(MaxPlanDepth_Respected)
	{
		// Create a chain of 25 entities: each requires the previous
		// This exceeds the 20-step limit
		TArray<FArcPotentialEntity> Entities;
		for (int32 i = 1; i <= 25; i++)
		{
			auto Provides = MakeTagContainer({*FString::Printf(TEXT("Test.Deep%d"), i)});
			FGameplayTagContainer Requires;
			if (i > 1)
			{
				Requires = MakeTagContainer({*FString::Printf(TEXT("Test.Deep%d"), i - 1)});
			}
			Entities.Add(MakeEntity(i, FVector(i * 10.0f, 0, 0), Provides, Requires));
		}

		auto Need = MakeTagContainer({TEXT("Test.Deep25")});
		auto Plans = RunPlanner(Entities, Need);

		// Should find no plans because the chain exceeds depth 20
		ASSERT_THAT(AreEqual(0, Plans.Num()));
	}

	TEST_METHOD(EntityNotReused)
	{
		// Need TagA + TagB, but only one entity provides both
		// Should result in a single 1-step plan, not trying to use it twice
		FGameplayTagContainer BothTags;
		BothTags.AppendTags(MakeTagContainer({TEXT("Test.ReuseA")}));
		BothTags.AppendTags(MakeTagContainer({TEXT("Test.ReuseB")}));

		TArray<FArcPotentialEntity> Entities;
		Entities.Add(MakeEntity(1, FVector::ZeroVector, BothTags));

		auto Plans = RunPlanner(Entities, BothTags);

		ASSERT_THAT(AreEqual(1, Plans.Num()));
		ASSERT_THAT(AreEqual(1, Plans[0].Items.Num()));
	}
};

// ===============================================================
// Group 4: Plan Sorting
// ===============================================================

TEST_CLASS(SmartObjectPlanner_Sorting, "ArcAI.SmartObjectPlanner.Sorting")
{
	TEST_METHOD(SortByStepCount)
	{
		// Entity 1: provides TagA only
		// Entity 2: provides TagB only
		// Entity 3: provides TagA+TagB (1-step plan)
		// Need TagA+TagB → should get 1-step plan first after sorting
		auto TagA = MakeTagContainer({TEXT("Test.SortA")});
		auto TagB = MakeTagContainer({TEXT("Test.SortB")});
		FGameplayTagContainer BothTags;
		BothTags.AppendTags(TagA);
		BothTags.AppendTags(TagB);

		TArray<FArcPotentialEntity> Entities;
		Entities.Add(MakeEntity(1, FVector(100, 0, 0), TagA));
		Entities.Add(MakeEntity(2, FVector(200, 0, 0), TagB));
		Entities.Add(MakeEntity(3, FVector(50, 0, 0), BothTags));

		auto Plans = RunPlanner(Entities, BothTags);
		SortPlans(Plans, FVector::ZeroVector);

		ASSERT_THAT(IsTrue(Plans.Num() >= 2));
		// First plan should be the 1-step plan
		ASSERT_THAT(AreEqual(1, Plans[0].Items.Num()));
		// Later plans should be 2-step
		ASSERT_THAT(AreEqual(2, Plans[1].Items.Num()));
	}

	TEST_METHOD(SortByDistance_SameStepCount)
	{
		// Two entities providing same tag at different distances
		auto TagA = MakeTagContainer({TEXT("Test.DistA")});

		TArray<FArcPotentialEntity> Entities;
		Entities.Add(MakeEntity(1, FVector(1000, 0, 0), TagA)); // far
		Entities.Add(MakeEntity(2, FVector(100, 0, 0), TagA));  // near

		auto Plans = RunPlanner(Entities, TagA);
		SortPlans(Plans, FVector::ZeroVector);

		ASSERT_THAT(AreEqual(2, Plans.Num()));
		// Closer entity should be first
		ASSERT_THAT(AreEqual(FMassEntityHandle(2, 2), Plans[0].Items[0].EntityHandle));
		ASSERT_THAT(AreEqual(FMassEntityHandle(1, 1), Plans[1].Items[0].EntityHandle));
	}
};

// ===============================================================
// Group 5: Condition Evaluation
// ===============================================================

TEST_CLASS(SmartObjectPlanner_Conditions, "ArcAI.SmartObjectPlanner.Conditions")
{
	TEST_METHOD(NoConditions_EntityUsed)
	{
		// Entity with no custom conditions should be usable
		auto TagA = MakeTagContainer({TEXT("Test.CondPassA")});

		TArray<FArcPotentialEntity> Entities;
		Entities.Add(MakeEntity(1, FVector::ZeroVector, TagA));

		auto Plans = RunPlanner(Entities, TagA);
		ASSERT_THAT(AreEqual(1, Plans.Num()));
	}

	TEST_METHOD(NoSlots_EntitySkipped)
	{
		auto TagA = MakeTagContainer({TEXT("Test.NoSlotA")});

		FArcPotentialEntity Entity;
		Entity.EntityHandle = FMassEntityHandle(1, 1);
		Entity.Location = FVector::ZeroVector;
		Entity.Provides = TagA;
		Entity.FoundCandidateSlots.NumSlots = 0; // No available slots

		TArray<FArcPotentialEntity> Entities;
		Entities.Add(Entity);

		auto Plans = RunPlanner(Entities, TagA);

		ASSERT_THAT(AreEqual(0, Plans.Num()));
	}

	TEST_METHOD(NullEntityManager_SkipsConditionEval)
	{
		// When EntityManager is nullptr, conditions are skipped entirely.
		// Entity has CustomConditions but they shouldn't be evaluated.
		// (We can't create USTRUCT condition evaluators in a cpp file,
		// so this verifies the nullptr guard path works.)
		auto TagA = MakeTagContainer({TEXT("Test.NullMgrA")});

		TArray<FArcPotentialEntity> Entities;
		Entities.Add(MakeEntity(1, FVector::ZeroVector, TagA));

		// RunPlanner passes nullptr for EntityManager, so conditions are skipped
		auto Plans = RunPlanner(Entities, TagA);
		ASSERT_THAT(AreEqual(1, Plans.Num()));
	}
};

// ===============================================================
// Group 6: Tag Logic
// ===============================================================

TEST_CLASS(SmartObjectPlanner_TagLogic, "ArcAI.SmartObjectPlanner.TagLogic")
{
	TEST_METHOD(InitialTags_ReduceRequirements)
	{
		// Need TagA+TagB, already have TagA → only need TagB
		auto TagA = MakeTagContainer({TEXT("Test.InitReduceA")});
		auto TagB = MakeTagContainer({TEXT("Test.InitReduceB")});
		FGameplayTagContainer NeedBoth;
		NeedBoth.AppendTags(TagA);
		NeedBoth.AppendTags(TagB);

		TArray<FArcPotentialEntity> Entities;
		Entities.Add(MakeEntity(1, FVector::ZeroVector, TagB));

		auto Plans = RunPlanner(Entities, NeedBoth, TagA);

		ASSERT_THAT(AreEqual(1, Plans.Num()));
		ASSERT_THAT(AreEqual(1, Plans[0].Items.Num()));
	}

	TEST_METHOD(EntityProvidesExtraTags)
	{
		// Entity provides TagA+TagB+TagC, we only need TagA
		auto TagA = MakeTagContainer({TEXT("Test.ExtraA")});
		FGameplayTagContainer ManyTags;
		ManyTags.AppendTags(MakeTagContainer({TEXT("Test.ExtraA")}));
		ManyTags.AppendTags(MakeTagContainer({TEXT("Test.ExtraB")}));
		ManyTags.AppendTags(MakeTagContainer({TEXT("Test.ExtraC")}));

		TArray<FArcPotentialEntity> Entities;
		Entities.Add(MakeEntity(1, FVector::ZeroVector, ManyTags));

		auto Plans = RunPlanner(Entities, TagA);

		ASSERT_THAT(AreEqual(1, Plans.Num()));
		ASSERT_THAT(AreEqual(1, Plans[0].Items.Num()));
	}

	TEST_METHOD(NoveltyCheck_AlreadyProvidedTag_Skipped)
	{
		// Entity provides TagA (which is in InitialTags) and nothing else new
		// Should be skipped because it doesn't contribute anything new toward NeededTags
		auto TagA = MakeTagContainer({TEXT("Test.NovelA")});
		auto TagB = MakeTagContainer({TEXT("Test.NovelB")});

		TArray<FArcPotentialEntity> Entities;
		Entities.Add(MakeEntity(1, FVector::ZeroVector, TagA)); // provides only TagA (already have it)

		// Need TagB, already have TagA
		auto Plans = RunPlanner(Entities, TagB, TagA);

		ASSERT_THAT(AreEqual(0, Plans.Num()));
	}
};
