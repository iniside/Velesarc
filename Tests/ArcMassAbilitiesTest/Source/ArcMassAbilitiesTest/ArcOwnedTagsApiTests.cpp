// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcMassAbilitiesTestTypes.h"
#include "Components/ActorTestSpawner.h"
#include "Abilities/ArcAbilityDefinition.h"
#include "Abilities/ArcAbilityFunctions.h"
#include "Abilities/ArcAbilityStateTreeSubsystem.h"
#include "Attributes/ArcAggregator.h"
#include "Attributes/ArcAttributeAggregatorProcessor.h"
#include "Attributes/ArcAttributeHandlerConfig.h"
#include "Attributes/ArcAttributeNotification.h"
#include "Events/ArcAbilityEventPayloads.h"
#include "Events/ArcGameEvent.h"
#include "Events/ArcGameEventSubsystem.h"
#include "Events/ArcGameEventTags.h"
#include "Events/ArcTagEventPayloads.h"
#include "Fragments/ArcAbilityCollectionFragment.h"
#include "Fragments/ArcAbilityCooldownFragment.h"
#include "Fragments/ArcAbilityInputFragment.h"
#include "Fragments/ArcAggregatorFragment.h"
#include "Fragments/ArcEffectStackFragment.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Fragments/ArcPendingAttributeOpsFragment.h"
#include "Processors/ArcAbilityInputProcessor.h"
#include "Signals/ArcAbilitySignals.h"
#include "Signals/ArcAttributeRecalculateSignal.h"
#include "Tags/ArcGameplayTagRequirements.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityView.h"
#include "Mass/ExecutionContext.h"
#include "Mass/Executor.h"
#include "Mass/ProcessingContext.h"
#include "MassSignalSubsystem.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "StructUtils/InstancedStruct.h"
#include "StructUtils/SharedStruct.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_OwnedApi_Fire, "Test.OwnedApi.Fire");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_OwnedApi_Ice, "Test.OwnedApi.Ice");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_OwnedApi_State_Firing, "Test.OwnedApi.State.Firing");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_OwnedApi_Ability_Fire, "Test.OwnedApi.Ability.Fire");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_OwnedApi_Ability_Dodge, "Test.OwnedApi.Ability.Dodge");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_OwnedApi_Input_Fire, "Test.OwnedApi.Input.Fire");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_OwnedApi_Input_Dodge, "Test.OwnedApi.Input.Dodge");

namespace ArcOwnedTagsApiTestsPrivate
{
	FMassEntityHandle CreateAbilityEntity(FMassEntityManager& EntityManager)
	{
		TArray<FInstancedStruct> Instances;
		Instances.Add(FInstancedStruct::Make(FArcAttributeSet_TestStats()));
		Instances.Add(FInstancedStruct::Make(FArcAbilityCollectionFragment()));
		Instances.Add(FInstancedStruct::Make(FArcAbilityInputFragment()));
		Instances.Add(FInstancedStruct::Make(FArcOwnedTagsFragment()));
		Instances.Add(FInstancedStruct::Make(FArcAbilityCooldownFragment()));
		Instances.Add(FInstancedStruct::Make(FArcEffectStackFragment()));
		Instances.Add(FInstancedStruct::Make(FArcAggregatorFragment()));
		Instances.Add(FInstancedStruct::Make(FArcPendingAttributeOpsFragment()));
		return ArcMassAbilitiesTestHelpers::CreateEntityFromInstances(EntityManager, Instances);
	}

	UArcAbilityDefinition* CreateAbilityDefinition(const FGameplayTag& AbilityTag)
	{
		UArcAbilityDefinition* Def = NewObject<UArcAbilityDefinition>();
		Def->AbilityTags.AddTag(AbilityTag);
		return Def;
	}
}

// ============================================================================
// ArcOwnedTagsApi - AddOwnedTags / RemoveOwnedTags contract
// ============================================================================

