// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcAbilityKitTestTypes.h"
#include "ArcScalableFloat.h"
#include "Attributes/ArcAttributeAggregatorProcessor.h"
#include "Attributes/ArcAttributeHandlerConfig.h"
#include "Attributes/ArcInstantModifier.h"
#include "Attributes/ArcResourceAttributeHandlers.h"
#include "Components/ActorTestSpawner.h"
#include "Effects/ArcEffectDefinition.h"
#include "Effects/ArcEffectFunctions.h"
#include "Effects/ArcEffectTypes.h"
#include "Engine/World.h"
#include "Abilities/ArcAbilityFunctions.h"
#include "Events/ArcGameEvent.h"
#include "Events/ArcGameEventSubsystem.h"
#include "Fragments/ArcAggregatorFragment.h"
#include "Fragments/ArcAttributeHandlerSharedFragment.h"
#include "Fragments/ArcEffectStackFragment.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Fragments/ArcPendingAttributeOpsFragment.h"
#include "Mass/EntityBuilder.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "Mass/Executor.h"
#include "Mass/ProcessingContext.h"
#include "MassEntitySubsystem.h"
#include "MassSignalSubsystem.h"
#include "NativeGameplayTags.h"
#include "Processors/ArcPendingAttributeOpsProcessor.h"
#include "Signals/ArcAttributeRecalculateSignal.h"
#include "StructUtils/InstancedStruct.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ResourceHandlerTest_Changed, "Test.AbilityKit.ResourceChanged");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ResourceHandlerTest_SourceOwned, "Test.AbilityKit.SourceOwned");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ResourceHandlerTest_TargetOwned, "Test.AbilityKit.TargetOwned");

namespace ArcResourceAttributeHandlerTestsPrivate
{
	UArcAttributeHandlerConfig* MakeMetaConfig(const FArcAttributeHandler_MetaToResource& Handler)
	{
		UArcAttributeHandlerConfig* Config = NewObject<UArcAttributeHandlerConfig>();
		Config->Handlers.Add(FArcAbilityKitTest_ResourceSet::GetMetaAttribute(), FInstancedStruct::Make(Handler));
		return Config;
	}

	UArcAttributeHandlerConfig* MakeClampConfig(const FArcAttributeHandler_ClampRange& Handler)
	{
		UArcAttributeHandlerConfig* Config = NewObject<UArcAttributeHandlerConfig>();
		Config->Handlers.Add(FArcAbilityKitTest_ResourceSet::GetResourceAttribute(), FInstancedStruct::Make(Handler));
		return Config;
	}

	FArcAttributeHandler_MetaToResource MakeMetaHandler(EArcResourceOp Op, bool bUseMaxAttribute)
	{
		FArcAttributeHandler_MetaToResource Handler;
		Handler.Resource = FArcAbilityKitTest_ResourceSet::GetResourceAttribute();
		if (bUseMaxAttribute)
		{
			Handler.MaxAttribute = FArcAbilityKitTest_ResourceSet::GetMaxResourceAttribute();
		}
		Handler.MinConst = 0.f;
		Handler.Op = Op;
		return Handler;
	}

	FMassEntityHandle CreateEntity(FMassEntityManager& EntityManager, UArcAttributeHandlerConfig* Config, float Resource, float MaxResource)
	{
		FArcAbilityKitTest_ResourceSet Set;
		Set.InitResource(Resource);
		Set.InitMaxResource(MaxResource);
		Set.InitMeta(0.f);

		FArcAttributeHandlerSharedFragment HandlerShared;
		HandlerShared.Config = Config;

		UE::Mass::FEntityBuilder Builder(EntityManager);
		return Builder
			.Add<FArcAbilityKitTest_ResourceSet>(Set)
			.Add<FArcEffectStackFragment>()
			.Add<FArcAggregatorFragment>()
			.Add<FArcOwnedTagsFragment>()
			.Add<FArcPendingAttributeOpsFragment>()
			.Add<FArcAttributeHandlerSharedFragment>(HandlerShared)
			.Commit();
	}

