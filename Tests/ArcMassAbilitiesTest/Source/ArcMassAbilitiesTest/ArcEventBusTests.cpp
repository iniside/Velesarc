// Copyright Lukasz Baran. All Rights Reserved.
//
// CQTest coverage for the one event bus (UArcGameEventSubsystem): the channels AMA publishes on it, their
// envelope tags and payloads, the (entity, tag) and (entity, attribute) subscriptions, attribute coalescing and
// chain order. Plan: docs/plans/2026-10-10-1355-arc-mechanics-plan7-event-bus-plan.md (Step 7).
//
// Topology: standalone world (FActorTestSpawner) with a real FMassEntityManager. The bus has no replication
// surface; every event is queued and drained on the game thread through the production entry point
// DrainOnPostActorTick. Events are published by the production code paths (input processor, cooldown processor,
// pending-ops processor, owned-tag API); only chain tests use hand-written subscribers.

#include "CQTest.h"
#include "ArcAbilityCostTestTypes.h"
#include "ArcMassAbilitiesTestTypes.h"
#include "Components/ActorTestSpawner.h"
#include "Abilities/ArcAbilityCooldown.h"
#include "Abilities/ArcAbilityDefinition.h"
#include "Abilities/ArcAbilityFunctions.h"
#include "Abilities/ArcAbilityHandle.h"
#include "Effects/ArcEffectComponent.h"
#include "Effects/ArcEffectDefinition.h"
#include "Effects/ArcEffectFunctions.h"
#include "Effects/ArcEffectTask.h"
#include "Events/ArcAbilityEventPayloads.h"
#include "Events/ArcAttributeEventPayloads.h"
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
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "Mass/ExecutionContext.h"
#include "Mass/Executor.h"
#include "Mass/ProcessingContext.h"
#include "MassEntitySubsystem.h"
#include "MassSignalSubsystem.h"
#include "NativeGameplayTags.h"
#include "Processors/ArcAbilityInputProcessor.h"
#include "Processors/ArcCooldownProcessor.h"
#include "Processors/ArcPendingAttributeOpsProcessor.h"
#include "Signals/ArcAbilitySignals.h"
#include "StructUtils/InstancedStruct.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Bus_AbilitySpell, "Test.EventBus.Ability.Spell");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Bus_Owned, "Test.EventBus.Owned");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Bus_Origin, "Test.EventBus.Origin");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Bus_Input, "Test.EventBus.Input.A");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Bus_TagA, "Test.EventBus.Tag.A");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Bus_TagB, "Test.EventBus.Tag.B");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Bus_TagParent, "Test.EventBus.Tag.Parent");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Bus_TagChild, "Test.EventBus.Tag.Parent.Child");

namespace ArcEventBusTestsPrivate
{
	struct FBusFixture
	{
		FActorTestSpawner Spawner;
		FMassEntityManager* EntityManager = nullptr;
		UMassSignalSubsystem* SignalSubsystem = nullptr;
		UArcGameEventSubsystem* Bus = nullptr;
		UArcPendingAttributeOpsProcessor* PendingProcessor = nullptr;
		TArray<FArcGameEvent> Events;

		void Init()
		{
			FAutomationTestBase::bSuppressLogWarnings = true;
			FAutomationTestBase::SuppressedLogCategories.AddUnique("LogUObjectGlobals");
			FAutomationTestBase::SuppressedLogCategories.AddUnique("LogRHI");
			FAutomationTestBase::SuppressedLogCategories.AddUnique("LogMass");
			Events.Reset();
			Spawner.GetWorld();
			Spawner.InitializeGameSubsystems();
			UMassEntitySubsystem* EntitySubsystem = Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
			check(EntitySubsystem);
			EntityManager = &EntitySubsystem->GetMutableEntityManager();
			SignalSubsystem = Spawner.GetWorld().GetSubsystem<UMassSignalSubsystem>();
			check(SignalSubsystem);
			Bus = Spawner.GetWorld().GetSubsystem<UArcGameEventSubsystem>();
			check(Bus);

			PendingProcessor = NewObject<UArcPendingAttributeOpsProcessor>();
			PendingProcessor->CallInitialize(EntityManager->GetOwner(), EntityManager->AsShared());
		}

		FMassEntityHandle CreateEntity(float Health = 100.f)
		{
			FArcAttributeSet_TestStats Stats;
			Stats.Health.BaseValue = Health;
			Stats.Health.CurrentValue = Health;

			TArray<FInstancedStruct> Instances;
			Instances.Add(FInstancedStruct::Make(Stats));
			Instances.Add(FInstancedStruct::Make(FArcAbilityCollectionFragment()));
			Instances.Add(FInstancedStruct::Make(FArcAbilityInputFragment()));
			Instances.Add(FInstancedStruct::Make(FArcOwnedTagsFragment()));
			Instances.Add(FInstancedStruct::Make(FArcAbilityCooldownFragment()));
			Instances.Add(FInstancedStruct::Make(FArcEffectStackFragment()));
			Instances.Add(FInstancedStruct::Make(FArcAggregatorFragment()));
			Instances.Add(FInstancedStruct::Make(FArcPendingAttributeOpsFragment()));
			return ArcMassAbilitiesTestHelpers::CreateEntityFromInstances(*EntityManager, Instances);
		}