TEST_CLASS(ArcOwnedTagsApi, "ArcMassAbilities.Tags.OwnedApi")
{
	struct FTagChange
	{
		FGameplayTag Channel;
		FGameplayTag Tag;
		int32 OldCount = 0;
		int32 NewCount = 0;
	};

	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	UMassSignalSubsystem* SignalSubsystem = nullptr;
	UArcGameEventSubsystem* Bus = nullptr;
	UArcAttributeAggregatorProcessor* AggregatorProcessor = nullptr;

	FDelegateHandle RecalculateHandle;
	FDelegateHandle TagAddedHandle;
	FDelegateHandle TagRemovedHandle;
	TArray<FMassEntityHandle> RecalculateSignals;
	TArray<FTagChange> TagChanges;

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogUObjectGlobals");
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogRHI");
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();

		UMassEntitySubsystem* MES = Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
		check(MES);
		EntityManager = &MES->GetMutableEntityManager();

		SignalSubsystem = Spawner.GetWorld().GetSubsystem<UMassSignalSubsystem>();
		check(SignalSubsystem);
		Bus = Spawner.GetWorld().GetSubsystem<UArcGameEventSubsystem>();
		check(Bus);

		TSharedRef<FMassEntityManager> SharedEM = EntityManager->AsShared();
		AggregatorProcessor = NewObject<UArcAttributeAggregatorProcessor>();
		AggregatorProcessor->CallInitialize(EntityManager->GetOwner(), SharedEM);

		RecalculateHandle = SignalSubsystem->GetSignalDelegateByName(UE::ArcMassAbilities::Signals::AttributeRecalculate).AddLambda(
			[this](FName /*SignalName*/, TConstArrayView<FMassEntityHandle> Entities)
			{
				RecalculateSignals.Append(Entities.GetData(), Entities.Num());
			});

		const FArcGameEventDelegate RecordTagChange = FArcGameEventDelegate::CreateLambda([this](const FArcGameEvent& Event)
		{
			const FArcTagChangedPayload* Payload = Event.Payload.GetPtr<FArcTagChangedPayload>();
			if (Payload)
			{
				FTagChange Change;
				Change.Channel = Event.Tag;
				Change.Tag = Payload->Tag;
				Change.OldCount = Payload->OldCount;
				Change.NewCount = Payload->NewCount;
				TagChanges.Add(Change);
			}
		});
		TagAddedHandle = Bus->Subscribe(ArcGameEventTags::TagAdded.GetTag(), FMassEntityHandle(), EArcEventRole::Any, RecordTagChange);
		TagRemovedHandle = Bus->Subscribe(ArcGameEventTags::TagRemoved.GetTag(), FMassEntityHandle(), EArcEventRole::Any, RecordTagChange);
	}

	AFTER_EACH()
	{
		SignalSubsystem->GetSignalDelegateByName(UE::ArcMassAbilities::Signals::AttributeRecalculate).Remove(RecalculateHandle);
		Bus->Unsubscribe(TagAddedHandle);
		Bus->Unsubscribe(TagRemovedHandle);
	}

	void Drain()
	{
		Bus->DrainOnPostActorTick(&Spawner.GetWorld(), LEVELTICK_All, 0.f);
	}

	const FArcOwnedTagsFragment& GetOwnedTags(FMassEntityHandle Entity)
	{
		FMassEntityView View(*EntityManager, Entity);
		return *View.GetSparseFragmentDataPtr<FArcOwnedTagsFragment>();
	}

	float GetArmorCurrent(FMassEntityHandle Entity)
	{
		FMassEntityView View(*EntityManager, Entity);
		return View.GetSparseFragmentDataPtr<FArcAttributeSet_TestStats>()->Armor.CurrentValue;
	}

	void RunAggregatorOnSignalsAlreadyRaised()
	{
		FMassProcessingContext ProcessingContext(*EntityManager, 0.f);
		UE::Mass::Executor::Run(*AggregatorProcessor, ProcessingContext);
	}

	TEST_METHOD(AddOwnedTags_RaisesCountPerCall)
	{
		const FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);
		FGameplayTagContainer Tags;
		Tags.AddTag(TAG_OwnedApi_Fire);

		ASSERT_THAT(AreEqual(0, GetOwnedTags(Entity).GetTags().GetCount(TAG_OwnedApi_Fire)));

		ArcAbilities::AddOwnedTags(*EntityManager, Entity, Tags);
		ASSERT_THAT(AreEqual(1, GetOwnedTags(Entity).GetTags().GetCount(TAG_OwnedApi_Fire)));
		ASSERT_THAT(IsTrue(GetOwnedTags(Entity).GetTags().HasTag(TAG_OwnedApi_Fire)));

		ArcAbilities::AddOwnedTags(*EntityManager, Entity, Tags);
		ASSERT_THAT(AreEqual(2, GetOwnedTags(Entity).GetTags().GetCount(TAG_OwnedApi_Fire)));
	}

	TEST_METHOD(RemoveOwnedTags_LowersCountAndClearsTagAtZero)
	{
		const FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);
		FGameplayTagContainer Tags;
		Tags.AddTag(TAG_OwnedApi_Fire);
		ArcAbilities::AddOwnedTags(*EntityManager, Entity, Tags);
		ArcAbilities::AddOwnedTags(*EntityManager, Entity, Tags);

		ArcAbilities::RemoveOwnedTags(*EntityManager, Entity, Tags);
		ASSERT_THAT(AreEqual(1, GetOwnedTags(Entity).GetTags().GetCount(TAG_OwnedApi_Fire)));
		ASSERT_THAT(IsTrue(GetOwnedTags(Entity).GetTags().HasTag(TAG_OwnedApi_Fire)));

		ArcAbilities::RemoveOwnedTags(*EntityManager, Entity, Tags);
		ASSERT_THAT(AreEqual(0, GetOwnedTags(Entity).GetTags().GetCount(TAG_OwnedApi_Fire)));
		ASSERT_THAT(IsFalse(GetOwnedTags(Entity).GetTags().HasTag(TAG_OwnedApi_Fire)));
	}

	TEST_METHOD(CountChanges_PublishOnlyTheEdges_AddedAtZeroToOneRemovedAtOneToZero)
	{
		const FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);
		FGameplayTagContainer Tags;
		Tags.AddTag(TAG_OwnedApi_Fire);

		ArcAbilities::AddOwnedTags(*EntityManager, Entity, Tags);
		ArcAbilities::AddOwnedTags(*EntityManager, Entity, Tags);
		ArcAbilities::RemoveOwnedTags(*EntityManager, Entity, Tags);
		Drain();

		ASSERT_THAT(AreEqual(1, TagChanges.Num(), TEXT("0->1 is an edge, 1->2 and 2->1 are not")));
		ASSERT_THAT(IsTrue(TagChanges[0].Channel == ArcGameEventTags::TagAdded.GetTag()));
		ASSERT_THAT(IsTrue(TagChanges[0].Tag == TAG_OwnedApi_Fire));
		ASSERT_THAT(AreEqual(0, TagChanges[0].OldCount));
		ASSERT_THAT(AreEqual(1, TagChanges[0].NewCount));

		ArcAbilities::RemoveOwnedTags(*EntityManager, Entity, Tags);
		Drain();

		ASSERT_THAT(AreEqual(2, TagChanges.Num()));
		ASSERT_THAT(IsTrue(TagChanges[1].Channel == ArcGameEventTags::TagRemoved.GetTag()));
		ASSERT_THAT(IsTrue(TagChanges[1].Tag == TAG_OwnedApi_Fire));
		ASSERT_THAT(AreEqual(1, TagChanges[1].OldCount));
		ASSERT_THAT(AreEqual(0, TagChanges[1].NewCount));
	}

	TEST_METHOD(MultiTagCall_PublishesOneEventPerChangedTag)
	{
		const FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);
		FGameplayTagContainer Tags;
		Tags.AddTag(TAG_OwnedApi_Fire);
		Tags.AddTag(TAG_OwnedApi_Ice);

		ArcAbilities::AddOwnedTags(*EntityManager, Entity, Tags);
		Drain();

		ASSERT_THAT(AreEqual(2, TagChanges.Num()));
		const bool bSawFire = TagChanges.ContainsByPredicate([](const FTagChange& Change) { return Change.Tag == TAG_OwnedApi_Fire && Change.NewCount == 1; });
		const bool bSawIce = TagChanges.ContainsByPredicate([](const FTagChange& Change) { return Change.Tag == TAG_OwnedApi_Ice && Change.NewCount == 1; });
		ASSERT_THAT(IsTrue(bSawFire));
		ASSERT_THAT(IsTrue(bSawIce));
	}

	TEST_METHOD(CountChange_SignalsAttributeRecalculateForThatEntity)
	{
		const FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);
		FGameplayTagContainer Tags;
		Tags.AddTag(TAG_OwnedApi_Fire);

		ArcAbilities::AddOwnedTags(*EntityManager, Entity, Tags);
		ASSERT_THAT(IsTrue(RecalculateSignals.Num() >= 1));
		ASSERT_THAT(IsTrue(RecalculateSignals.Contains(Entity)));

		RecalculateSignals.Reset();
		ArcAbilities::RemoveOwnedTags(*EntityManager, Entity, Tags);
		ASSERT_THAT(IsTrue(RecalculateSignals.Contains(Entity)));
	}

	TEST_METHOD(StackedCountChange_StillSignalsRecalculate)
	{
		const FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);
		FGameplayTagContainer Tags;
		Tags.AddTag(TAG_OwnedApi_Fire);
		ArcAbilities::AddOwnedTags(*EntityManager, Entity, Tags);

		RecalculateSignals.Reset();
		ArcAbilities::AddOwnedTags(*EntityManager, Entity, Tags);

		ASSERT_THAT(IsTrue(RecalculateSignals.Contains(Entity)));
	}

	TEST_METHOD(NoCountChange_QueuesNoEventAndRaisesNoSignal)
	{
		const FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);
		FGameplayTagContainer Tags;
		Tags.AddTag(TAG_OwnedApi_Fire);

		ArcAbilities::RemoveOwnedTags(*EntityManager, Entity, Tags);
		ArcAbilities::AddOwnedTags(*EntityManager, Entity, FGameplayTagContainer());
		ArcAbilities::RemoveOwnedTags(*EntityManager, Entity, FGameplayTagContainer());
		ArcAbilities::AddOwnedTags(*EntityManager, FMassEntityHandle(), Tags);
		Drain();

		ASSERT_THAT(AreEqual(0, RecalculateSignals.Num()));
		ASSERT_THAT(AreEqual(0, TagChanges.Num()));
		ASSERT_THAT(AreEqual(0, GetOwnedTags(Entity).GetTags().GetCount(TAG_OwnedApi_Fire)));
	}

	TEST_METHOD(AddOwnedTags_RequalifiesTagConditionalModOnNextAggregatorRun)
	{
		const FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager, 100.f, 10.f);

		FArcGameplayTagRequirements TargetReqs;
		TargetReqs.RequireTags.AddTag(TAG_OwnedApi_Fire);
		{
			FMassEntityView View(*EntityManager, Entity);
			FArcAggregatorFragment* AggFrag = View.GetFragmentDataPtr<FArcAggregatorFragment>();
			AggFrag->FindOrAddAggregator(FArcAttributeSet_TestStats::GetArmorAttribute())
				.AddMod(EArcModifierOp::Add, 50.f, FMassEntityHandle(), nullptr, &TargetReqs);
		}

		FGameplayTagContainer Tags;
		Tags.AddTag(TAG_OwnedApi_Fire);
		ArcAbilities::AddOwnedTags(*EntityManager, Entity, Tags);
		RunAggregatorOnSignalsAlreadyRaised();

		ASSERT_THAT(IsNear(60.f, GetArmorCurrent(Entity), 0.001f));
	}

	TEST_METHOD(RemoveOwnedTags_DisqualifiesTagConditionalModOnNextAggregatorRun)
	{
		const FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager, 100.f, 10.f);

		FArcGameplayTagRequirements TargetReqs;
		TargetReqs.RequireTags.AddTag(TAG_OwnedApi_Fire);
		{
			FMassEntityView View(*EntityManager, Entity);
			FArcAggregatorFragment* AggFrag = View.GetFragmentDataPtr<FArcAggregatorFragment>();
			AggFrag->FindOrAddAggregator(FArcAttributeSet_TestStats::GetArmorAttribute())
				.AddMod(EArcModifierOp::Add, 50.f, FMassEntityHandle(), nullptr, &TargetReqs);
		}

		FGameplayTagContainer Tags;
		Tags.AddTag(TAG_OwnedApi_Fire);
		ArcAbilities::AddOwnedTags(*EntityManager, Entity, Tags);
		RunAggregatorOnSignalsAlreadyRaised();
		ASSERT_THAT(IsNear(60.f, GetArmorCurrent(Entity), 0.001f));

		ArcAbilities::RemoveOwnedTags(*EntityManager, Entity, Tags);
		RunAggregatorOnSignalsAlreadyRaised();

		ASSERT_THAT(IsNear(10.f, GetArmorCurrent(Entity), 0.001f));
	}

	TEST_METHOD(ModStaysQualified_WhileAnotherSourceStillHoldsTheTag)
	{
		const FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager, 100.f, 10.f);

		FArcGameplayTagRequirements TargetReqs;
		TargetReqs.RequireTags.AddTag(TAG_OwnedApi_Fire);
		{
			FMassEntityView View(*EntityManager, Entity);
			FArcAggregatorFragment* AggFrag = View.GetFragmentDataPtr<FArcAggregatorFragment>();
			AggFrag->FindOrAddAggregator(FArcAttributeSet_TestStats::GetArmorAttribute())
				.AddMod(EArcModifierOp::Add, 50.f, FMassEntityHandle(), nullptr, &TargetReqs);
		}

		FGameplayTagContainer Tags;
		Tags.AddTag(TAG_OwnedApi_Fire);
		ArcAbilities::AddOwnedTags(*EntityManager, Entity, Tags);
		ArcAbilities::AddOwnedTags(*EntityManager, Entity, Tags);
		ArcAbilities::RemoveOwnedTags(*EntityManager, Entity, Tags);
		RunAggregatorOnSignalsAlreadyRaised();

		ASSERT_THAT(IsNear(60.f, GetArmorCurrent(Entity), 0.001f));
	}
};

