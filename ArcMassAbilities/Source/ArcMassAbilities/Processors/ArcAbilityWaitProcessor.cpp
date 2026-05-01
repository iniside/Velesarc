// Copyright Lukasz Baran. All Rights Reserved.

#include "Processors/ArcAbilityWaitProcessor.h"
#include "Fragments/ArcAbilityWaitFragment.h"
#include "Signals/ArcAbilitySignals.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"

UArcAbilityWaitProcessor::UArcAbilityWaitProcessor()
	: EntityQuery_Conditional(*this)
{
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Standalone | EProcessorExecutionFlags::Server);
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionOrder.ExecuteInGroup = FName(TEXT("Tasks"));
	ExecutionOrder.ExecuteAfter.Add(FName(TEXT("ArcAbilityStateTreeTickProcessor")));
	QueryBasedPruning = EMassQueryBasedPruning::Never;
	bRequiresGameThreadExecution = true;
}

void UArcAbilityWaitProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery_Conditional.AddSparseRequirement<FArcAbilityWaitFragment>(EMassFragmentPresence::All);
	EntityQuery_Conditional.AddSparseRequirement<FArcAbilityWaitTag>(EMassFragmentPresence::All);
}

void UArcAbilityWaitProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);
	SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
}

void UArcAbilityWaitProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	if (!SignalSubsystem)
	{
		return;
	}

	TArray<FMassEntityHandle> EntitiesToSignal;

	EntityQuery_Conditional.ForEachEntityChunk(Context, [&EntitiesToSignal](FMassExecutionContext& MyContext)
	{
		const float WorldDeltaTime = MyContext.GetDeltaTimeSeconds();

		for (FMassExecutionContext::FEntityIterator EntityIt = MyContext.CreateSparseEntityIterator(); EntityIt; ++EntityIt)
		{
			FArcAbilityWaitFragment* WaitFragment = MyContext.GetMutableSparseElement<FArcAbilityWaitFragment>(EntityIt);
			if (!WaitFragment)
			{
				continue;
			}

			WaitFragment->ElapsedTime += WorldDeltaTime;

			if (WaitFragment->ElapsedTime >= WaitFragment->Duration)
			{
				EntitiesToSignal.Add(MyContext.GetEntity(EntityIt));
			}
		}
	});

	if (EntitiesToSignal.Num())
	{
		SignalSubsystem->SignalEntities(UE::ArcMassAbilities::Signals::AbilityStateTreeTickRequired, EntitiesToSignal);
	}
}
