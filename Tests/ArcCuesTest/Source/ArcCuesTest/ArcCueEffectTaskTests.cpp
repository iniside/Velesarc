// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "Components/ActorTestSpawner.h"
#include "Engine/World.h"
#include "GameplayTagContainer.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityView.h"
#include "Mass/EntityFragments.h"
#include "Mass/EntityHandle.h"
#include "NativeGameplayTags.h"
#include "StructUtils/InstancedStruct.h"

#include "Effects/ArcEffectDefinition.h"
#include "Effects/ArcEffectFunctions.h"
#include "Effects/ArcEffectTask.h"
#include "Tasks/ArcEffectTask_FireCue.h"
#include "Fragments/ArcAggregatorFragment.h"
#include "Fragments/ArcEffectStackFragment.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Fragments/ArcPendingAttributeOpsFragment.h"

#include "ArcCues/ArcCueDefinition.h"
#include "ArcCues/ArcCueSubsystem.h"

#include "ArcCuesTestHelpers.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ArcCuesEffectTest_Cue, "ArcCuesTest.Effect.Cue");

namespace ArcCuesEffectTaskTests_Internal
{
	FMassEntityHandle CreateEffectEntity(FMassEntityManager& EntityManager)
	{
		// Minimal entity carrying the fragments TryApplyEffect expects.
		TArray<FInstancedStruct> Instances;
		Instances.Add(FInstancedStruct::Make(FArcEffectStackFragment()));
		Instances.Add(FInstancedStruct::Make(FArcAggregatorFragment()));
		Instances.Add(FInstancedStruct::Make(FArcOwnedTagsFragment()));
		Instances.Add(FInstancedStruct::Make(FArcPendingAttributeOpsFragment()));
		Instances.Add(FInstancedStruct::Make(FTransformFragment()));
		return EntityManager.CreateEntity(Instances);
	}
}

