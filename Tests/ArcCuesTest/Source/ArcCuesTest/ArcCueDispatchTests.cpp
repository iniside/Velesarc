// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "Components/ActorTestSpawner.h"
#include "Engine/World.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"

#include "ArcCues/ArcCueDefinition.h"
#include "ArcCues/ArcCueFunctions.h"
#include "ArcCues/ArcCueSubsystem.h"
#include "ArcCues/ArcCueTypes.h"

#include "ArcCuesTestHelpers.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ArcCuesTest_Cue_Impact, "ArcCuesTest.Cue.Impact");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ArcCuesTest_Surface_Stone, "ArcCuesTest.Surface.Stone");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ArcCuesTest_Surface_Wood, "ArcCuesTest.Surface.Wood");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ArcCuesTest_Cue_Missing, "ArcCuesTest.Cue.Missing");

TEST_CLASS(ArcCues_Dispatch, "ArcCues.Dispatch")
{
	FActorTestSpawner Spawner;
	UWorld* World = nullptr;
	UArcCueSubsystem* Subsystem = nullptr;
	UArcCueDefinition* StoneDef = nullptr;
	UArcCueDefinition* WoodDef = nullptr;

	BEFORE_EACH()
	{
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();

		World = &Spawner.GetWorld();
		Subsystem = World->GetSubsystem<UArcCueSubsystem>();
		check(Subsystem);
	}

	AFTER_EACH()
	{
		if (StoneDef != nullptr)
		{
			Subsystem->UnregisterDefinitionForTests(StoneDef);
			ArcCuesTest::ReleaseTransientCueDef(StoneDef);
			StoneDef = nullptr;
		}
		if (WoodDef != nullptr)
		{
			Subsystem->UnregisterDefinitionForTests(WoodDef);
			ArcCuesTest::ReleaseTransientCueDef(WoodDef);
			WoodDef = nullptr;
		}
	}

	TEST_METHOD(Discovery_TwoDefsSameTagDifferentFilters_OnlyMatchingFiresOnExecute)
	{
		FGameplayTagContainer StoneFilter;
		StoneFilter.AddTag(TAG_ArcCuesTest_Surface_Stone);
		StoneDef = ArcCuesTest::CreateProbeDef(
			TEXT("StoneCueDef"), TAG_ArcCuesTest_Cue_Impact, StoneFilter, TEXT("Stone"), /*bPersistent*/ false);

		FGameplayTagContainer WoodFilter;
		WoodFilter.AddTag(TAG_ArcCuesTest_Surface_Wood);
		WoodDef = ArcCuesTest::CreateProbeDef(
			TEXT("WoodCueDef"), TAG_ArcCuesTest_Cue_Impact, WoodFilter, TEXT("Wood"), /*bPersistent*/ false);

		Subsystem->RegisterDefinitionForTests(StoneDef);
		Subsystem->RegisterDefinitionForTests(WoodDef);

		// Exclusivity: any PROBE EXEC firing must occur exactly once across both
		// definitions (each log line satisfies at most one expectation, so we use
		// a single regex that matches BOTH probes' lines — Stone firing alone
		// gives count 1, both firing gives count 2 → fail).
		TestRunner->AddExpectedMessage(TEXT("PROBE/(Stone|Wood)/EXEC"), ELogVerbosity::Display, EAutomationExpectedMessageFlags::Contains, 1);

		FArcCueParameters Params;
		Params.Tags.AddTag(TAG_ArcCuesTest_Surface_Stone);
		ArcCues::ExecuteCue(World, TAG_ArcCuesTest_Cue_Impact, Params);
	}

	TEST_METHOD(Discovery_NoMatchingDefinition_DoesNothing)
	{
		FArcCueParameters Params;
		ArcCues::ExecuteCue(World, TAG_ArcCuesTest_Cue_Missing, Params);

		const FArcCueHandle Handle = ArcCues::AddCue(World, TAG_ArcCuesTest_Cue_Missing, Params);
		ASSERT_THAT(IsFalse(Handle.IsValid()));
	}

	TEST_METHOD(AddCue_EmptyBehaviorState_ReturnsValidHandle_RemoveCueRemovesInstance)
	{
		StoneDef = ArcCuesTest::CreateProbeDef(
			TEXT("OneShotOnlyDef"), TAG_ArcCuesTest_Cue_Impact, FGameplayTagContainer(), TEXT("OneShot"), /*bPersistent*/ false);
		Subsystem->RegisterDefinitionForTests(StoneDef);

		TestRunner->AddExpectedMessage(TEXT("PROBE/OneShot/APP"), ELogVerbosity::Display, EAutomationExpectedMessageFlags::Contains, 1);
		TestRunner->AddExpectedMessage(TEXT("PROBE/OneShot/EVENT"), ELogVerbosity::Display, EAutomationExpectedMessageFlags::Contains, 1);

		FArcCueParameters Params;
		const FArcCueHandle Handle = ArcCues::AddCue(World, TAG_ArcCuesTest_Cue_Impact, Params);
		ASSERT_THAT(IsTrue(Handle.IsValid()));

		const FArcCueEvent Event;
		ArcCues::SendCueEvent(World, Handle, Event);
		ArcCues::RemoveCue(World, Handle);
		ArcCues::SendCueEvent(World, Handle, Event);
	}

	TEST_METHOD(AddCue_PersistentBehavior_ReturnsValidHandle_RemoveCueRunsOnRemoved)
	{
		StoneDef = ArcCuesTest::CreateProbeDef(
			TEXT("PersistentDef"), TAG_ArcCuesTest_Cue_Impact, FGameplayTagContainer(), TEXT("Persist"), /*bPersistent*/ true);
		Subsystem->RegisterDefinitionForTests(StoneDef);

		TestRunner->AddExpectedMessage(TEXT("PROBE/Persist/APP"), ELogVerbosity::Display, EAutomationExpectedMessageFlags::Contains, 1);
		TestRunner->AddExpectedMessage(TEXT("PROBE/Persist/REM"), ELogVerbosity::Display, EAutomationExpectedMessageFlags::Contains, 1);

		FArcCueParameters Params;
		const FArcCueHandle Handle = ArcCues::AddCue(World, TAG_ArcCuesTest_Cue_Impact, Params);
		ASSERT_THAT(IsTrue(Handle.IsValid()));

		ArcCues::RemoveCue(World, Handle);
	}
};
