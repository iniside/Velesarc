#pragma once

#include "GameplayInteractionsTypes.h"
#include "ArcGI_EnhancedInputEventTask.generated.h"

class UInputAction;
struct FEnhancedInputActionValueBinding;

USTRUCT()
struct FArcGI_EnhancedInputEventTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Context")
	TObjectPtr<AActor> Actor = nullptr;
	
	UPROPERTY(EditAnywhere, Category = Parameter)
	FStateTreeDelegateDispatcher OnInputStarted;
	
	TArray<FEnhancedInputActionValueBinding*> Bindings;
};


USTRUCT(meta = (DisplayName = "Enhanced Input Event", Category="Gameplay Interactions"))
struct FArcGI_EnhancedInputEventTask : public FGameplayInteractionStateTreeTask
{
	GENERATED_BODY()
	
	typedef FArcGI_EnhancedInputEventTaskInstanceData FInstanceDataType;

protected:
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	
	UPROPERTY(EditAnywhere, Category = Parameter)
	TArray<TObjectPtr<UInputAction>> InputActions;
};