TEST_CLASS(ArcCues_EffectTask, "ArcCues.EffectTask")
{
	FActorTestSpawner Spawner;
	UWorld* World = nullptr;
	FMassEntityManager* EM = nullptr;
	UArcCueSubsystem* Subsystem = nullptr;
	TArray<UArcCueDefinition*> RegisteredDefs;

	BEFORE_EACH()
	{
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();

		World = &Spawner.GetWorld();
		EM = &ArcCuesTest::GetEntityManager(*World);
		Subsystem = World->GetSubsystem<UArcCueSubsystem>();
		check(Subsystem);
	}

	AFTER_EACH()
	{
		for (UArcCueDefinition* Def : RegisteredDefs)
		{
			if (Def != nullptr)
			{
				Subsystem->UnregisterDefinitionForTests(Def);
				ArcCuesTest::ReleaseTransientCueDef(Def);
			}
		}
		RegisteredDefs.Reset();
	}

	UArcCueDefinition* RegisterPersistentProbeDef(FName DefName, FName ProbeName)
	{
		UArcCueDefinition* Def = ArcCuesTest::CreateProbeDef(
			DefName, TAG_ArcCuesEffectTest_Cue, FGameplayTagContainer(), ProbeName, /*bPersistent*/ true);
		Subsystem->RegisterDefinitionForTests(Def);
		RegisteredDefs.Add(Def);
		return Def;
	}

	UArcCueDefinition* RegisterOneShotProbeDef(FName DefName, FName ProbeName)
	{
		UArcCueDefinition* Def = ArcCuesTest::CreateProbeDef(
			DefName, TAG_ArcCuesEffectTest_Cue, FGameplayTagContainer(), ProbeName, /*bPersistent*/ false);
		Subsystem->RegisterDefinitionForTests(Def);
		RegisteredDefs.Add(Def);
		return Def;
	}

	TEST_METHOD(FireCueTask_OnApplicationFires_WithPersistentTrue_AddCue)
	{
		RegisterPersistentProbeDef(TEXT("PersistentEffectCueDef"), TEXT("EffPersist"));

		const FMassEntityHandle Entity = ArcCuesEffectTaskTests_Internal::CreateEffectEntity(*EM);

		TestRunner->AddExpectedMessage(TEXT("PROBE/EffPersist/APP"), ELogVerbosity::Display, EAutomationExpectedMessageFlags::Contains, 1);
		TestRunner->AddExpectedMessage(TEXT("PROBE/EffPersist/REM"), ELogVerbosity::Display, EAutomationExpectedMessageFlags::Contains, 1);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Infinite;

		FArcEffectTask_FireCue Task;
		Task.CueTag = TAG_ArcCuesEffectTest_Cue;
		Task.bPersistent = true;
		Effect->Tasks.Add(FInstancedStruct::Make(Task));

		ArcEffects::TryApplyEffect(*EM, Entity, Effect, FMassEntityHandle());
		FArcEffectRemovalQuery RemoveQuery;
		RemoveQuery.Definition = Effect;
		ArcEffects::RemoveEffects(*EM, Entity, RemoveQuery);
	}

	TEST_METHOD(FireCueTask_OnApplicationFires_WithPersistentFalse_ExecuteCue)
	{
		RegisterOneShotProbeDef(TEXT("OneShotEffectCueDef"), TEXT("EffOneShot"));

		const FMassEntityHandle Entity = ArcCuesEffectTaskTests_Internal::CreateEffectEntity(*EM);

		// Persistent=false on the FireCue task means OnExecute path on the cue,
		// not OnApplication/OnRemoved. Removing the effect must not trigger REM.
		TestRunner->AddExpectedMessage(TEXT("PROBE/EffOneShot/EXEC"), ELogVerbosity::Display, EAutomationExpectedMessageFlags::Contains, 1);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Infinite;

		FArcEffectTask_FireCue Task;
		Task.CueTag = TAG_ArcCuesEffectTest_Cue;
		Task.bPersistent = false;
		Effect->Tasks.Add(FInstancedStruct::Make(Task));

		ArcEffects::TryApplyEffect(*EM, Entity, Effect, FMassEntityHandle());
		FArcEffectRemovalQuery RemoveQuery;
		RemoveQuery.Definition = Effect;
		ArcEffects::RemoveEffects(*EM, Entity, RemoveQuery);
	}

	TEST_METHOD(FireCueTask_PeriodicExecute_FiresExecuteCuePerTick)
	{
		RegisterOneShotProbeDef(TEXT("PeriodicEffectCueDef"), TEXT("EffPeriodic"));

		const FMassEntityHandle Entity = ArcCuesEffectTaskTests_Internal::CreateEffectEntity(*EM);

		// Apply once + drive OnExecute thrice → expect 4 EXEC firings (1 from
		// TryApplyEffect's initial EXEC + 3 from the explicit ticks).
		TestRunner->AddExpectedMessage(TEXT("PROBE/EffPeriodic/EXEC"), ELogVerbosity::Display, EAutomationExpectedMessageFlags::Contains, 4);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Infinite;

		FArcEffectTask_FireCue Task;
		Task.CueTag = TAG_ArcCuesEffectTest_Cue;
		Task.bPersistent = false;
		Effect->Tasks.Add(FInstancedStruct::Make(Task));

		ArcEffects::TryApplyEffect(*EM, Entity, Effect, FMassEntityHandle());

		FMassEntityView View(*EM, Entity);
		FArcEffectStackFragment* Stack = View.GetFragmentDataPtr<FArcEffectStackFragment>();
		ASSERT_THAT(IsNotNull(Stack));
		ASSERT_THAT(AreEqual(1, Stack->ActiveEffects.Num()));

		FArcActiveEffect& Active = Stack->ActiveEffects[0];
		ASSERT_THAT(AreEqual(1, Active.Tasks.Num()));

		FArcEffectTask* TaskPtr = Active.Tasks[0].GetMutablePtr<FArcEffectTask>();
		ASSERT_THAT(IsNotNull(TaskPtr));

		TaskPtr->OnExecute(Active);
		TaskPtr->OnExecute(Active);
		TaskPtr->OnExecute(Active);
	}
};
