// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcGameplayAbilityExecuteTargetingTask.h"

#include "StateTreeExecutionContext.h"

#include "AbilitySystem/ArcCoreGameplayAbility.h"
#include "AbilitySystem/ArcItemGameplayAbility.h"
#include "AbilitySystem/Targeting/ArcTargetingObject.h"
#include "Targeting/ArcTargetingSourceContext.h"
#include "Items/ArcItemDefinition.h"
#include "TargetingSystem/TargetingSubsystem.h"

EStateTreeRunStatus FArcGameplayAbilityExecuteTargetingTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.bRequestComplete = false;
	InstanceData.bHasResults = false;
	InstanceData.HitResults.Empty();
	InstanceData.TargetDataHandle = FGameplayAbilityTargetDataHandle();

	UArcItemGameplayAbility* ArcAbility = Cast<UArcItemGameplayAbility>(InstanceData.Ability);
	if (!ArcAbility)
	{
		return EStateTreeRunStatus::Failed;
	}

	// Resolve targeting object
	UArcTargetingObject* TargetingObj = InstanceData.TargetingObject;
	if (!TargetingObj)
	{
		const UArcItemDefinition* ItemDef = ArcAbility->NativeGetSourceItemData();
		if (ItemDef)
		{
			if (const auto* Fragment = ItemDef->FindFragment<FArcItemFragment_TargetingObjectPreset>())
			{
				TargetingObj = Fragment->TargetingObject;
			}
		}
	}

	if (!TargetingObj || !TargetingObj->TargetingPreset)
	{
		return EStateTreeRunStatus::Failed;
	}

	UTargetingSubsystem* TargetingSystem = UTargetingSubsystem::Get(ArcAbility->GetWorld());
	if (!TargetingSystem)
	{
		return EStateTreeRunStatus::Failed;
	}

	// Setup source context — same as NativeExecuteLocalTargeting
	FArcTargetingSourceContext SourceContext;
	SourceContext.SourceActor = ArcAbility->GetAvatarActorFromActorInfo();
	SourceContext.InstigatorActor = ArcAbility->GetActorInfo().OwnerActor.Get();
	SourceContext.SourceObject = ArcAbility;

	InstanceData.RequestHandle = Arcx::MakeTargetRequestHandle(TargetingObj->TargetingPreset, SourceContext);

	TargetingSystem->ExecuteTargetingRequestWithHandle(InstanceData.RequestHandle,
		FTargetingRequestDelegate::CreateLambda(
			[WeakContext = Context.MakeWeakExecutionContext()](FTargetingRequestHandle CompletedHandle)
			{
				FStateTreeStrongExecutionContext StrongContext = WeakContext.MakeStrongExecutionContext();
				FInstanceDataType* Data = StrongContext.GetInstanceDataPtr<FInstanceDataType>();
				if (!Data)
				{
					return;
				}

				FTargetingDefaultResultsSet* TargetingResults = FTargetingDefaultResultsSet::Find(CompletedHandle);
				if (TargetingResults)
				{
					Data->HitResults.Reserve(TargetingResults->TargetResults.Num());
					for (const FTargetingDefaultResultData& Result : TargetingResults->TargetResults)
					{
						Data->HitResults.Add(Result.HitResult);
					}

					// Convert to GAS target data handle
					Data->TargetDataHandle = FGameplayAbilityTargetDataHandle();
					for (const FHitResult& Hit : Data->HitResults)
					{
						FGameplayAbilityTargetData_SingleTargetHit* NewData = new FGameplayAbilityTargetData_SingleTargetHit();
						NewData->HitResult = Hit;
						Data->TargetDataHandle.Add(NewData);
					}

					Data->bHasResults = Data->HitResults.Num() > 0;
				}

				Data->bRequestComplete = true;
			}));

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FArcGameplayAbilityExecuteTargetingTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (InstanceData.bRequestComplete)
	{
		return EStateTreeRunStatus::Succeeded;
	}

	return EStateTreeRunStatus::Running;
}

void FArcGameplayAbilityExecuteTargetingTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!InstanceData.bRequestComplete && InstanceData.RequestHandle.IsValid())
	{
		if (UTargetingSubsystem* TargetingSystem = UTargetingSubsystem::Get(Context.GetWorld()))
		{
			TargetingSystem->RemoveAsyncTargetingRequestWithHandle(InstanceData.RequestHandle);
		}
	}
}

#if WITH_EDITOR
FText FArcGameplayAbilityExecuteTargetingTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	if (InstanceDataView.IsValid())
	{
		const FInstanceDataType* InstanceData = InstanceDataView.GetPtr<FInstanceDataType>();
		if (InstanceData && InstanceData->TargetingObject)
		{
			return FText::Format(NSLOCTEXT("ArcCore", "ExecuteTargetingDesc", "Execute Targeting: {0}"), FText::FromString(GetNameSafe(InstanceData->TargetingObject)));
		}
	}
	return NSLOCTEXT("ArcCore", "ExecuteTargetingDescDefault", "Execute Targeting");
}
#endif