		void Drain()
		{
			Bus->DrainOnPostActorTick(&Spawner.GetWorld(), LEVELTICK_All, 0.f);
		}

		void RecordGlobal(FGameplayTag Tag)
		{
			TArray<FArcGameEvent>* Out = &Events;
			Bus->Subscribe(Tag, FMassEntityHandle(), EArcEventRole::Any,
				FArcGameEventDelegate::CreateLambda([Out](const FArcGameEvent& Event) { Out->Add(Event); }));
		}

		int32 Count(FGameplayTag Tag) const
		{
			int32 Result = 0;
			for (const FArcGameEvent& Event : Events)
			{
				if (Event.Tag == Tag)
				{
					++Result;
				}
			}
			return Result;
		}

		const FArcGameEvent* Find(FGameplayTag Tag, int32 Occurrence = 0) const
		{
			int32 Seen = 0;
			for (const FArcGameEvent& Event : Events)
			{
				if (Event.Tag == Tag)
				{
					if (Seen == Occurrence)
					{
						return &Event;
					}
					++Seen;
				}
			}
			return nullptr;
		}

		void RunPendingOps(FMassEntityHandle Entity)
		{
			SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::PendingAttributeOps, Entity);
			FMassProcessingContext ProcessingContext(*EntityManager, 0.f);
			UE::Mass::Executor::Run(*PendingProcessor, ProcessingContext);
		}

		void ApplyAttributeDelta(FMassEntityHandle Entity, const FArcAttributeRef& Attribute, float Delta)
		{
			UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
			Effect->StackingPolicy.DurationType = EArcEffectDuration::Instant;
			FArcTestAddExecution Exec;
			Exec.TargetAttribute = Attribute;
			Exec.Value = Delta;
			Effect->Executions.Add(ArcMassAbilitiesTestHelpers::MakeExecutionEntry(FInstancedStruct::Make(Exec)));
			ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());
			RunPendingOps(Entity);
		}

		void RunInputProcessor(FMassEntityHandle Entity)
		{
			UArcAbilityInputProcessor* Processor = NewObject<UArcAbilityInputProcessor>();
			Processor->CallInitialize(&Spawner.GetWorld(), EntityManager->AsShared());
			SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::AbilityInputReceived, Entity);
			UE::Mass::FProcessingContext ProcessingContext(*EntityManager, 0.016f);
			Processor->CallExecute(*EntityManager, ProcessingContext.GetExecutionContext());
		}

		void RunCooldownProcessor(float DeltaTime)
		{
			UArcCooldownProcessor* Processor = NewObject<UArcCooldownProcessor>();
			Processor->CallInitialize(&Spawner.GetWorld(), EntityManager->AsShared());
			UE::Mass::FProcessingContext ProcessingContext(*EntityManager, DeltaTime);
			Processor->CallExecute(*EntityManager, ProcessingContext.GetExecutionContext());
		}

		UArcAbilityDefinition* MakeSpellDefinition() const
		{
			UArcAbilityDefinition* Definition = NewObject<UArcAbilityDefinition>();
			Definition->AbilityTags.AddTag(TAG_Bus_AbilitySpell);
			return Definition;
		}

		void AddTag(FMassEntityHandle Entity, const FGameplayTag& Tag)
		{
			ArcAbilities::AddOwnedTags(*EntityManager, Entity, FGameplayTagContainer(Tag));
		}

		void RemoveTag(FMassEntityHandle Entity, const FGameplayTag& Tag)
		{
			ArcAbilities::RemoveOwnedTags(*EntityManager, Entity, FGameplayTagContainer(Tag));
		}
	};
}

// ============================================================================
// ArcEventBus_Channels - each AMA channel with its envelope tags and payload
// ============================================================================