	FMassEntityHandle CreateSourceEntity(FMassEntityManager& EntityManager)
	{
		UE::Mass::FEntityBuilder Builder(EntityManager);
		return Builder
			.Add<FArcEffectStackFragment>()
			.Add<FArcAggregatorFragment>()
			.Add<FArcOwnedTagsFragment>()
			.Commit();
	}

	UArcEffectDefinition* MakeInstantMetaEffect(float Magnitude)
	{
		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Instant;

		FArcEffectModifier Mod;
		Mod.Attribute = FArcAbilityKitTest_ResourceSet::GetMetaAttribute();
		Mod.Operation = EArcModifierOp::Add;
		Mod.Magnitude = FArcScalableFloat(Magnitude);

		FArcEffectExecutionEntry Entry;
		Entry.Modifiers.Add(Mod);
		Effect->Executions.Add(Entry);
		return Effect;
	}

	UArcEffectDefinition* MakeDurationResourceEffect(float Magnitude)
	{
		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Duration;
		Effect->StackingPolicy.Duration = 10.f;

		FArcEffectModifier Mod;
		Mod.Attribute = FArcAbilityKitTest_ResourceSet::GetResourceAttribute();
		Mod.Operation = EArcModifierOp::Add;
		Mod.Magnitude = FArcScalableFloat(Magnitude);
		Effect->Modifiers.Add(Mod);
		return Effect;
	}
}