// ============================================================================
// ArcAbilityEndReasonEvents - CancelAbility end reason / instigator and
// CancelAbilitiesWithTags AbilityEnded
// ============================================================================

TEST_CLASS(ArcAbilityEndReasonEvents, "ArcMassAbilities.Abilities.EndReasonEvents")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	UMassSignalSubsystem* SignalSubsystem = nullptr;
	UArcAbilityStateTreeSubsystem* AbilitySubsystem = nullptr;
	UArcGameEventSubsystem* GameEventSubsystem = nullptr;
	UArcAbilityInputProcessor* InputProcessor = nullptr;

	FDelegateHandle BusHandle;
	TArray<FArcGameEvent> BusEnded;

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogUObjectGlobals");
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogRHI");
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogStateTree");
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogMass");
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();

		UMassEntitySubsystem* MES = Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
		check(MES);
		EntityManager = &MES->GetMutableEntityManager();

		SignalSubsystem = Spawner.GetWorld().GetSubsystem<UMassSignalSubsystem>();
		check(SignalSubsystem);
		AbilitySubsystem = Spawner.GetWorld().GetSubsystem<UArcAbilityStateTreeSubsystem>();
		check(AbilitySubsystem);
		GameEventSubsystem = Spawner.GetWorld().GetSubsystem<UArcGameEventSubsystem>();
		check(GameEventSubsystem);

		TSharedRef<FMassEntityManager> SharedEM = EntityManager->AsShared();
		InputProcessor = NewObject<UArcAbilityInputProcessor>();
		InputProcessor->CallInitialize(&Spawner.GetWorld(), SharedEM);

		BusHandle = GameEventSubsystem->Subscribe(ArcGameEventTags::AbilityEnded, FMassEntityHandle(), EArcEventRole::Any,
			FArcGameEventDelegate::CreateLambda([this](const FArcGameEvent& Event)
			{
				BusEnded.Add(Event);
			}));
	}

	AFTER_EACH()
	{
		GameEventSubsystem->Unsubscribe(BusHandle);
	}

	void RunInputProcessor(FMassEntityHandle Entity)
	{
		SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::AbilityInputReceived, Entity);
		UE::Mass::FProcessingContext ProcessingContext(*EntityManager, 0.016f);
		InputProcessor->CallExecute(*EntityManager, ProcessingContext.GetExecutionContext());
	}

	FArcAbilityHandle GrantAndActivate(FMassEntityHandle Entity, const UArcAbilityDefinition* Def, FGameplayTag InputTag)
	{
		const FArcAbilityHandle Handle = ArcAbilities::GrantAbility(*EntityManager, Entity, Def, InputTag);
		ArcAbilities::PushInput(*EntityManager, Entity, InputTag);
		RunInputProcessor(Entity);
		return Handle;
	}

	void DrainBus()
	{
		GameEventSubsystem->DrainOnPostActorTick(&Spawner.GetWorld(), LEVELTICK_All, 0.f);
	}

	bool IsAbilityActive(FMassEntityHandle Entity, FArcAbilityHandle Handle)
	{
		const FArcActiveAbility* Ability = ArcAbilities::FindAbility(*EntityManager, Entity, Handle);
		return Ability && Ability->bIsActive;
	}

	bool OwnsTag(FMassEntityHandle Entity, const FGameplayTag& Tag)
	{
		FMassEntityView View(*EntityManager, Entity);
		return View.GetSparseFragmentDataPtr<FArcOwnedTagsFragment>()->GetTags().HasTag(Tag);
	}

	TEST_METHOD(CancelAbility_Interrupted_EndsAbilityWithReasonAndInstigator)
	{
		const FMassEntityHandle Entity = ArcOwnedTagsApiTestsPrivate::CreateAbilityEntity(*EntityManager);
		const FMassEntityHandle Instigator = ArcOwnedTagsApiTestsPrivate::CreateAbilityEntity(*EntityManager);
		UArcAbilityDefinition* Def = ArcOwnedTagsApiTestsPrivate::CreateAbilityDefinition(TAG_OwnedApi_Ability_Fire);
		Def->GrantTagsWhileActive.AddTag(TAG_OwnedApi_State_Firing);

		const FArcAbilityHandle Handle = GrantAndActivate(Entity, Def, TAG_OwnedApi_Input_Fire);
		ASSERT_THAT(IsTrue(IsAbilityActive(Entity, Handle)));
		ASSERT_THAT(IsTrue(OwnsTag(Entity, TAG_OwnedApi_State_Firing)));

		ArcAbilities::CancelAbility(*EntityManager, Entity, Handle, *AbilitySubsystem, EArcAbilityEndReason::Interrupted, Instigator);
		DrainBus();

		ASSERT_THAT(IsFalse(IsAbilityActive(Entity, Handle)));
		ASSERT_THAT(IsFalse(OwnsTag(Entity, TAG_OwnedApi_State_Firing)));

		ASSERT_THAT(AreEqual(1, BusEnded.Num()));
		ASSERT_THAT(IsTrue(BusEnded[0].Instigator == Instigator));
		ASSERT_THAT(IsTrue(BusEnded[0].Target == Entity));
		const FArcAbilityEndedPayload* Payload = BusEnded[0].Payload.GetPtr<FArcAbilityEndedPayload>();
		ASSERT_THAT(IsNotNull(Payload));
		ASSERT_THAT(IsTrue(Payload->Reason == EArcAbilityEndReason::Interrupted));
		ASSERT_THAT(IsTrue(Payload->Handle == Handle));
		ASSERT_THAT(IsTrue(Payload->Definition.Get() == Def));
	}

	TEST_METHOD(CancelAbility_CancelledWithoutInstigator_CarriesThatReasonAndTheOwnerAsInstigator)
	{
		const FMassEntityHandle Entity = ArcOwnedTagsApiTestsPrivate::CreateAbilityEntity(*EntityManager);
		UArcAbilityDefinition* Def = ArcOwnedTagsApiTestsPrivate::CreateAbilityDefinition(TAG_OwnedApi_Ability_Fire);

		const FArcAbilityHandle Handle = GrantAndActivate(Entity, Def, TAG_OwnedApi_Input_Fire);
		ASSERT_THAT(IsTrue(IsAbilityActive(Entity, Handle)));

		ArcAbilities::CancelAbility(*EntityManager, Entity, Handle, *AbilitySubsystem, EArcAbilityEndReason::Cancelled, FMassEntityHandle());
		DrainBus();

		ASSERT_THAT(AreEqual(1, BusEnded.Num()));
		ASSERT_THAT(IsTrue(BusEnded[0].Instigator == Entity, TEXT("with no instigator supplied the owner is the instigator")));
		const FArcAbilityEndedPayload* Payload = BusEnded[0].Payload.GetPtr<FArcAbilityEndedPayload>();
		ASSERT_THAT(IsNotNull(Payload));
		ASSERT_THAT(IsTrue(Payload->Reason == EArcAbilityEndReason::Cancelled));
	}

	TEST_METHOD(CancelAbility_InactiveAbility_EndsNothing)
	{
		const FMassEntityHandle Entity = ArcOwnedTagsApiTestsPrivate::CreateAbilityEntity(*EntityManager);
		UArcAbilityDefinition* Def = ArcOwnedTagsApiTestsPrivate::CreateAbilityDefinition(TAG_OwnedApi_Ability_Fire);
		const FArcAbilityHandle Handle = ArcAbilities::GrantAbility(*EntityManager, Entity, Def, TAG_OwnedApi_Input_Fire);

		ArcAbilities::CancelAbility(*EntityManager, Entity, Handle, *AbilitySubsystem, EArcAbilityEndReason::Interrupted, Entity);
		DrainBus();

		ASSERT_THAT(AreEqual(0, BusEnded.Num()));
	}

	TEST_METHOD(CancelAbilitiesWithTags_QueuesAbilityEndedForTheCancelledAbility)
	{
		const FMassEntityHandle Entity = ArcOwnedTagsApiTestsPrivate::CreateAbilityEntity(*EntityManager);

		UArcAbilityDefinition* FireDef = ArcOwnedTagsApiTestsPrivate::CreateAbilityDefinition(TAG_OwnedApi_Ability_Fire);
		FireDef->GrantTagsWhileActive.AddTag(TAG_OwnedApi_State_Firing);
		UArcAbilityDefinition* DodgeDef = ArcOwnedTagsApiTestsPrivate::CreateAbilityDefinition(TAG_OwnedApi_Ability_Dodge);
		DodgeDef->CancelAbilitiesWithTags.AddTag(TAG_OwnedApi_Ability_Fire);

		const FArcAbilityHandle FireHandle = GrantAndActivate(Entity, FireDef, TAG_OwnedApi_Input_Fire);
		ASSERT_THAT(IsTrue(IsAbilityActive(Entity, FireHandle)));
		DrainBus();
		ASSERT_THAT(AreEqual(0, BusEnded.Num()));

		const FArcAbilityHandle DodgeHandle = GrantAndActivate(Entity, DodgeDef, TAG_OwnedApi_Input_Dodge);
		DrainBus();

		ASSERT_THAT(IsTrue(IsAbilityActive(Entity, DodgeHandle)));
		ASSERT_THAT(IsFalse(IsAbilityActive(Entity, FireHandle)));
		ASSERT_THAT(IsFalse(OwnsTag(Entity, TAG_OwnedApi_State_Firing)));

		ASSERT_THAT(AreEqual(1, BusEnded.Num()));
		ASSERT_THAT(IsTrue(BusEnded[0].Target == Entity));
		const FArcAbilityEndedPayload* Payload = BusEnded[0].Payload.GetPtr<FArcAbilityEndedPayload>();
		ASSERT_THAT(IsNotNull(Payload));
		ASSERT_THAT(IsTrue(Payload->Reason == EArcAbilityEndReason::Cancelled));
		ASSERT_THAT(IsTrue(Payload->Handle == FireHandle));
		ASSERT_THAT(IsTrue(Payload->Definition.Get() == FireDef));
	}
};