TEST_CLASS(ArcEventBus_Channels, "ArcMassAbilities.EventBus.Channels")
{
	ArcEventBusTestsPrivate::FBusFixture Fix;

	BEFORE_EACH()
	{
		Fix.Init();
	}

	TEST_METHOD(Activated_PayloadHasHandleAndDefinition_SourceTagsCarryAbilityOriginAndOwnedTags)
	{
		const FMassEntityHandle Entity = Fix.CreateEntity();
		UArcAbilityDefinition* Definition = Fix.MakeSpellDefinition();
		Fix.AddTag(Entity, TAG_Bus_Owned);
		const FArcAbilityHandle Handle = ArcAbilities::GrantAbility(*Fix.EntityManager, Entity, Definition, TAG_Bus_Input);
		Fix.RecordGlobal(ArcGameEventTags::AbilityActivated.GetTag());

		ArcAbilities::PushInput(*Fix.EntityManager, Entity, TAG_Bus_Input, FGameplayTagContainer(TAG_Bus_Origin));
		Fix.RunInputProcessor(Entity);
		ASSERT_THAT(AreEqual(0, Fix.Events.Num(), TEXT("nothing is delivered before the drain")));
		Fix.Drain();

		ASSERT_THAT(AreEqual(1, Fix.Count(ArcGameEventTags::AbilityActivated.GetTag())));
		const FArcGameEvent* Event = Fix.Find(ArcGameEventTags::AbilityActivated.GetTag());
		ASSERT_THAT(IsNotNull(Event));
		ASSERT_THAT(IsTrue(Event->Instigator == Entity));
		ASSERT_THAT(IsTrue(Event->Target == Entity));
		ASSERT_THAT(IsNear(0.f, Event->Magnitude, 0.001f));
		ASSERT_THAT(IsTrue(Event->SourceTags.HasTagExact(TAG_Bus_AbilitySpell), TEXT("the ability's tags are on the source side")));
		ASSERT_THAT(IsTrue(Event->SourceTags.HasTagExact(TAG_Bus_Origin), TEXT("the activation's origin tags are on the source side")));
		ASSERT_THAT(IsTrue(Event->SourceTags.HasTagExact(TAG_Bus_Owned), TEXT("the caster's owned tags are on the source side")));
		ASSERT_THAT(IsFalse(Event->TargetTags.HasTagExact(TAG_Bus_AbilitySpell), TEXT("the ability's tags never reach the target side")));

		const FArcAbilityActivatedPayload* Payload = Event->Payload.GetPtr<FArcAbilityActivatedPayload>();
		ASSERT_THAT(IsNotNull(Payload));
		ASSERT_THAT(IsTrue(Payload->Handle == Handle));
		ASSERT_THAT(IsTrue(Payload->Definition.Get() == Definition));
	}

	TEST_METHOD(CostFailed_MagnitudeAndPayloadCarryTheMissingAmount_AndTheAbilityDoesNotActivate)
	{
		const FMassEntityHandle Entity = Fix.CreateEntity(100.f);
		UArcAbilityDefinition* Definition = Fix.MakeSpellDefinition();
		FArcTestAttributeCost Cost;
		Cost.Attribute = FArcAttributeSet_TestStats::GetHealthAttribute();
		Cost.Amount = 150.f;
		Definition->Cost = FInstancedStruct::Make(Cost);
		const FArcAbilityHandle Handle = ArcAbilities::GrantAbility(*Fix.EntityManager, Entity, Definition, TAG_Bus_Input);
		Fix.RecordGlobal(ArcGameEventTags::AbilityCostFailed.GetTag());
		Fix.RecordGlobal(ArcGameEventTags::AbilityActivated.GetTag());

		ArcAbilities::PushInput(*Fix.EntityManager, Entity, TAG_Bus_Input);
		Fix.RunInputProcessor(Entity);
		Fix.Drain();

		ASSERT_THAT(AreEqual(1, Fix.Count(ArcGameEventTags::AbilityCostFailed.GetTag())));
		ASSERT_THAT(AreEqual(0, Fix.Count(ArcGameEventTags::AbilityActivated.GetTag())));
		ASSERT_THAT(IsFalse(ArcAbilities::IsAbilityActive(*Fix.EntityManager, Entity, Handle)));

		const FArcGameEvent* Event = Fix.Find(ArcGameEventTags::AbilityCostFailed.GetTag());
		ASSERT_THAT(IsNotNull(Event));
		ASSERT_THAT(IsTrue(Event->Instigator == Entity));
		ASSERT_THAT(IsTrue(Event->Target == Entity));
		ASSERT_THAT(IsNear(50.f, Event->Magnitude, 0.001f));
		ASSERT_THAT(IsTrue(Event->SourceTags.HasTagExact(TAG_Bus_AbilitySpell)));

		const FArcAbilityCostFailedPayload* Payload = Event->Payload.GetPtr<FArcAbilityCostFailedPayload>();
		ASSERT_THAT(IsNotNull(Payload));
		ASSERT_THAT(IsNear(50.f, Payload->MissingAmount, 0.001f));
		ASSERT_THAT(IsTrue(Payload->Reason == EArcCostFailReason::Insufficient));
		ASSERT_THAT(IsTrue(Payload->Definition.Get() == Definition));
	}

	TEST_METHOD(CooldownFailed_MagnitudeAndPayloadCarryTheRemainingSeconds)
	{
		const FMassEntityHandle Entity = Fix.CreateEntity();
		UArcAbilityDefinition* Definition = Fix.MakeSpellDefinition();
		Definition->Cooldown = FInstancedStruct::Make(FArcAbilityCooldown_Duration());
		const FArcAbilityHandle Handle = ArcAbilities::GrantAbility(*Fix.EntityManager, Entity, Definition, TAG_Bus_Input);

		FArcAbilityCooldownFragment& Cooldowns = FMassEntityView(*Fix.EntityManager, Entity).GetFragmentData<FArcAbilityCooldownFragment>();
		FArcCooldownEntry& Entry = Cooldowns.ActiveCooldowns.AddDefaulted_GetRef();
		Entry.AbilityHandle = Handle;
		Entry.RemainingTime = 4.f;
		Entry.MaxCooldownTime = 5.f;
		Entry.bBlocking = true;
		Fix.RecordGlobal(ArcGameEventTags::AbilityCooldownFailed.GetTag());

		ArcAbilities::PushInput(*Fix.EntityManager, Entity, TAG_Bus_Input);
		Fix.RunInputProcessor(Entity);
		Fix.Drain();

		ASSERT_THAT(AreEqual(1, Fix.Count(ArcGameEventTags::AbilityCooldownFailed.GetTag())));
		ASSERT_THAT(IsFalse(ArcAbilities::IsAbilityActive(*Fix.EntityManager, Entity, Handle)));

		const FArcGameEvent* Event = Fix.Find(ArcGameEventTags::AbilityCooldownFailed.GetTag());
		ASSERT_THAT(IsNotNull(Event));
		ASSERT_THAT(IsTrue(Event->Instigator == Entity));
		ASSERT_THAT(IsTrue(Event->Target == Entity));
		ASSERT_THAT(IsNear(4.f, Event->Magnitude, 0.001f));
		ASSERT_THAT(IsTrue(Event->SourceTags.HasTagExact(TAG_Bus_AbilitySpell)));

		const FArcAbilityCooldownFailedPayload* Payload = Event->Payload.GetPtr<FArcAbilityCooldownFailedPayload>();
		ASSERT_THAT(IsNotNull(Payload));
		ASSERT_THAT(IsNear(4.f, Payload->RemainingSeconds, 0.001f));
		ASSERT_THAT(IsTrue(Payload->Definition.Get() == Definition));
	}

	TEST_METHOD(CooldownExpired_PayloadHasHandleAndDefinition_SourceTagsCarryTheAbilityTags)
	{
		const FMassEntityHandle Entity = Fix.CreateEntity();
		UArcAbilityDefinition* Definition = Fix.MakeSpellDefinition();
		Definition->Cooldown = FInstancedStruct::Make(FArcAbilityCooldown_Duration());
		const FArcAbilityHandle Handle = ArcAbilities::GrantAbility(*Fix.EntityManager, Entity, Definition, TAG_Bus_Input);

		FArcAbilityCooldownFragment& Cooldowns = FMassEntityView(*Fix.EntityManager, Entity).GetFragmentData<FArcAbilityCooldownFragment>();
		FArcCooldownEntry& Entry = Cooldowns.ActiveCooldowns.AddDefaulted_GetRef();
		Entry.AbilityHandle = Handle;
		Entry.RemainingTime = 0.1f;
		Entry.MaxCooldownTime = 5.f;
		Entry.bBlocking = true;
		Fix.RecordGlobal(ArcGameEventTags::AbilityCooldownExpired.GetTag());

		Fix.RunCooldownProcessor(1.f);
		ASSERT_THAT(AreEqual(0, Fix.Events.Num(), TEXT("nothing is delivered before the drain")));
		Fix.Drain();

		ASSERT_THAT(AreEqual(1, Fix.Count(ArcGameEventTags::AbilityCooldownExpired.GetTag())));
		const FArcGameEvent* Event = Fix.Find(ArcGameEventTags::AbilityCooldownExpired.GetTag());
		ASSERT_THAT(IsNotNull(Event));
		ASSERT_THAT(IsTrue(Event->Instigator == Entity));
		ASSERT_THAT(IsTrue(Event->Target == Entity));
		ASSERT_THAT(IsTrue(Event->SourceTags.HasTagExact(TAG_Bus_AbilitySpell)));

		const FArcAbilityCooldownExpiredPayload* Payload = Event->Payload.GetPtr<FArcAbilityCooldownExpiredPayload>();
		ASSERT_THAT(IsNotNull(Payload));
		ASSERT_THAT(IsTrue(Payload->Handle == Handle));
		ASSERT_THAT(IsTrue(Payload->Definition.Get() == Definition));
	}

	TEST_METHOD(TagAddedAndRemoved_FireOnlyOnEdges_EnvelopeShowsTheTagsAfterTheChange)
	{
		const FMassEntityHandle Entity = Fix.CreateEntity();
		Fix.RecordGlobal(ArcGameEventTags::TagAdded.GetTag());
		Fix.RecordGlobal(ArcGameEventTags::TagRemoved.GetTag());

		Fix.AddTag(Entity, TAG_Bus_TagA);
		Fix.AddTag(Entity, TAG_Bus_TagA);
		Fix.RemoveTag(Entity, TAG_Bus_TagA);
		Fix.Drain();

		ASSERT_THAT(AreEqual(1, Fix.Count(ArcGameEventTags::TagAdded.GetTag()), TEXT("only the 0 -> 1 step is an Added edge")));
		ASSERT_THAT(AreEqual(0, Fix.Count(ArcGameEventTags::TagRemoved.GetTag()), TEXT("2 -> 1 is not a Removed edge")));

		const FArcGameEvent* Added = Fix.Find(ArcGameEventTags::TagAdded.GetTag());
		ASSERT_THAT(IsNotNull(Added));
		ASSERT_THAT(IsTrue(Added->Instigator == Entity));
		ASSERT_THAT(IsTrue(Added->Target == Entity));
		ASSERT_THAT(IsTrue(Added->SourceTags.HasTagExact(TAG_Bus_TagA)));
		ASSERT_THAT(IsTrue(Added->TargetTags.HasTagExact(TAG_Bus_TagA)));
		const FArcTagChangedPayload* AddedPayload = Added->Payload.GetPtr<FArcTagChangedPayload>();
		ASSERT_THAT(IsNotNull(AddedPayload));
		ASSERT_THAT(IsTrue(AddedPayload->Tag == TAG_Bus_TagA));
		ASSERT_THAT(AreEqual(0, AddedPayload->OldCount));
		ASSERT_THAT(AreEqual(1, AddedPayload->NewCount));

		Fix.RemoveTag(Entity, TAG_Bus_TagA);
		Fix.Drain();

		ASSERT_THAT(AreEqual(1, Fix.Count(ArcGameEventTags::TagRemoved.GetTag())));
		const FArcGameEvent* Removed = Fix.Find(ArcGameEventTags::TagRemoved.GetTag());
		ASSERT_THAT(IsNotNull(Removed));
		ASSERT_THAT(IsFalse(Removed->SourceTags.HasTagExact(TAG_Bus_TagA), TEXT("the envelope shows the tags after the change")));
		const FArcTagChangedPayload* RemovedPayload = Removed->Payload.GetPtr<FArcTagChangedPayload>();
		ASSERT_THAT(IsNotNull(RemovedPayload));
		ASSERT_THAT(AreEqual(1, RemovedPayload->OldCount));
		ASSERT_THAT(AreEqual(0, RemovedPayload->NewCount));
	}

	TEST_METHOD(AttributeChanged_PayloadHasOldAndNewValue_EnvelopeCarriesOwnedTags)
	{
		const FMassEntityHandle Entity = Fix.CreateEntity(100.f);
		Fix.AddTag(Entity, TAG_Bus_Owned);
		Fix.RecordGlobal(ArcGameEventTags::AttributeChanged.GetTag());

		Fix.ApplyAttributeDelta(Entity, FArcAttributeSet_TestStats::GetHealthAttribute(), -10.f);
		ASSERT_THAT(AreEqual(0, Fix.Events.Num(), TEXT("nothing is delivered before the drain")));
		Fix.Drain();

		ASSERT_THAT(AreEqual(1, Fix.Count(ArcGameEventTags::AttributeChanged.GetTag())));
		const FArcGameEvent* Event = Fix.Find(ArcGameEventTags::AttributeChanged.GetTag());
		ASSERT_THAT(IsNotNull(Event));
		ASSERT_THAT(IsTrue(Event->Instigator == Entity));
		ASSERT_THAT(IsTrue(Event->Target == Entity));
		ASSERT_THAT(IsNear(-10.f, Event->Magnitude, 0.001f));
		ASSERT_THAT(IsTrue(Event->SourceTags.HasTagExact(TAG_Bus_Owned)));
		ASSERT_THAT(IsTrue(Event->TargetTags.HasTagExact(TAG_Bus_Owned)));

		const FArcAttributeChangedPayload* Payload = Event->Payload.GetPtr<FArcAttributeChangedPayload>();
		ASSERT_THAT(IsNotNull(Payload));
		ASSERT_THAT(IsTrue(Payload->Attribute == FArcAttributeSet_TestStats::GetHealthAttribute()));
		ASSERT_THAT(IsNear(100.f, Payload->OldValue, 0.001f));
		ASSERT_THAT(IsNear(90.f, Payload->NewValue, 0.001f));
	}

	TEST_METHOD(ExactMatch_RealChannel_ParentAndSiblingChannelsReceiveNothing)
	{
		const FMassEntityHandle Entity = Fix.CreateEntity();
		const FGameplayTag Exact = ArcGameEventTags::TagAdded.GetTag();
		const FGameplayTag Parent = Exact.RequestDirectParent();
		const FGameplayTag Sibling = ArcGameEventTags::TagRemoved.GetTag();
		ASSERT_THAT(IsTrue(Parent.IsValid()));

		int32 ExactGlobal = 0;
		int32 ParentGlobal = 0;
		int32 SiblingGlobal = 0;
		int32 ParentOnEntity = 0;
		int32 SiblingOnEntity = 0;
		Fix.Bus->Subscribe(Exact, FMassEntityHandle(), EArcEventRole::Any, FArcGameEventDelegate::CreateLambda([&ExactGlobal](const FArcGameEvent&) { ++ExactGlobal; }));
		Fix.Bus->Subscribe(Parent, FMassEntityHandle(), EArcEventRole::Any, FArcGameEventDelegate::CreateLambda([&ParentGlobal](const FArcGameEvent&) { ++ParentGlobal; }));
		Fix.Bus->Subscribe(Sibling, FMassEntityHandle(), EArcEventRole::Any, FArcGameEventDelegate::CreateLambda([&SiblingGlobal](const FArcGameEvent&) { ++SiblingGlobal; }));
		Fix.Bus->Subscribe(Parent, Entity, EArcEventRole::Any, FArcGameEventDelegate::CreateLambda([&ParentOnEntity](const FArcGameEvent&) { ++ParentOnEntity; }));
		Fix.Bus->Subscribe(Sibling, Entity, EArcEventRole::Any, FArcGameEventDelegate::CreateLambda([&SiblingOnEntity](const FArcGameEvent&) { ++SiblingOnEntity; }));

		Fix.AddTag(Entity, TAG_Bus_TagA);
		Fix.Drain();

		ASSERT_THAT(AreEqual(1, ExactGlobal));
		ASSERT_THAT(AreEqual(0, ParentGlobal));
		ASSERT_THAT(AreEqual(0, SiblingGlobal));
		ASSERT_THAT(AreEqual(0, ParentOnEntity));
		ASSERT_THAT(AreEqual(0, SiblingOnEntity));
	}
};

