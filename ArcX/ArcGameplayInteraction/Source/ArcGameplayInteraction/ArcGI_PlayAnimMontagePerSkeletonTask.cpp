#include "ArcGI_PlayAnimMontagePerSkeletonTask.h"

#include "StateTreeExecutionContext.h"

#define LOCTEXT_NAMESPACE "GameplayInteractions"

EStateTreeRunStatus FArcGI_PlayAnimMontagePerSkeletonTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	if (MontageMap.IsEmpty())
	{
		return EStateTreeRunStatus::Failed;
	}

	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	USkeletalMeshComponent* MeshComponent = InstanceData.Actor->FindComponentByClass<USkeletalMeshComponent>();
	
	if (MeshComponent == nullptr)
	{
		return EStateTreeRunStatus::Failed;
	}

	if (!MeshComponent->GetSkeletalMeshAsset())
	{
		return EStateTreeRunStatus::Failed;
	}
	
	if (!MontageMap.Contains(MeshComponent->GetSkeletalMeshAsset()->GetSkeleton()))
	{
		return EStateTreeRunStatus::Failed;
	}
	
	UAnimMontage* Montage = MontageMap.FindRef(MeshComponent->GetSkeletalMeshAsset()->GetSkeleton());
	
	InstanceData.Time = 0.f;

	// Grab the task duration from the montage.
	InstanceData.ComputedDuration = Montage->GetPlayLength();
	InstanceData.CurrentMontage = Montage;
	
	UAnimInstance * AnimInstance = MeshComponent->GetAnimInstance();
	if (AnimInstance)
	{
		float const Duration = AnimInstance->Montage_Play(Montage);
	}
	// @todo: listen anim completed event

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FArcGI_PlayAnimMontagePerSkeletonTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	InstanceData.Time += DeltaTime;
	return InstanceData.ComputedDuration <= 0.0f ? EStateTreeRunStatus::Running : (InstanceData.Time < InstanceData.ComputedDuration ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Succeeded);
}

#if WITH_EDITOR
FText FArcGI_PlayAnimMontagePerSkeletonTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	const FInstanceDataType* InstanceData = InstanceDataView.GetPtr<FInstanceDataType>();
	check(InstanceData);

	// Asset
	const FText MontageValue;//FText::FromString(GetNameSafe(Montage));

	// Actor
	FText ActorValue = BindingLookup.GetBindingSourceDisplayName(FPropertyBindingPath(ID, GET_MEMBER_NAME_CHECKED(FInstanceDataType, Actor)), Formatting);
	if (ActorValue.IsEmpty())
	{
		ActorValue = LOCTEXT("None", "None");
	}

	if (Formatting == EStateTreeNodeFormatting::RichText)
	{
		// Rich
		return FText::Format(LOCTEXT("PlayMontageRich", "<b>Play Montage</> {0} <s>with </>{1}"), MontageValue, ActorValue);
	}
	
	// Plain
	return FText::Format(LOCTEXT("PlayMontage", "Play Montage {0} with {1}"), MontageValue, ActorValue);
}
#endif

#undef LOCTEXT_NAMESPACE