TEST_CLASS(ArcResourceAttributeHandler_MetaToResource, "ArcAbilityKit.Attributes.MetaToResource")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	UMassSignalSubsystem* SignalSubsystem = nullptr;
	UArcGameEventSubsystem* Bus = nullptr;
	UArcPendingAttributeOpsProcessor* PendingProcessor = nullptr;
	TArray<FArcGameEvent> Received;

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogUObjectGlobals");
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogRHI");
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

	void DrainPendingOps(FMassEntityHandle Entity)
	{
		SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::PendingAttributeOps, Entity);
		FMassProcessingContext ProcessingContext(*EntityManager, 0.f);
		UE::Mass::Executor::Run(*PendingProcessor, ProcessingContext);
	}

	void ApplyMeta(FMassEntityHandle Target, FMassEntityHandle Source, float Magnitude)
	{
		ArcEffects::TryApplyEffect(*EntityManager, Target, ArcResourceAttributeHandlerTestsPrivate::MakeInstantMetaEffect(Magnitude), Source);
		DrainPendingOps(Target);
	}

	void RecordGlobal()
	{
		TArray<FArcGameEvent>* Out = &Received;
		Bus->Subscribe(TAG_ResourceHandlerTest_Changed, FMassEntityHandle(), EArcEventRole::Any,
			FArcGameEventDelegate::CreateLambda([Out](const FArcGameEvent& Event)
			{
				Out->Add(Event);
			}));
	}

	void DrainBus()
	{
		Bus->DrainOnPostActorTick(&Spawner.GetWorld(), LEVELTICK_All, 0.f);
	}

	FArcAbilityKitTest_ResourceSet* GetSet(FMassEntityHandle Entity)
	{
		FMassEntityView View(*EntityManager, Entity);
		return View.GetSparseFragmentDataPtr<FArcAbilityKitTest_ResourceSet>();
	}

	TEST_METHOD(Add_PositiveMeta_AddsToResource)
	{
		using namespace ArcResourceAttributeHandlerTestsPrivate;
		const FMassEntityHandle Entity = CreateEntity(*EntityManager, MakeMetaConfig(MakeMetaHandler(EArcResourceOp::Add, true)), 40.f, 100.f);

		ApplyMeta(Entity, FMassEntityHandle(), 25.f);

		const FArcAbilityKitTest_ResourceSet* Set = GetSet(Entity);
		ASSERT_THAT(IsNear(65.f, Set->Resource.CurrentValue, 0.001f));
		ASSERT_THAT(IsNear(65.f, Set->Resource.BaseValue, 0.001f));
	}

	TEST_METHOD(Subtract_PositiveMeta_SubtractsFromResource)
	{
		using namespace ArcResourceAttributeHandlerTestsPrivate;
		const FMassEntityHandle Entity = CreateEntity(*EntityManager, MakeMetaConfig(MakeMetaHandler(EArcResourceOp::Subtract, true)), 80.f, 100.f);

		ApplyMeta(Entity, FMassEntityHandle(), 30.f);

		const FArcAbilityKitTest_ResourceSet* Set = GetSet(Entity);
		ASSERT_THAT(IsNear(50.f, Set->Resource.CurrentValue, 0.001f));
		ASSERT_THAT(IsNear(50.f, Set->Resource.BaseValue, 0.001f));
	}

	TEST_METHOD(Add_ClampsToMaxAttributeCurrentValue)
	{
		using namespace ArcResourceAttributeHandlerTestsPrivate;
		const FMassEntityHandle Entity = CreateEntity(*EntityManager, MakeMetaConfig(MakeMetaHandler(EArcResourceOp::Add, true)), 50.f, 80.f);

		ApplyMeta(Entity, FMassEntityHandle(), 100.f);

		ASSERT_THAT(IsNear(80.f, GetSet(Entity)->GetResource(), 0.001f));
	}

	TEST_METHOD(Add_MaxConstOnly_ClampsToMaxConst)
	{
		using namespace ArcResourceAttributeHandlerTestsPrivate;
		FArcAttributeHandler_MetaToResource Handler = MakeMetaHandler(EArcResourceOp::Add, false);
		Handler.MaxConst = 60.f;
		const FMassEntityHandle Entity = CreateEntity(*EntityManager, MakeMetaConfig(Handler), 50.f, 1000.f);

		ApplyMeta(Entity, FMassEntityHandle(), 100.f);

		ASSERT_THAT(IsNear(60.f, GetSet(Entity)->GetResource(), 0.001f));
	}

	TEST_METHOD(Add_MaxConstBelowMaxAttribute_UsesSmallerBound)
	{
		using namespace ArcResourceAttributeHandlerTestsPrivate;
		FArcAttributeHandler_MetaToResource Handler = MakeMetaHandler(EArcResourceOp::Add, true);
		Handler.MaxConst = 60.f;
		const FMassEntityHandle Entity = CreateEntity(*EntityManager, MakeMetaConfig(Handler), 50.f, 80.f);

		ApplyMeta(Entity, FMassEntityHandle(), 100.f);

		ASSERT_THAT(IsNear(60.f, GetSet(Entity)->GetResource(), 0.001f));
	}

	TEST_METHOD(Add_MaxAttributeBelowMaxConst_UsesSmallerBound)
	{
		using namespace ArcResourceAttributeHandlerTestsPrivate;
		FArcAttributeHandler_MetaToResource Handler = MakeMetaHandler(EArcResourceOp::Add, true);
		Handler.MaxConst = 90.f;
		const FMassEntityHandle Entity = CreateEntity(*EntityManager, MakeMetaConfig(Handler), 50.f, 70.f);

		ApplyMeta(Entity, FMassEntityHandle(), 100.f);

		ASSERT_THAT(IsNear(70.f, GetSet(Entity)->GetResource(), 0.001f));
	}

	TEST_METHOD(Add_NoMaxAttributeNoMaxConst_HasNoUpperBound)
	{
		using namespace ArcResourceAttributeHandlerTestsPrivate;
		const FMassEntityHandle Entity = CreateEntity(*EntityManager, MakeMetaConfig(MakeMetaHandler(EArcResourceOp::Add, false)), 50.f, 10.f);

		ApplyMeta(Entity, FMassEntityHandle(), 1000.f);

		ASSERT_THAT(IsNear(1050.f, GetSet(Entity)->GetResource(), 0.001f));
	}

	TEST_METHOD(Subtract_ClampsToMinConstFloor)
	{
		using namespace ArcResourceAttributeHandlerTestsPrivate;
		FArcAttributeHandler_MetaToResource Handler = MakeMetaHandler(EArcResourceOp::Subtract, true);
		Handler.MinConst = 10.f;
		const FMassEntityHandle Entity = CreateEntity(*EntityManager, MakeMetaConfig(Handler), 50.f, 100.f);

		ApplyMeta(Entity, FMassEntityHandle(), 500.f);

		ASSERT_THAT(IsNear(10.f, GetSet(Entity)->GetResource(), 0.001f));
	}

	TEST_METHOD(Meta_IsZeroedAfterApply)
	{
		using namespace ArcResourceAttributeHandlerTestsPrivate;
		const FMassEntityHandle Entity = CreateEntity(*EntityManager, MakeMetaConfig(MakeMetaHandler(EArcResourceOp::Add, true)), 40.f, 100.f);

		ApplyMeta(Entity, FMassEntityHandle(), 25.f);
		ApplyMeta(Entity, FMassEntityHandle(), 5.f);

		const FArcAbilityKitTest_ResourceSet* Set = GetSet(Entity);
		ASSERT_THAT(IsNear(70.f, Set->GetResource(), 0.001f));
		ASSERT_THAT(IsNear(0.f, Set->Meta.CurrentValue, 0.001f));
		ASSERT_THAT(IsNear(0.f, Set->Meta.BaseValue, 0.001f));
	}

	TEST_METHOD(Add_NegativeMeta_IsIgnoredAndMetaZeroed)
	{
		using namespace ArcResourceAttributeHandlerTestsPrivate;
		const FMassEntityHandle Entity = CreateEntity(*EntityManager, MakeMetaConfig(MakeMetaHandler(EArcResourceOp::Add, true)), 40.f, 100.f);

		ApplyMeta(Entity, FMassEntityHandle(), -25.f);

		const FArcAbilityKitTest_ResourceSet* Set = GetSet(Entity);
		ASSERT_THAT(IsNear(40.f, Set->GetResource(), 0.001f));
		ASSERT_THAT(IsNear(0.f, Set->Meta.CurrentValue, 0.001f));
		ASSERT_THAT(IsNear(0.f, Set->Meta.BaseValue, 0.001f));
	}

	TEST_METHOD(Subtract_NegativeMeta_IsIgnoredAndMetaZeroed)
	{
		using namespace ArcResourceAttributeHandlerTestsPrivate;
		const FMassEntityHandle Entity = CreateEntity(*EntityManager, MakeMetaConfig(MakeMetaHandler(EArcResourceOp::Subtract, true)), 40.f, 100.f);

		ApplyMeta(Entity, FMassEntityHandle(), -25.f);

		const FArcAbilityKitTest_ResourceSet* Set = GetSet(Entity);
		ASSERT_THAT(IsNear(40.f, Set->GetResource(), 0.001f));
		ASSERT_THAT(IsNear(0.f, Set->Meta.CurrentValue, 0.001f));
		ASSERT_THAT(IsNear(0.f, Set->Meta.BaseValue, 0.001f));
	}

	TEST_METHOD(AddSigned_KeepsSignOfMeta)
	{
		using namespace ArcResourceAttributeHandlerTestsPrivate;
		FArcAttributeHandler_MetaToResource Handler = MakeMetaHandler(EArcResourceOp::AddSigned, true);
		Handler.MinConst = -100.f;
		const FMassEntityHandle Entity = CreateEntity(*EntityManager, MakeMetaConfig(Handler), 40.f, 100.f);

		ApplyMeta(Entity, FMassEntityHandle(), -60.f);
		ASSERT_THAT(IsNear(-20.f, GetSet(Entity)->GetResource(), 0.001f));

		ApplyMeta(Entity, FMassEntityHandle(), 30.f);
		ASSERT_THAT(IsNear(10.f, GetSet(Entity)->GetResource(), 0.001f));
		ASSERT_THAT(IsNear(0.f, GetSet(Entity)->Meta.CurrentValue, 0.001f));
	}

	TEST_METHOD(EventTag_PublishesPayloadToTargetSubscriberNextDrain)
	{
		using namespace ArcResourceAttributeHandlerTestsPrivate;
		FArcAttributeHandler_MetaToResource Handler = MakeMetaHandler(EArcResourceOp::Subtract, true);
		Handler.EventTag = TAG_ResourceHandlerTest_Changed;
		const FMassEntityHandle Entity = CreateEntity(*EntityManager, MakeMetaConfig(Handler), 50.f, 100.f);
		const FMassEntityHandle Source = CreateSourceEntity(*EntityManager);
		ArcAbilities::AddOwnedTags(*EntityManager, Source, FGameplayTagContainer(TAG_ResourceHandlerTest_SourceOwned));
		ArcAbilities::AddOwnedTags(*EntityManager, Entity, FGameplayTagContainer(TAG_ResourceHandlerTest_TargetOwned));

		TArray<FArcGameEvent>* Out = &Received;
		Bus->Subscribe(TAG_ResourceHandlerTest_Changed, Entity, EArcEventRole::Target,
			FArcGameEventDelegate::CreateLambda([Out](const FArcGameEvent& Event)
			{
				Out->Add(Event);
			}));

		ApplyMeta(Entity, Source, 80.f);
		ASSERT_THAT(AreEqual(0, Received.Num()));

		DrainBus();
		ASSERT_THAT(AreEqual(1, Received.Num()));

		const FArcGameEvent& Event = Received[0];
		ASSERT_THAT(IsTrue(Event.Tag == TAG_ResourceHandlerTest_Changed.GetTag()));
		ASSERT_THAT(IsTrue(Event.Instigator == Source));
		ASSERT_THAT(IsTrue(Event.Target == Entity));
		ASSERT_THAT(IsTrue(Event.SourceTags.HasTagExact(TAG_ResourceHandlerTest_SourceOwned), TEXT("the source side carries the instigator's owned tags")));
		ASSERT_THAT(IsTrue(Event.TargetTags.HasTagExact(TAG_ResourceHandlerTest_TargetOwned), TEXT("the target side carries the receiver's owned tags")));

		const FArcResourceChangedPayload* Payload = Event.Payload.GetPtr<FArcResourceChangedPayload>();
		ASSERT_THAT(IsNotNull(Payload));
		ASSERT_THAT(IsTrue(Payload->Resource == FArcAbilityKitTest_ResourceSet::GetResourceAttribute()));
		ASSERT_THAT(IsNear(50.f, Payload->OldValue, 0.001f));
		ASSERT_THAT(IsNear(0.f, Payload->NewValue, 0.001f));
		ASSERT_THAT(IsNear(80.f, Payload->Requested, 0.001f));
	}

	TEST_METHOD(IgnoredMeta_PublishesNoEvent)
	{
		using namespace ArcResourceAttributeHandlerTestsPrivate;
		FArcAttributeHandler_MetaToResource Handler = MakeMetaHandler(EArcResourceOp::Add, true);
		Handler.EventTag = TAG_ResourceHandlerTest_Changed;
		const FMassEntityHandle Entity = CreateEntity(*EntityManager, MakeMetaConfig(Handler), 50.f, 100.f);
		RecordGlobal();

		ApplyMeta(Entity, FMassEntityHandle(), -10.f);
		DrainBus();

		ASSERT_THAT(AreEqual(0, Received.Num()));
	}

	TEST_METHOD(MissingSource_StillAppliesAndPublishesWithUnsetInstigator)
	{
		using namespace ArcResourceAttributeHandlerTestsPrivate;
		FArcAttributeHandler_MetaToResource Handler = MakeMetaHandler(EArcResourceOp::Add, true);
		Handler.EventTag = TAG_ResourceHandlerTest_Changed;
		const FMassEntityHandle Entity = CreateEntity(*EntityManager, MakeMetaConfig(Handler), 50.f, 100.f);
		RecordGlobal();

		ApplyMeta(Entity, FMassEntityHandle(), 20.f);
		DrainBus();

		ASSERT_THAT(IsNear(70.f, GetSet(Entity)->GetResource(), 0.001f));
		ASSERT_THAT(AreEqual(1, Received.Num()));
		ASSERT_THAT(IsFalse(Received[0].Instigator.IsSet()));
		ASSERT_THAT(IsTrue(Received[0].Target == Entity));
	}

	TEST_METHOD(DestroyedSource_StillApplies)
	{
		using namespace ArcResourceAttributeHandlerTestsPrivate;
		FArcAttributeHandler_MetaToResource Handler = MakeMetaHandler(EArcResourceOp::Subtract, true);
		Handler.EventTag = TAG_ResourceHandlerTest_Changed;
		const FMassEntityHandle Entity = CreateEntity(*EntityManager, MakeMetaConfig(Handler), 50.f, 100.f);
		const FMassEntityHandle Source = CreateSourceEntity(*EntityManager);
		RecordGlobal();

		ArcEffects::TryApplyEffect(*EntityManager, Entity, MakeInstantMetaEffect(15.f), Source);
		EntityManager->DestroyEntity(Source);
		DrainPendingOps(Entity);
		DrainBus();

		ASSERT_THAT(IsNear(35.f, GetSet(Entity)->GetResource(), 0.001f));
		ASSERT_THAT(AreEqual(1, Received.Num()));
		ASSERT_THAT(IsTrue(Received[0].Instigator == Source));
		ASSERT_THAT(IsFalse(EntityManager->IsEntityValid(Received[0].Instigator)));
	}
};