// ============================================================================
// ArcEventBus_TagSubscription - Subscribe(Tag.Added|Removed, Entity, ChangedTag)
// ============================================================================

TEST_CLASS(ArcEventBus_TagSubscription, "ArcMassAbilities.EventBus.TagSubscription")
{
	ArcEventBusTestsPrivate::FBusFixture Fix;
	FMassEntityHandle Entity;

	BEFORE_EACH()
	{
		Fix.Init();
		Entity = Fix.CreateEntity();
	}

	FDelegateHandle SubscribeCounter(const FGameplayTag& Channel, FMassEntityHandle On, const FGameplayTag& ChangedTag, int32& OutCount)
	{
		return Fix.Bus->Subscribe(Channel, On, ChangedTag,
			FArcGameEventDelegate::CreateLambda([&OutCount](const FArcGameEvent&) { ++OutCount; }));
	}

	TEST_METHOD(ChangeOfAnotherTag_CallsNoDelegate_TheSubscribedTagCallsItOnce)
	{
		int32 Calls = 0;
		SubscribeCounter(ArcGameEventTags::TagAdded.GetTag(), Entity, TAG_Bus_TagA, Calls);

		Fix.AddTag(Entity, TAG_Bus_TagB);
		Fix.Drain();
		ASSERT_THAT(AreEqual(0, Calls, TEXT("another tag's edge must not call the delegate")));

		Fix.AddTag(Entity, TAG_Bus_TagA);
		Fix.Drain();
		ASSERT_THAT(AreEqual(1, Calls));
	}

	TEST_METHOD(AddedSubscription_DoesNotHearTheRemovalAndTheRemovedSubscriptionDoesNotHearTheGain)
	{
		int32 AddedCalls = 0;
		int32 RemovedCalls = 0;
		SubscribeCounter(ArcGameEventTags::TagAdded.GetTag(), Entity, TAG_Bus_TagA, AddedCalls);
		SubscribeCounter(ArcGameEventTags::TagRemoved.GetTag(), Entity, TAG_Bus_TagA, RemovedCalls);

		Fix.AddTag(Entity, TAG_Bus_TagA);
		Fix.Drain();
		ASSERT_THAT(AreEqual(1, AddedCalls));
		ASSERT_THAT(AreEqual(0, RemovedCalls));

		Fix.RemoveTag(Entity, TAG_Bus_TagA);
		Fix.Drain();
		ASSERT_THAT(AreEqual(1, AddedCalls));
		ASSERT_THAT(AreEqual(1, RemovedCalls));
	}

	TEST_METHOD(ParentAndChildTags_AreNotTheSubscribedTag)
	{
		int32 ParentCalls = 0;
		int32 ChildCalls = 0;
		SubscribeCounter(ArcGameEventTags::TagAdded.GetTag(), Entity, TAG_Bus_TagParent, ParentCalls);
		SubscribeCounter(ArcGameEventTags::TagAdded.GetTag(), Entity, TAG_Bus_TagChild, ChildCalls);

		Fix.AddTag(Entity, TAG_Bus_TagChild);
		Fix.Drain();
		ASSERT_THAT(AreEqual(0, ParentCalls, TEXT("a child tag's edge must not call the parent tag's delegate")));
		ASSERT_THAT(AreEqual(1, ChildCalls));

		Fix.AddTag(Entity, TAG_Bus_TagParent);
		Fix.Drain();
		ASSERT_THAT(AreEqual(1, ParentCalls));
		ASSERT_THAT(AreEqual(1, ChildCalls, TEXT("a parent tag's edge must not call the child tag's delegate")));
	}

	TEST_METHOD(AnotherEntitysEdge_CallsNoDelegate)
	{
		const FMassEntityHandle OtherEntity = Fix.CreateEntity();
		int32 Calls = 0;
		SubscribeCounter(ArcGameEventTags::TagAdded.GetTag(), Entity, TAG_Bus_TagA, Calls);

		Fix.AddTag(OtherEntity, TAG_Bus_TagA);
		Fix.Drain();
		ASSERT_THAT(AreEqual(0, Calls));

		Fix.AddTag(Entity, TAG_Bus_TagA);
		Fix.Drain();
		ASSERT_THAT(AreEqual(1, Calls));
	}

	TEST_METHOD(UnsubscribedHandle_StopsDelivery)
	{
		int32 Calls = 0;
		const FDelegateHandle Handle = SubscribeCounter(ArcGameEventTags::TagAdded.GetTag(), Entity, TAG_Bus_TagA, Calls);

		Fix.Bus->Unsubscribe(Handle);
		Fix.AddTag(Entity, TAG_Bus_TagA);
		Fix.Drain();
		ASSERT_THAT(AreEqual(0, Calls));
	}
};

