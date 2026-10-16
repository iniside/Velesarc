// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcAttributeObserverTestTypes.h"
#include "ArcMassAbilitiesTestTypes.h"
#include "Components/ActorTestSpawner.h"
#include "Attributes/ArcAttribute.h"
#include "Attributes/ArcAttributeHandlerConfig.h"
#include "Attributes/ArcInstantModifier.h"
#include "Fragments/ArcPendingAttributeOpsFragment.h"
#include "Fragments/ArcAggregatorFragment.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Fragments/ArcEffectStackFragment.h"
#include "Fragments/ArcAttributeHandlerSharedFragment.h"
#include "Fragments/ArcAttributeChangedFragment.h"
#include "Processors/ArcPendingAttributeOpsProcessor.h"
#include "Processors/ArcAttributeObserverProcessor.h"
#include "Signals/ArcAttributeChangedSignal.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityView.h"
#include "Mass/ExecutionContext.h"
#include "MassSubsystemAccess.h"
#include "Mass/Executor.h"
#include "Mass/ProcessingContext.h"
#include "MassSignalSubsystem.h"
#include "StructUtils/InstancedStruct.h"
#include "StructUtils/SharedStruct.h"

// ---------------------------------------------------------------------------
// File-scoped helpers
// ---------------------------------------------------------------------------
namespace ArcAttributeObserverTestDetail
{
	// Entity with FragA + FragB + all Mass plumbing for EnqueueInstantModifier
	FMassEntityHandle CreateEntityAB(FMassEntityManager& EntityManager, UArcAttributeHandlerConfig* HandlerConfig)
	{
		FArcObsTest_FragA InitA;
		FArcObsTest_FragB InitB;

		TArray<FInstancedStruct> Fragments;
		Fragments.Add(FInstancedStruct::Make(InitA));
		Fragments.Add(FInstancedStruct::Make(InitB));
		Fragments.Add(FInstancedStruct::Make(FArcEffectStackFragment()));
		Fragments.Add(FInstancedStruct::Make(FArcAggregatorFragment()));
		Fragments.Add(FInstancedStruct::Make(FArcOwnedTagsFragment()));
		Fragments.Add(FInstancedStruct::Make(FArcPendingAttributeOpsFragment()));

		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateEntityFromInstances(EntityManager, Fragments);

		FArcAttributeHandlerSharedFragment SharedHandler;
		SharedHandler.Config = HandlerConfig;
		const FConstSharedStruct HandlerSharedFrag = EntityManager.GetOrCreateConstSharedFragment(SharedHandler);
		EntityManager.AddConstSharedFragmentToEntity(Entity, HandlerSharedFrag);

		return Entity;
	}

	// Entity with FragA + FragB + FragC
	FMassEntityHandle CreateEntityABC(FMassEntityManager& EntityManager, UArcAttributeHandlerConfig* HandlerConfig)
	{
		FArcObsTest_FragA InitA;
		FArcObsTest_FragB InitB;
		FArcObsTest_FragC InitC;

		TArray<FInstancedStruct> Fragments;
		Fragments.Add(FInstancedStruct::Make(InitA));
		Fragments.Add(FInstancedStruct::Make(InitB));
		Fragments.Add(FInstancedStruct::Make(InitC));
		Fragments.Add(FInstancedStruct::Make(FArcEffectStackFragment()));
		Fragments.Add(FInstancedStruct::Make(FArcAggregatorFragment()));
		Fragments.Add(FInstancedStruct::Make(FArcOwnedTagsFragment()));
		Fragments.Add(FInstancedStruct::Make(FArcPendingAttributeOpsFragment()));

		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateEntityFromInstances(EntityManager, Fragments);

		FArcAttributeHandlerSharedFragment SharedHandler;
		SharedHandler.Config = HandlerConfig;
		const FConstSharedStruct HandlerSharedFrag = EntityManager.GetOrCreateConstSharedFragment(SharedHandler);
		EntityManager.AddConstSharedFragmentToEntity(Entity, HandlerSharedFrag);

		return Entity;
	}

	// Entity with FragA only (no FragB) — tests absent-observer-fragment path
	FMassEntityHandle CreateEntityAOnly(FMassEntityManager& EntityManager, UArcAttributeHandlerConfig* HandlerConfig)
	{
		FArcObsTest_FragA InitA;

		TArray<FInstancedStruct> Fragments;
		Fragments.Add(FInstancedStruct::Make(InitA));
		Fragments.Add(FInstancedStruct::Make(FArcEffectStackFragment()));
		Fragments.Add(FInstancedStruct::Make(FArcAggregatorFragment()));
		Fragments.Add(FInstancedStruct::Make(FArcOwnedTagsFragment()));
		Fragments.Add(FInstancedStruct::Make(FArcPendingAttributeOpsFragment()));

		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateEntityFromInstances(EntityManager, Fragments);

		FArcAttributeHandlerSharedFragment SharedHandler;
		SharedHandler.Config = HandlerConfig;
		const FConstSharedStruct HandlerSharedFrag = EntityManager.GetOrCreateConstSharedFragment(SharedHandler);
		EntityManager.AddConstSharedFragmentToEntity(Entity, HandlerSharedFrag);

		return Entity;
	}

