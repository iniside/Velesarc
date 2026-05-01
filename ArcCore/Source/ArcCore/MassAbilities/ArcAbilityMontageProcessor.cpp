// Copyright Lukasz Baran. All Rights Reserved.

#include "MassAbilities/ArcAbilityMontageProcessor.h"
#include "MassAbilities/ArcAbilityMontageFragment.h"
#include "Signals/ArcAbilitySignals.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "Animation/AnimInstance.h"

UArcAbilityMontageProcessor::UArcAbilityMontageProcessor()
	: EntityQuery_Conditional(*this)
{
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Standalone | EProcessorExecutionFlags::Server);
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionOrder.ExecuteInGroup = FName(TEXT("Tasks"));
	ExecutionOrder.ExecuteAfter.Add(FName(TEXT("ArcAbilityStateTreeTickProcessor")));
	QueryBasedPruning = EMassQueryBasedPruning::Never;
	bRequiresGameThreadExecution = true;
}

void UArcAbilityMontageProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery_Conditional.AddSparseRequirement<FArcAbilityMontageFragment>(EMassFragmentPresence::All);
	EntityQuery_Conditional.AddSparseRequirement<FArcAbilityMontageTag>(EMassFragmentPresence::All);
}

void UArcAbilityMontageProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);
	SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
}

void UArcAbilityMontageProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
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
			FArcAbilityMontageFragment* MontageFrag = MyContext.GetMutableSparseElement<FArcAbilityMontageFragment>(EntityIt);
			if (!MontageFrag)
			{
				continue;
			}

			MontageFrag->ElapsedTime += WorldDeltaTime;

			if (MontageFrag->bLoop)
			{
				continue;
			}

			UAnimInstance* AnimInst = MontageFrag->AnimInstance.Get();
			bool bCompleted = false;

			if (!AnimInst || !AnimInst->Montage_IsPlaying(MontageFrag->Montage.Get()))
			{
				bCompleted = true;
			}
			else if (MontageFrag->ElapsedTime >= MontageFrag->Duration)
			{
				bCompleted = true;
			}

			if (bCompleted)
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