TEST_CLASS(ArcResourceAttributeHandler_ClampRange, "ArcAbilityKit.Attributes.ClampRange")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	UMassSignalSubsystem* SignalSubsystem = nullptr;
	UArcPendingAttributeOpsProcessor* PendingProcessor = nullptr;
	UArcAttributeAggregatorProcessor* AggregatorProcessor = nullptr;

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogUObjectGlobals");
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogRHI");
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();

		UMassEntitySubsystem* EntitySubsystem = Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
		check(EntitySubsystem);
		EntityManager = &EntitySubsystem->GetMutableEntityManager();

		SignalSubsystem = Spawner.GetWorld().GetSubsystem<UMassSignalSubsystem>();
		check(SignalSubsystem);

		PendingProcessor = NewObject<UArcPendingAttributeOpsProcessor>();
		PendingProcessor->CallInitialize(EntityManager->GetOwner(), EntityManager->AsShared());

		AggregatorProcessor = NewObject<UArcAttributeAggregatorProcessor>();
		AggregatorProcessor->CallInitialize(EntityManager->GetOwner(), EntityManager->AsShared());
	}

	void ApplyInstant(FMassEntityHandle Entity, float Delta)
	{
		ArcAttributes::EnqueueInstantModifier(*EntityManager, Entity, FArcAbilityKitTest_ResourceSet::GetResourceAttribute(), EArcModifierOp::Add, Delta, EArcAttributeOpType::Execute);
		SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::PendingAttributeOps, Entity);
		FMassProcessingContext ProcessingContext(*EntityManager, 0.f);
		UE::Mass::Executor::Run(*PendingProcessor, ProcessingContext);
	}

	void Recalculate(FMassEntityHandle Entity)
	{
		SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::AttributeRecalculate, Entity);
		FMassProcessingContext ProcessingContext(*EntityManager, 0.f);
		UE::Mass::Executor::Run(*AggregatorProcessor, ProcessingContext);
	}

	float GetResource(FMassEntityHandle Entity)
	{
		FMassEntityView View(*EntityManager, Entity);
		return View.GetSparseFragmentDataPtr<FArcAbilityKitTest_ResourceSet>()->GetResource();
	}

	TEST_METHOD(PreChange_ClampsToMaxAttribute)
	{
		using namespace ArcResourceAttributeHandlerTestsPrivate;
		FArcAttributeHandler_ClampRange Clamp;
		Clamp.MaxAttribute = FArcAbilityKitTest_ResourceSet::GetMaxResourceAttribute();
		const FMassEntityHandle Entity = CreateEntity(*EntityManager, MakeClampConfig(Clamp), 50.f, 80.f);

		ApplyInstant(Entity, 500.f);

		ASSERT_THAT(IsNear(80.f, GetResource(Entity), 0.001f));
	}

	TEST_METHOD(PreChange_ClampsToMinConst)
	{
		using namespace ArcResourceAttributeHandlerTestsPrivate;
		FArcAttributeHandler_ClampRange Clamp;
		Clamp.MaxAttribute = FArcAbilityKitTest_ResourceSet::GetMaxResourceAttribute();
		Clamp.MinConst = 5.f;
		const FMassEntityHandle Entity = CreateEntity(*EntityManager, MakeClampConfig(Clamp), 50.f, 80.f);

		ApplyInstant(Entity, -500.f);

		ASSERT_THAT(IsNear(5.f, GetResource(Entity), 0.001f));
	}

	TEST_METHOD(PreChange_MaxConstBelowMaxAttribute_UsesSmallerBound)
	{
		using namespace ArcResourceAttributeHandlerTestsPrivate;
		FArcAttributeHandler_ClampRange Clamp;
		Clamp.MaxAttribute = FArcAbilityKitTest_ResourceSet::GetMaxResourceAttribute();
		Clamp.MaxConst = 3.f;
		const FMassEntityHandle Entity = CreateEntity(*EntityManager, MakeClampConfig(Clamp), 1.f, 80.f);

		ApplyInstant(Entity, 10.f);

		ASSERT_THAT(IsNear(3.f, GetResource(Entity), 0.001f));
	}

	TEST_METHOD(PreChange_NoUpperBound_OnlyMinApplies)
	{
		using namespace ArcResourceAttributeHandlerTestsPrivate;
		FArcAttributeHandler_ClampRange Clamp;
		const FMassEntityHandle Entity = CreateEntity(*EntityManager, MakeClampConfig(Clamp), 50.f, 10.f);

		ApplyInstant(Entity, 500.f);
		ASSERT_THAT(IsNear(550.f, GetResource(Entity), 0.001f));

		ApplyInstant(Entity, -1000.f);
		ASSERT_THAT(IsNear(0.f, GetResource(Entity), 0.001f));
	}

	TEST_METHOD(AggregatorRecalculate_ClampsBothEnds)
	{
		using namespace ArcResourceAttributeHandlerTestsPrivate;
		FArcAttributeHandler_ClampRange Clamp;
		Clamp.MaxAttribute = FArcAbilityKitTest_ResourceSet::GetMaxResourceAttribute();
		const FMassEntityHandle Entity = CreateEntity(*EntityManager, MakeClampConfig(Clamp), 50.f, 80.f);

		const FArcActiveEffectHandle Raise = ArcEffects::TryApplyEffect(*EntityManager, Entity, MakeDurationResourceEffect(500.f), FMassEntityHandle()).Handle;
		Recalculate(Entity);
		ASSERT_THAT(IsNear(80.f, GetResource(Entity), 0.001f));

		FArcEffectRemovalQuery Query;
		Query.Handle = Raise;
		ArcEffects::RemoveEffects(*EntityManager, Entity, Query);
		ArcEffects::TryApplyEffect(*EntityManager, Entity, MakeDurationResourceEffect(-500.f), FMassEntityHandle());
		Recalculate(Entity);
		ASSERT_THAT(IsNear(0.f, GetResource(Entity), 0.001f));
	}
};
