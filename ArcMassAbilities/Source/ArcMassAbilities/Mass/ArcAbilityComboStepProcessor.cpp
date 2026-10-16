// Copyright Lukasz Baran. All Rights Reserved.

#include "Mass/ArcAbilityComboStepProcessor.h"
#include "Mass/ArcMassAbilitiesProcessorGroups.h"
#include "ArcMassAbilitiesLogs.h"
#include "Mass/ArcAbilityComboStepFragment.h"
#include "Fragments/ArcAbilityInputFragment.h"
#include "Signals/ArcAbilitySignals.h"
#include "Mass/EntityView.h"
#include "Mass/ExecutionContext.h"
#include "MassSubsystemAccess.h"
#include "MassSignalSubsystem.h"
#include "Animation/AnimInstance.h"

UArcAbilityComboStepProcessor::UArcAbilityComboStepProcessor()
	: EntityQuery_Conditional(*this)
{
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Standalone | EProcessorExecutionFlags::Server);
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionOrder.ExecuteInGroup = UE::ArcMassAbilities::ProcessorGroups::Tasks;
	ExecutionOrder.ExecuteAfter.Add(FName(TEXT("ArcAbilityStateTreeTickProcessor")));
	QueryBasedPruning = EMassQueryBasedPruning::Never;
	bRequiresGameThreadExecution = true;
}

void UArcAbilityComboStepProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery_Conditional.AddSparseRequirement<FArcAbilityComboStepFragment>(EMassFragmentPresence::All);
	EntityQuery_Conditional.AddSparseRequirement<FArcAbilityComboStepTag>(EMassFragmentPresence::All);
}

void UArcAbilityComboStepProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);
	SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
}

void UArcAbilityComboStepProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
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
			FArcAbilityComboStepFragment* ComboFrag = MyContext.GetMutableSparseElement<FArcAbilityComboStepFragment>(EntityIt);
			if (!ComboFrag)
			{
				continue;
			}

			ComboFrag->ElapsedTime += WorldDeltaTime;

			if (!ComboFrag->bInputReceived
				&& ComboFrag->ElapsedTime >= ComboFrag->ComboWindowStart
				&& ComboFrag->ElapsedTime <= ComboFrag->ComboWindowEnd)
			{
				FMassEntityHandle Entity = MyContext.GetEntity(EntityIt);
				FMassEntityView EntityView(EntityManager, Entity);
				const FArcAbilityInputFragment* InputFrag = EntityView.GetSparseFragmentDataPtr<FArcAbilityInputFragment>();
				if (InputFrag && InputFrag->PressedThisFrame.Contains(ComboFrag->InputTag))
				{
					ComboFrag->bInputReceived = true;
					ARCMA_VLOG(MyContext.GetWorld(), Log,
						TEXT("[Ability] Combo input accepted entity=%s tag=%s elapsed=%.2f window=[%.2f,%.2f]"),
						*Entity.DebugGetDescription(),
						*ComboFrag->InputTag.ToString(),
						ComboFrag->ElapsedTime,
						ComboFrag->ComboWindowStart,
						ComboFrag->ComboWindowEnd);
				}
			}

			bool bCompleted = false;
			if (ComboFrag->bInputReceived)
			{
				bCompleted = true;
			}
			else
			{
				UAnimInstance* AnimInst = ComboFrag->AnimInstance.Get();
				if (!AnimInst || ComboFrag->ElapsedTime >= ComboFrag->MontageDuration)
				{
					bCompleted = true;
				}
			}

			if (bCompleted)
			{
				const FMassEntityHandle CompletedEntity = MyContext.GetEntity(EntityIt);
				ARCMA_VLOG(MyContext.GetWorld(), Log,
					TEXT("[Ability] Combo step resolved entity=%s outcome=%s"),
					*CompletedEntity.DebugGetDescription(),
					ComboFrag->bInputReceived ? TEXT("input") : TEXT("timeout"));
				EntitiesToSignal.Add(CompletedEntity);
			}
		}
	});

	if (EntitiesToSignal.Num())
	{
		SignalSubsystem->SignalEntities(UE::ArcMassAbilities::Signals::AbilityStateTreeTickRequired, EntitiesToSignal);
	}
}