	FInstancedStruct MakeObserverHandler(const FArcAttributeRef& Observed)
	{
		FArcAttributeHandler_Test_Observer Handler;
		Handler.Inputs.Add(Observed);
		return FInstancedStruct::Make(Handler);
	}

} // namespace ArcAttributeObserverTestDetail

// ---------------------------------------------------------------------------
// Test class
// ---------------------------------------------------------------------------

TEST_CLASS(ArcAttributeObserver, "ArcMassAbilities.Attributes.Observers")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	UMassSignalSubsystem* SignalSubsystem = nullptr;
	UArcPendingAttributeOpsProcessor* PendingProcessor = nullptr;
	UArcAttributeObserverProcessor* ObserverProcessor = nullptr;

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogUObjectGlobals");
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogRHI");
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogTemp");

		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();

		UMassEntitySubsystem* MES = Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
		check(MES);
		EntityManager = &MES->GetMutableEntityManager();

		SignalSubsystem = Spawner.GetWorld().GetSubsystem<UMassSignalSubsystem>();
		check(SignalSubsystem);

		TSharedRef<FMassEntityManager> SharedEM = EntityManager->AsShared();

		PendingProcessor = NewObject<UArcPendingAttributeOpsProcessor>();
		PendingProcessor->CallInitialize(EntityManager->GetOwner(), SharedEM);

		ObserverProcessor = NewObject<UArcAttributeObserverProcessor>();
		ObserverProcessor->CallInitialize(EntityManager->GetOwner(), SharedEM);
	}

	void DrainPendingOps(FMassEntityHandle Entity)
	{
		SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::PendingAttributeOps, Entity);
		FMassProcessingContext ProcessingContext(*EntityManager, 0.f);
		UE::Mass::Executor::Run(*PendingProcessor, ProcessingContext);
	}

	void RunObserverProcessor(FMassEntityHandle Entity)
	{
		SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::AttributeChanged, Entity);
		FMassProcessingContext ProcessingContext(*EntityManager, 0.f);
		UE::Mass::Executor::Run(*ObserverProcessor, ProcessingContext);
	}

	// -----------------------------------------------------------------------
	// Test 1: Observer fires when the observed attribute changes.
	// -----------------------------------------------------------------------
	TEST_METHOD(Observer_FiresOnChangedAttribute)
	{
		// AttrB observes AttrA — handler on AttrB captures the callback
		UArcAttributeHandlerConfig* HandlerConfig = NewObject<UArcAttributeHandlerConfig>();
		HandlerConfig->Handlers.Add(FArcObsTest_FragB::GetAttrBAttribute(), ArcAttributeObserverTestDetail::MakeObserverHandler(FArcObsTest_FragA::GetAttrAAttribute()));
		HandlerConfig->PostLoad(); // builds the reverse index

		FMassEntityHandle Entity = ArcAttributeObserverTestDetail::CreateEntityAB(*EntityManager, HandlerConfig);

		// Verify initial AttrA = 10
		{
			FMassEntityView View(*EntityManager, Entity);
			FArcObsTest_FragA* FragA = View.GetSparseFragmentDataPtr<FArcObsTest_FragA>();
			ASSERT_THAT(IsNotNull(FragA));
			ASSERT_THAT(IsNear(10.f, FragA->AttrA.CurrentValue, 0.001f));
		}

		// Override AttrA to 50 via pending-ops pipeline
		ArcAttributes::EnqueueInstantModifier(
			*EntityManager, Entity,
			FArcObsTest_FragA::GetAttrAAttribute(),
			EArcModifierOp::Override, 50.f,
			EArcAttributeOpType::Execute);

		DrainPendingOps(Entity);   // emits AttributeChanged signal with sparse fragment
		RunObserverProcessor(Entity); // dispatches OnObservedAttributeChanged

		// Read the live handler stored inside HandlerConfig (handlers stored by value in the TMap)
		const FInstancedStruct* LiveHandlerStruct = HandlerConfig->Handlers.Find(FArcObsTest_FragB::GetAttrBAttribute());
		ASSERT_THAT(IsNotNull(LiveHandlerStruct));
		const FArcAttributeHandler_Test_Observer* LiveHandler = LiveHandlerStruct->GetPtr<FArcAttributeHandler_Test_Observer>();
		ASSERT_THAT(IsNotNull(LiveHandler));

		ASSERT_THAT(AreEqual(1, LiveHandler->ObservedChangeCount));
		ASSERT_THAT(IsTrue(LiveHandler->LastChangedRef == FArcObsTest_FragA::GetAttrAAttribute()));
		ASSERT_THAT(IsNear(10.f, LiveHandler->LastOldValue, 0.001f));
		ASSERT_THAT(IsNear(50.f, LiveHandler->LastNewValue, 0.001f));
	}

	// -----------------------------------------------------------------------
	// Test 2: Observer is skipped when the observer's fragment is absent on the entity.
	//         No crash, no fire.
	// -----------------------------------------------------------------------
	TEST_METHOD(Observer_SkippedWhenObserverFragmentAbsent)
	{
		UArcAttributeHandlerConfig* HandlerConfig = NewObject<UArcAttributeHandlerConfig>();
		HandlerConfig->Handlers.Add(FArcObsTest_FragB::GetAttrBAttribute(), ArcAttributeObserverTestDetail::MakeObserverHandler(FArcObsTest_FragA::GetAttrAAttribute()));
		HandlerConfig->PostLoad();

		// Entity only has FragA — no FragB, so the observer fragment is absent
		FMassEntityHandle Entity = ArcAttributeObserverTestDetail::CreateEntityAOnly(*EntityManager, HandlerConfig);

		ArcAttributes::EnqueueInstantModifier(
			*EntityManager, Entity,
			FArcObsTest_FragA::GetAttrAAttribute(),
			EArcModifierOp::Override, 99.f,
			EArcAttributeOpType::Execute);

		DrainPendingOps(Entity);
		RunObserverProcessor(Entity); // must not crash

		const FInstancedStruct* LiveHandlerStruct = HandlerConfig->Handlers.Find(FArcObsTest_FragB::GetAttrBAttribute());
		ASSERT_THAT(IsNotNull(LiveHandlerStruct));
		const FArcAttributeHandler_Test_Observer* LiveHandler = LiveHandlerStruct->GetPtr<FArcAttributeHandler_Test_Observer>();
		ASSERT_THAT(IsNotNull(LiveHandler));

		ASSERT_THAT(AreEqual(0, LiveHandler->ObservedChangeCount));
	}

	// -----------------------------------------------------------------------
	// Test 3: Multiple observers (AttrB and AttrC) both fire when AttrA changes.
	// -----------------------------------------------------------------------
	TEST_METHOD(Observer_MultipleObserversFire)
	{
		UArcAttributeHandlerConfig* HandlerConfig = NewObject<UArcAttributeHandlerConfig>();
		HandlerConfig->Handlers.Add(FArcObsTest_FragB::GetAttrBAttribute(), ArcAttributeObserverTestDetail::MakeObserverHandler(FArcObsTest_FragA::GetAttrAAttribute()));
		HandlerConfig->Handlers.Add(FArcObsTest_FragC::GetAttrCAttribute(), ArcAttributeObserverTestDetail::MakeObserverHandler(FArcObsTest_FragA::GetAttrAAttribute()));

		HandlerConfig->PostLoad();

		FMassEntityHandle Entity = ArcAttributeObserverTestDetail::CreateEntityABC(*EntityManager, HandlerConfig);

		ArcAttributes::EnqueueInstantModifier(
			*EntityManager, Entity,
			FArcObsTest_FragA::GetAttrAAttribute(),
			EArcModifierOp::Override, 77.f,
			EArcAttributeOpType::Execute);

		DrainPendingOps(Entity);
		RunObserverProcessor(Entity);

		const FInstancedStruct* LiveBStruct = HandlerConfig->Handlers.Find(FArcObsTest_FragB::GetAttrBAttribute());
		const FInstancedStruct* LiveCStruct = HandlerConfig->Handlers.Find(FArcObsTest_FragC::GetAttrCAttribute());
		ASSERT_THAT(IsNotNull(LiveBStruct));
		ASSERT_THAT(IsNotNull(LiveCStruct));

		const FArcAttributeHandler_Test_Observer* LiveB = LiveBStruct->GetPtr<FArcAttributeHandler_Test_Observer>();
		const FArcAttributeHandler_Test_Observer* LiveC = LiveCStruct->GetPtr<FArcAttributeHandler_Test_Observer>();
		ASSERT_THAT(IsNotNull(LiveB));
		ASSERT_THAT(IsNotNull(LiveC));

		ASSERT_THAT(AreEqual(1, LiveB->ObservedChangeCount));
		ASSERT_THAT(AreEqual(1, LiveC->ObservedChangeCount));
	}

	// -----------------------------------------------------------------------
	// Test 4: Cycle detection returns false + non-empty error for both
	//         mutual cycles and self-edges; returns true for acyclic graphs.
	// -----------------------------------------------------------------------
	TEST_METHOD(Observer_CycleDetectionFlags)
	{
		// Mutual cycle: AttrA observes AttrB, AttrB observes AttrA
		{
			UArcAttributeHandlerConfig* Config = NewObject<UArcAttributeHandlerConfig>();

			Config->Handlers.Add(FArcObsTest_FragA::GetAttrAAttribute(), ArcAttributeObserverTestDetail::MakeObserverHandler(FArcObsTest_FragB::GetAttrBAttribute()));

			Config->Handlers.Add(FArcObsTest_FragB::GetAttrBAttribute(), ArcAttributeObserverTestDetail::MakeObserverHandler(FArcObsTest_FragA::GetAttrAAttribute()));

			FString Error;
			const bool bValid = Config->ValidateNoCycles(Error);

			ASSERT_THAT(IsFalse(bValid));
			ASSERT_THAT(IsFalse(Error.IsEmpty()));
		}

		// Self-edge: AttrA observes itself
		{
			UArcAttributeHandlerConfig* Config = NewObject<UArcAttributeHandlerConfig>();

			Config->Handlers.Add(FArcObsTest_FragA::GetAttrAAttribute(), ArcAttributeObserverTestDetail::MakeObserverHandler(FArcObsTest_FragA::GetAttrAAttribute()));

			FString Error;
			const bool bValid = Config->ValidateNoCycles(Error);

			ASSERT_THAT(IsFalse(bValid));
			ASSERT_THAT(IsFalse(Error.IsEmpty()));
		}

		// Acyclic: AttrA observes AttrB, AttrC observes AttrB — no cycle
		{
			UArcAttributeHandlerConfig* Config = NewObject<UArcAttributeHandlerConfig>();

			Config->Handlers.Add(FArcObsTest_FragA::GetAttrAAttribute(), ArcAttributeObserverTestDetail::MakeObserverHandler(FArcObsTest_FragB::GetAttrBAttribute()));

			Config->Handlers.Add(FArcObsTest_FragC::GetAttrCAttribute(), ArcAttributeObserverTestDetail::MakeObserverHandler(FArcObsTest_FragB::GetAttrBAttribute()));

			FString Error;
			const bool bValid = Config->ValidateNoCycles(Error);

			ASSERT_THAT(IsTrue(bValid));
		}
	}

	// -----------------------------------------------------------------------
	// Test 5: Observer does NOT fire when the *observed* attribute didn't change.
	//         We change AttrB (the observer), not AttrA (what it watches).
	//         The reverse index has no entry for AttrB, so no callback.
	// -----------------------------------------------------------------------
	TEST_METHOD(Observer_NoFireWhenObservedDidNotChange)
	{
		UArcAttributeHandlerConfig* HandlerConfig = NewObject<UArcAttributeHandlerConfig>();
		HandlerConfig->Handlers.Add(FArcObsTest_FragB::GetAttrBAttribute(), ArcAttributeObserverTestDetail::MakeObserverHandler(FArcObsTest_FragA::GetAttrAAttribute()));
		HandlerConfig->PostLoad();

		FMassEntityHandle Entity = ArcAttributeObserverTestDetail::CreateEntityAB(*EntityManager, HandlerConfig);

		// Change AttrB (the observer attribute), NOT AttrA (the observed attribute).
		// The reverse index has no entry for AttrB, so OnObservedAttributeChanged is never called.
		ArcAttributes::EnqueueInstantModifier(
			*EntityManager, Entity,
			FArcObsTest_FragB::GetAttrBAttribute(),
			EArcModifierOp::Override, 55.f,
			EArcAttributeOpType::Execute);

		DrainPendingOps(Entity);
		RunObserverProcessor(Entity);

		const FInstancedStruct* LiveHandlerStruct = HandlerConfig->Handlers.Find(FArcObsTest_FragB::GetAttrBAttribute());
		ASSERT_THAT(IsNotNull(LiveHandlerStruct));
		const FArcAttributeHandler_Test_Observer* LiveHandler = LiveHandlerStruct->GetPtr<FArcAttributeHandler_Test_Observer>();
		ASSERT_THAT(IsNotNull(LiveHandler));

		ASSERT_THAT(AreEqual(0, LiveHandler->ObservedChangeCount));
	}
};
