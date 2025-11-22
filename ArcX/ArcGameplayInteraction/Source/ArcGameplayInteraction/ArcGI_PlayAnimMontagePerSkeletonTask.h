#pragma once
#include "GameplayInteractionsTypes.h"

#include "ArcGI_PlayAnimMontagePerSkeletonTask.generated.h"

class UAnimMontage;

USTRUCT()
struct FArcGI_PlayAnimMontagePerSkeletonTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Context")
	TObjectPtr<AActor> Actor = nullptr;
	
	UPROPERTY()
	float ComputedDuration = 0.0f;

	/** Accumulated time used to stop task if a montage is set */
	UPROPERTY()
	float Time = 0.f;
	
	UPROPERTY()
	TWeakObjectPtr<UAnimMontage> CurrentMontage;
};


USTRUCT(meta = (DisplayName = "Play Anim Montage Per Skeleton", Category="Gameplay Interactions"))
struct FArcGI_PlayAnimMontagePerSkeletonTask : public FGameplayInteractionStateTreeTask
{
	GENERATED_BODY()
	
	typedef FArcGI_PlayAnimMontagePerSkeletonTaskInstanceData FInstanceDataType;

protected:
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
	virtual FName GetIconName() const override
	{
		return FName("StateTreeEditorStyle|Node.Animation");
	}
	virtual FColor GetIconColor() const override
	{
		return UE::StateTree::Colors::Blue;
	}
#endif
	UPROPERTY(EditAnywhere, Category = Parameter)
	TMap<TObjectPtr<USkeleton>, TObjectPtr<UAnimMontage>> MontageMap;
};