// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcConditionEffectsTestHelpers.h"
#include "ArcConditionEffectsSubsystem.h"
#include "ArcConditionFragments.h"
#include "ArcConditionTypes.h"
#include "ArcThermalConditionProcessor.h"
#include "ArcBiologicalConditionProcessor.h"
#include "ArcConditionSaturationAttributes.h"
#include "Processors/ArcPendingAttributeOpsProcessor.h"
#include "Fragments/ArcPendingAttributeOpsFragment.h"
#include "Components/ActorTestSpawner.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "MassEntityView.h"
#include "MassExecutor.h"
#include "MassProcessingContext.h"
#include "MassSignalSubsystem.h"

// ============================================================================
// Reverse path: effect -> attribute -> condition
// ============================================================================

TEST_CLASS(ArcCondition_ReversePath, "ArcConditionEffects.Integration.ReversePath")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	UMassSignalSubsystem* SignalSubsystem = nullptr;
	UArcConditionEffectsSubsystem* Subsystem = nullptr;
	UArcPendingAttributeOpsProcessor* PendingProcessor = nullptr;
	UArcThermalConditionProcessor* ThermalProcessor = nullptr;
	UArcBiologicalConditionProcessor* BiologicalProcessor = nullptr;

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

		Subsystem = Spawner.GetWorld().GetSubsystem<UArcConditionEffectsSubsystem>();
		check(Subsystem);

		TSharedRef<FMassEntityManager> SharedEM = EntityManager->AsShared();

		PendingProcessor = NewObject<UArcPendingAttributeOpsProcessor>();
		PendingProcessor->CallInitialize(EntityManager->GetOwner(), SharedEM);

		ThermalProcessor = NewObject<UArcThermalConditionProcessor>();
		ThermalProcessor->CallInitialize(EntityManager->GetOwner(), SharedEM);

		BiologicalProcessor = NewObject<UArcBiologicalConditionProcessor>();
		BiologicalProcessor->CallInitialize(EntityManager->GetOwner(), SharedEM);
	}

	void DrainPendingOps(FMassEntityHandle Entity)
	{
		SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::PendingAttributeOps, Entity);
		FMassProcessingContext ProcessingContext(*EntityManager, 0.f);
		UE::Mass::Executor::Run(*PendingProcessor, ProcessingContext);
	}

	void ExecuteProcessor(UMassProcessor& Processor)
	{
		FMassProcessingContext ProcessingContext(*EntityManager, 0.f);
		UE::Mass::Executor::Run(Processor, ProcessingContext);
	}

	TEST_METHOD(EffectAddsBurningSaturation_IncreasesCondition)
	{
		FMassEntityHandle Entity = ArcConditionTestHelpers::CreateFullConditionEntity(*EntityManager);

		{
			FMassEntityView View(*EntityManager, Entity);
			FArcPendingAttributeOpsFragment* PendingOps = View.GetFragmentDataPtr<FArcPendingAttributeOpsFragment>();
			check(PendingOps);

			FArcPendingAttributeOp Op;
			Op.Attribute = FArcConditionSaturationAttributes::GetBurningSaturationAttribute();
			Op.Operation = EArcModifierOp::Add;
			Op.Magnitude = 30.f;
			PendingOps->PendingOps.Add(Op);
		}

		DrainPendingOps(Entity);
		ExecuteProcessor(*ThermalProcessor);

		FMassEntityView View(*EntityManager, Entity);
		FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
		ASSERT_THAT(IsNear(30.f, Frag->States[(int32)EArcConditionType::Burning].Saturation, 0.5f));
	}

	TEST_METHOD(EffectAddsBleedingSaturation_IncreasesCondition)
	{
		FMassEntityHandle Entity = ArcConditionTestHelpers::CreateFullConditionEntity(*EntityManager);

		{
			FMassEntityView View(*EntityManager, Entity);
			FArcPendingAttributeOpsFragment* PendingOps = View.GetFragmentDataPtr<FArcPendingAttributeOpsFragment>();
			check(PendingOps);

			FArcPendingAttributeOp Op;
			Op.Attribute = FArcConditionSaturationAttributes::GetBleedingSaturationAttribute();
			Op.Operation = EArcModifierOp::Add;
			Op.Magnitude = 40.f;
			PendingOps->PendingOps.Add(Op);
		}

		DrainPendingOps(Entity);
		ExecuteProcessor(*BiologicalProcessor);

		FMassEntityView View(*EntityManager, Entity);
		FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
		ASSERT_THAT(IsNear(40.f, Frag->States[(int32)EArcConditionType::Bleeding].Saturation, 0.5f));
	}

	TEST_METHOD(NegativeDelta_ReducesBiologicalCondition)
	{
		FMassEntityHandle Entity = ArcConditionTestHelpers::CreateFullConditionEntity(*EntityManager);

		{
			FMassEntityView View(*EntityManager, Entity);
			FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
			Frag->States[(int32)EArcConditionType::Bleeding].Saturation = 50.f;
			Frag->States[(int32)EArcConditionType::Bleeding].bActive = true;
		}

		{
			FMassEntityView View(*EntityManager, Entity);
			FArcPendingAttributeOpsFragment* PendingOps = View.GetFragmentDataPtr<FArcPendingAttributeOpsFragment>();
			check(PendingOps);

			FArcPendingAttributeOp Op;
			Op.Attribute = FArcConditionSaturationAttributes::GetBleedingSaturationAttribute();
			Op.Operation = EArcModifierOp::Add;
			Op.Magnitude = -20.f;
			PendingOps->PendingOps.Add(Op);
		}

		DrainPendingOps(Entity);
		ExecuteProcessor(*BiologicalProcessor);

		FMassEntityView View(*EntityManager, Entity);
		FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
		ASSERT_THAT(IsTrue(Frag->States[(int32)EArcConditionType::Bleeding].Saturation < 50.f));
	}

	TEST_METHOD(ZeroDelta_NoChange)
	{
		FMassEntityHandle Entity = ArcConditionTestHelpers::CreateFullConditionEntity(*EntityManager);

		{
			FMassEntityView View(*EntityManager, Entity);
			FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
			Frag->States[(int32)EArcConditionType::Burning].Saturation = 25.f;
			Frag->States[(int32)EArcConditionType::Burning].bActive = true;
		}

		{
			FMassEntityView View(*EntityManager, Entity);
			FArcPendingAttributeOpsFragment* PendingOps = View.GetFragmentDataPtr<FArcPendingAttributeOpsFragment>();
			check(PendingOps);

			FArcPendingAttributeOp Op;
			Op.Attribute = FArcConditionSaturationAttributes::GetBurningSaturationAttribute();
			Op.Operation = EArcModifierOp::Add;
			Op.Magnitude = 0.f;
			PendingOps->PendingOps.Add(Op);
		}

		DrainPendingOps(Entity);
		ExecuteProcessor(*ThermalProcessor);

		FMassEntityView View(*EntityManager, Entity);
		FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
		ASSERT_THAT(IsNear(25.f, Frag->States[(int32)EArcConditionType::Burning].Saturation, 0.5f));
	}
};