// ============================================================================
// ArcEventBus_Coalescing - Attribute.Changed is one event per (entity, attribute) per drain
// ============================================================================

TEST_CLASS(ArcEventBus_Coalescing, "ArcMassAbilities.EventBus.Coalescing")
{
	ArcEventBusTestsPrivate::FBusFixture Fix;
	FMassEntityHandle Entity;

	BEFORE_EACH()
	{
		Fix.Init();
		Entity = Fix.CreateEntity(100.f);
	}

	TEST_METHOD(HealthChangedTwiceInOneFrame_DeliversOneEventFirstOldLastNew)
	{
		TArray<FArcGameEvent> Keyed;
		Fix.Bus->Subscribe(Entity, FArcAttributeSet_TestStats::GetHealthAttribute(),
			FArcGameEventDelegate::CreateLambda([&Keyed](const FArcGameEvent& Event) { Keyed.Add(Event); }));
		Fix.RecordGlobal(ArcGameEventTags::AttributeChanged.GetTag());

		Fix.ApplyAttributeDelta(Entity, FArcAttributeSet_TestStats::GetHealthAttribute(), -10.f);
		Fix.AddTag(Entity, TAG_Bus_Owned);
		Fix.ApplyAttributeDelta(Entity, FArcAttributeSet_TestStats::GetHealthAttribute(), -5.f);
		Fix.Drain();

		ASSERT_THAT(AreEqual(1, Keyed.Num(), TEXT("100 -> 90 -> 85 in one frame is one keyed event")));
		ASSERT_THAT(IsTrue(Keyed[0].SourceTags.HasTagExact(TAG_Bus_Owned), TEXT("the coalesced event carries the tags of the last change")));
		ASSERT_THAT(IsTrue(Keyed[0].TargetTags.HasTagExact(TAG_Bus_Owned)));
		ASSERT_THAT(AreEqual(1, Fix.Count(ArcGameEventTags::AttributeChanged.GetTag()), TEXT("and one event on the plain channel")));
		const FArcAttributeChangedPayload* Payload = Keyed[0].Payload.GetPtr<FArcAttributeChangedPayload>();
		ASSERT_THAT(IsNotNull(Payload));
		ASSERT_THAT(IsNear(100.f, Payload->OldValue, 0.001f));
		ASSERT_THAT(IsNear(85.f, Payload->NewValue, 0.001f));
		ASSERT_THAT(IsNear(-15.f, Keyed[0].Magnitude, 0.001f));
	}

	TEST_METHOD(CoalescedEvent_KeepsTheQueuePositionOfTheFirstChange)
	{
		Fix.RecordGlobal(ArcGameEventTags::AttributeChanged.GetTag());
		Fix.RecordGlobal(ArcGameEventTags::TagAdded.GetTag());

		Fix.ApplyAttributeDelta(Entity, FArcAttributeSet_TestStats::GetHealthAttribute(), -10.f);
		Fix.AddTag(Entity, TAG_Bus_TagA);
		Fix.ApplyAttributeDelta(Entity, FArcAttributeSet_TestStats::GetHealthAttribute(), -5.f);
		Fix.Drain();

		ASSERT_THAT(AreEqual(2, Fix.Events.Num()));
		ASSERT_THAT(IsTrue(Fix.Events[0].Tag == ArcGameEventTags::AttributeChanged.GetTag(), TEXT("the merged change sits where the first change was queued")));
		ASSERT_THAT(IsTrue(Fix.Events[1].Tag == ArcGameEventTags::TagAdded.GetTag()));
		const FArcAttributeChangedPayload* Payload = Fix.Events[0].Payload.GetPtr<FArcAttributeChangedPayload>();
		ASSERT_THAT(IsNotNull(Payload));
		ASSERT_THAT(IsNear(85.f, Payload->NewValue, 0.001f));
	}

	TEST_METHOD(ChangeAfterTheDrain_IsAFreshEventStartingFromTheDeliveredValue)
	{
		TArray<FArcGameEvent> Keyed;
		Fix.Bus->Subscribe(Entity, FArcAttributeSet_TestStats::GetHealthAttribute(),
			FArcGameEventDelegate::CreateLambda([&Keyed](const FArcGameEvent& Event) { Keyed.Add(Event); }));

		Fix.ApplyAttributeDelta(Entity, FArcAttributeSet_TestStats::GetHealthAttribute(), -15.f);
		Fix.Drain();
		Fix.ApplyAttributeDelta(Entity, FArcAttributeSet_TestStats::GetHealthAttribute(), -5.f);
		Fix.Drain();

		ASSERT_THAT(AreEqual(2, Keyed.Num()));
		const FArcAttributeChangedPayload* Second = Keyed[1].Payload.GetPtr<FArcAttributeChangedPayload>();
		ASSERT_THAT(IsNotNull(Second));
		ASSERT_THAT(IsNear(85.f, Second->OldValue, 0.001f));
		ASSERT_THAT(IsNear(80.f, Second->NewValue, 0.001f));
	}

	TEST_METHOD(DifferentAttributesAndEntities_AreNotMerged)
	{
		const FMassEntityHandle OtherEntity = Fix.CreateEntity(100.f);
		int32 HealthCalls = 0;
		Fix.Bus->Subscribe(Entity, FArcAttributeSet_TestStats::GetHealthAttribute(),
			FArcGameEventDelegate::CreateLambda([&HealthCalls](const FArcGameEvent&) { ++HealthCalls; }));
		Fix.RecordGlobal(ArcGameEventTags::AttributeChanged.GetTag());

		Fix.ApplyAttributeDelta(Entity, FArcAttributeSet_TestStats::GetHealthAttribute(), -10.f);
		Fix.ApplyAttributeDelta(Entity, FArcAttributeSet_TestStats::GetArmorAttribute(), 4.f);
		Fix.ApplyAttributeDelta(OtherEntity, FArcAttributeSet_TestStats::GetHealthAttribute(), -10.f);
		Fix.Drain();

		ASSERT_THAT(AreEqual(3, Fix.Count(ArcGameEventTags::AttributeChanged.GetTag()), TEXT("two attributes on one entity and one on another stay separate")));
		ASSERT_THAT(AreEqual(1, HealthCalls, TEXT("the keyed subscription hears only its own entity and attribute")));
	}
};

