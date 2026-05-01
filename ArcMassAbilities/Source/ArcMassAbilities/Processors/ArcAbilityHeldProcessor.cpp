// Copyright Lukasz Baran. All Rights Reserved.

#include "Processors/ArcAbilityHeldProcessor.h"
#include "Fragments/ArcAbilityHeldFragment.h"
#include "Fragments/ArcAbilityInputFragment.h"
#include "Signals/ArcAbilitySignals.h"
#include "MassEntityView.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"

UArcAbilityHeldProcessor::UArcAbilityHeldProcessor()
	: EntityQuery_Conditional(*this)
{
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Standalone | EProcessorExecutionFlags::Server);
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionOrder.ExecuteInGroup = FName(TEXT("Tasks"));
	ExecutionOrder.ExecuteAfter.Add(FName(TEXT("ArcAbilityStateTreeTickProcessor")));
	QueryBasedPruning = EMassQueryBasedPruning::Never;
	bRequiresGameThreadExecution = true;
}

void UArcAbilityHeldProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery_Conditional.AddSparseRequirement<FArcAbilityHeldFragment>(EMassFragmentPresence::All);
	EntityQuery_Conditional.AddSparseRequirement<FArcAbilityHeldTag>(EMassFragmentPresence::All);
}

void UArcAbilityHeldProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);
	SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
}

void UArcAbilityHeldProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	if (!SignalSubsystem)
	{
		return;
	}

	TArray<FMassEntityHandle> EntitiesToSignal;

	EntityQuery_Conditional.ForEachEntityChunk(Context, [&EntityManager, &EntitiesToSignal](FMassExecutionContext& MyContext)
	{
		const float WorldDeltaTime = MyContext.GetDeltaTimeSeconds();

		for (FMassExecutionContext::FEntityIterator EntityIt = MyContext.CreateSparseEntityIterator(); EntityIt; ++EntityIt)
		{
			FArcAbilityHeldFragment* HeldFrag = MyContext.GetMutableSparseElement<FArcAbilityHeldFragment>(EntityIt);
			if (!HeldFrag)
			{
				continue;
			}

			FMassEntityHandle Entity = MyContext.GetEntity(EntityIt);
			FMassEntityView EntityView(EntityManager, Entity);
			const FArcAbilityInputFragment* InputFrag = EntityView.GetFragmentDataPtr<FArcAbilityInputFragment>();

			bool bIsHeld = false;
			if (InputFrag && HeldFrag->InputTag.IsValid())
			{
				bIsHeld = InputFrag->HeldInputs.HasTag(HeldFrag->InputTag);
			}

			HeldFrag->bIsHeld = bIsHeld;

			if (bIsHeld)
			{
				HeldFrag->HeldDuration += WorldDeltaTime;
			}

			bool bShouldSignal = false;

			if (bIsHeld != HeldFrag->bWasHeld)
			{
				bShouldSignal = true;
			}

			if (!HeldFrag->bThresholdReached && HeldFrag->MinHeldDuration > 0.f
				&& HeldFrag->HeldDuration >= HeldFrag->MinHeldDuration)
			{
				HeldFrag->bThresholdReached = true;
				bShouldSignal = true;
			}

			if (bShouldSignal)
			{
				EntitiesToSignal.Add(Entity);
			}
		}
	});

	if (EntitiesToSignal.Num())
	{
		SignalSubsystem->SignalEntities(UE::ArcMassAbilities::Signals::AbilityStateTreeTickRequired, EntitiesToSignal);
	}
}