// ============================================================================
// ArcEvaluateWithTags - policy-aware tag evaluation
// ============================================================================

TEST_CLASS(ArcEvaluateWithTags, "ArcMassAbilities.Aggregator.EvaluateWithTags")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogUObjectGlobals");
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogRHI");
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UMassEntitySubsystem* MES = Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
		check(MES);
		EntityManager = &MES->GetMutableEntityManager();
	}

	float EvalWithTags(const FArcAggregator& Aggregator, float BaseValue, const FGameplayTagContainer& SourceTags, const FGameplayTagContainer& TargetTags)
	{
		const FGameplayTagContainer EmptyOwnerTags;
		const FArcAggregationContext Context{FMassEntityHandle(), EmptyOwnerTags, *EntityManager, BaseValue};
		return Aggregator.EvaluateWithTags(Context, SourceTags, TargetTags);
	}

	float EvalOwned(const FArcAggregator& Aggregator, float BaseValue, const FGameplayTagContainer& OwnerTags)
	{
		const FArcAggregationContext Context{FMassEntityHandle(), OwnerTags, *EntityManager, BaseValue};
		return Aggregator.Evaluate(Context);
	}

	FGameplayTagContainer MakeTags(const FGameplayTag& Tag)
	{
		FGameplayTagContainer Tags;
		Tags.AddTag(Tag);
		return Tags;
	}

	void AddHighestAdditiveFixtureMods(FArcAggregator& Aggregator)
	{
		FArcGameplayTagRequirements FireSourceReqs;
		FireSourceReqs.RequireTags.AddTag(TAG_OwnedApi_Fire);
		FArcGameplayTagRequirements FireTargetReqs;
		FireTargetReqs.RequireTags.AddTag(TAG_OwnedApi_Fire);

		Aggregator.AddMod(EArcModifierOp::Add, 10.f, FMassEntityHandle(), nullptr, nullptr);
		Aggregator.AddMod(EArcModifierOp::Add, 30.f, FMassEntityHandle(), &FireSourceReqs, nullptr);
		Aggregator.AddMod(EArcModifierOp::Add, 20.f, FMassEntityHandle(), nullptr, &FireTargetReqs);
	}

	TEST_METHOD(WithPolicy_UsesThePolicyInsteadOfTheDefaultFormula)
	{
		FArcAggregator Aggregator;
		Aggregator.AddMod(EArcModifierOp::Add, 10.f, FMassEntityHandle(), nullptr, nullptr);
		Aggregator.AddMod(EArcModifierOp::Add, 30.f, FMassEntityHandle(), nullptr, nullptr);
		const FGameplayTagContainer NoTags;

		const float WithoutPolicy = EvalWithTags(Aggregator, 100.f, NoTags, NoTags);
		Aggregator.AggregationPolicy = FConstSharedStruct::Make(FArcTestHighestAdditivePolicy());
		const float WithPolicy = EvalWithTags(Aggregator, 100.f, NoTags, NoTags);

		ASSERT_THAT(IsNear(140.f, WithoutPolicy, 0.001f));
		ASSERT_THAT(IsNear(130.f, WithPolicy, 0.001f));
	}

	TEST_METHOD(WithPolicy_PolicyReceivesOnlyModsQualifiedByTheGivenTags)
	{
		FArcAggregator Aggregator;
		AddHighestAdditiveFixtureMods(Aggregator);
		Aggregator.AggregationPolicy = FConstSharedStruct::Make(FArcTestHighestAdditivePolicy());

		const FGameplayTagContainer NoTags;
		const FGameplayTagContainer FireTags = MakeTags(TAG_OwnedApi_Fire);

		ASSERT_THAT(IsNear(110.f, EvalWithTags(Aggregator, 100.f, NoTags, NoTags), 0.001f));
		ASSERT_THAT(IsNear(130.f, EvalWithTags(Aggregator, 100.f, FireTags, NoTags), 0.001f));
		ASSERT_THAT(IsNear(120.f, EvalWithTags(Aggregator, 100.f, NoTags, FireTags), 0.001f));
	}

	TEST_METHOD(WithPolicy_DoesNotChangeStoredQualification)
	{
		FArcAggregator Aggregator;
		AddHighestAdditiveFixtureMods(Aggregator);
		Aggregator.AggregationPolicy = FConstSharedStruct::Make(FArcTestHighestAdditivePolicy());
		const FArcAggregatorMod& TaggedMod = Aggregator.Mods[static_cast<uint8>(EArcModifierOp::Add)][1];
		TaggedMod.bQualified = false;

		const FGameplayTagContainer NoTags;
		const FGameplayTagContainer FireTags = MakeTags(TAG_OwnedApi_Fire);
		EvalWithTags(Aggregator, 100.f, FireTags, NoTags);

		ASSERT_THAT(IsFalse(TaggedMod.bQualified));
	}

	TEST_METHOD(WithPolicy_PolicyCachedFromHandlerConfigIsUsed)
	{
		UArcAttributeHandlerConfig* Config = NewObject<UArcAttributeHandlerConfig>();
		const FArcAttributeRef Health = FArcAttributeSet_TestStats::GetHealthAttribute();
		Config->AggregationPolicies.Add(Health, FInstancedStruct::Make(FArcTestHighestAdditivePolicy()));

		FArcAggregator Aggregator;
		AddHighestAdditiveFixtureMods(Aggregator);
		ASSERT_THAT(IsFalse(Aggregator.AggregationPolicy.IsValid()));

		ArcAttributes::EnsureAggregationPolicy(Aggregator, Health, Config);
		ASSERT_THAT(IsTrue(Aggregator.AggregationPolicy.IsValid()));

		const FGameplayTagContainer NoTags;
		const FGameplayTagContainer FireTags = MakeTags(TAG_OwnedApi_Fire);
		ASSERT_THAT(IsNear(130.f, EvalWithTags(Aggregator, 100.f, FireTags, NoTags), 0.001f));
	}

	TEST_METHOD(NoPolicy_FastPathMatchesEvaluateAcrossChannels)
	{
		FArcGameplayTagRequirements FireTargetReqs;
		FireTargetReqs.RequireTags.AddTag(TAG_OwnedApi_Fire);

		FArcAggregator Aggregator;
		Aggregator.AddMod(EArcModifierOp::Add, 10.f, FMassEntityHandle(), nullptr, nullptr);
		Aggregator.AddMod(EArcModifierOp::Add, 5.f, FMassEntityHandle(), nullptr, &FireTargetReqs);
		Aggregator.AddMod(EArcModifierOp::MultiplyAdditive, 0.5f, FMassEntityHandle(), nullptr, nullptr);
		Aggregator.AddMod(EArcModifierOp::Add, 7.f, FMassEntityHandle(), nullptr, nullptr, static_cast<uint8>(1));
		Aggregator.AddMod(EArcModifierOp::MultiplyCompound, 2.f, FMassEntityHandle(), nullptr, nullptr, static_cast<uint8>(1));
		ASSERT_THAT(IsFalse(Aggregator.AggregationPolicy.IsValid()));

		const FGameplayTagContainer NoTags;
		const FGameplayTagContainer FireTags = MakeTags(TAG_OwnedApi_Fire);

		const float EvaluateWithFire = EvalOwned(Aggregator, 100.f, FireTags);
		const float WithTagsFire = EvalWithTags(Aggregator, 100.f, NoTags, FireTags);
		const float EvaluateWithoutFire = EvalOwned(Aggregator, 100.f, NoTags);
		const float WithTagsNoFire = EvalWithTags(Aggregator, 100.f, NoTags, NoTags);

		ASSERT_THAT(IsNear(359.f, EvaluateWithFire, 0.001f));
		ASSERT_THAT(IsNear(EvaluateWithFire, WithTagsFire, 0.001f));
		ASSERT_THAT(IsNear(344.f, EvaluateWithoutFire, 0.001f));
		ASSERT_THAT(IsNear(EvaluateWithoutFire, WithTagsNoFire, 0.001f));
	}

	TEST_METHOD(NoPolicy_NoMods_ReturnsBaseValue)
	{
		const FArcAggregator Aggregator;
		const FGameplayTagContainer NoTags;

		ASSERT_THAT(IsNear(100.f, EvalWithTags(Aggregator, 100.f, NoTags, NoTags), 0.001f));
	}
};