// ============================================================================
// ArcEventBus_ChainOrder - an event published during a drain arrives in the next drain
// ============================================================================

TEST_CLASS(ArcEventBus_ChainOrder, "ArcMassAbilities.EventBus.ChainOrder")
{
	ArcEventBusTestsPrivate::FBusFixture Fix;
	FMassEntityHandle Entity;

	BEFORE_EACH()
	{
		Fix.Init();
		Entity = Fix.CreateEntity();
	}

	TEST_METHOD(TagAddedHandlerApplyingAnEffectThatAddsASecondTag_DeliversTheSecondEventOneDrainLater)
	{
		UArcEffectDefinition* GrantB = NewObject<UArcEffectDefinition>();
		GrantB->StackingPolicy.DurationType = EArcEffectDuration::Infinite;
		FArcEffectComp_GrantTags GrantComponent;
		GrantComponent.GrantedTags.AddTag(TAG_Bus_TagB);
		GrantB->Components.Add(FInstancedStruct::Make(GrantComponent));

		UArcEffectDefinition* Host = NewObject<UArcEffectDefinition>();
		Host->StackingPolicy.DurationType = EArcEffectDuration::Infinite;
		FArcEffectTask_OnTagChanged Task;
		Task.Party = EArcTagChangeParty::Target;
		Task.RequiredTag = TAG_Bus_TagA;
		Task.When = EArcTagChangeTrigger::Added;
		Task.EffectToApply = GrantB;
		Host->Tasks.Add(FInstancedStruct::Make(Task));
		ASSERT_THAT(IsTrue(ArcEffects::TryApplyEffect(*Fix.EntityManager, Entity, Host, FMassEntityHandle()).Handle.IsValid()));
		Fix.Drain();

		Fix.RecordGlobal(ArcGameEventTags::TagAdded.GetTag());
		Fix.AddTag(Entity, TAG_Bus_TagA);

		Fix.Drain();
		ASSERT_THAT(AreEqual(1, Fix.Events.Num(), TEXT("the first drain delivers only the event queued before it")));
		ASSERT_THAT(IsTrue(Fix.Events[0].Payload.Get<FArcTagChangedPayload>().Tag == TAG_Bus_TagA));
		FMassEntityView View(*Fix.EntityManager, Entity);
		ASSERT_THAT(IsTrue(View.GetSparseFragmentDataPtr<FArcOwnedTagsFragment>()->GetTags().HasTagExact(TAG_Bus_TagB),
			TEXT("the handler already ran inside the first drain")));

		Fix.Drain();
		ASSERT_THAT(AreEqual(2, Fix.Events.Num(), TEXT("the second tag's event arrives one drain later")));
		ASSERT_THAT(IsTrue(Fix.Events[1].Payload.Get<FArcTagChangedPayload>().Tag == TAG_Bus_TagB));

		Fix.Drain();
		ASSERT_THAT(AreEqual(2, Fix.Events.Num()));
	}

	TEST_METHOD(SelfFlippingChain_AdvancesOneStepPerDrain_AndEndsOnceTheFlipperIsRemoved)
	{
		const FGameplayTag AddedChannel = ArcGameEventTags::TagAdded.GetTag();
		const FGameplayTag RemovedChannel = ArcGameEventTags::TagRemoved.GetTag();
		Fix.RecordGlobal(AddedChannel);
		Fix.RecordGlobal(RemovedChannel);

		FMassEntityManager* EntityManager = Fix.EntityManager;
		const FMassEntityHandle FlipEntity = Entity;
		const FDelegateHandle OnAdded = Fix.Bus->Subscribe(AddedChannel, Entity, TAG_Bus_TagA,
			FArcGameEventDelegate::CreateLambda([EntityManager, FlipEntity](const FArcGameEvent&)
			{
				ArcAbilities::RemoveOwnedTags(*EntityManager, FlipEntity, FGameplayTagContainer(TAG_Bus_TagA));
			}));
		const FDelegateHandle OnRemoved = Fix.Bus->Subscribe(RemovedChannel, Entity, TAG_Bus_TagA,
			FArcGameEventDelegate::CreateLambda([EntityManager, FlipEntity](const FArcGameEvent&)
			{
				ArcAbilities::AddOwnedTags(*EntityManager, FlipEntity, FGameplayTagContainer(TAG_Bus_TagA));
			}));

		Fix.AddTag(Entity, TAG_Bus_TagA);
		for (int32 Drain = 1; Drain <= 6; ++Drain)
		{
			Fix.Drain();
			ASSERT_THAT(AreEqual(Drain, Fix.Events.Num(), TEXT("each drain delivers exactly one step of the chain")));
		}

		Fix.Bus->Unsubscribe(OnAdded);
		Fix.Bus->Unsubscribe(OnRemoved);
		Fix.Drain();
		ASSERT_THAT(AreEqual(7, Fix.Events.Num(), TEXT("the step queued by the last flip is still delivered")));
		Fix.Drain();
		ASSERT_THAT(AreEqual(7, Fix.Events.Num(), TEXT("with nothing flipping, the chain ends")));
	}
};
