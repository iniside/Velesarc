// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Behaviors/ArcCueBehavior_SocketNiagara.h"
#include "Cues/ArcCueStateTreeTaskBase.h"
#include "Mass/EntityHandle.h"

#include "ArcCueStateTreeTask_ItemNiagaraMap.generated.h"

USTRUCT(BlueprintType)
struct ARCABILITYKIT_API FArcCueItemNiagaraMapAttachment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Attachment")
	FGameplayTag EffectTag;

	UPROPERTY(EditAnywhere, Category = "Attachment")
	FName MeshComponentTag;

	UPROPERTY(EditAnywhere, Category = "Attachment")
	FName SocketName;

	UPROPERTY(EditAnywhere, Category = "Attachment")
	FTransform LocalOffset = FTransform::Identity;

	UPROPERTY(EditAnywhere, Category = "Attachment")
	TArray<FArcCueNiagaraParameter> InitialParameters;
};

USTRUCT()
struct FArcCueStateTreeTask_ItemNiagaraMapInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Parameter")
	FMassEntityHandle TargetEntity;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	TArray<FArcCueItemNiagaraMapAttachment> Attachments;

	UPROPERTY()
	TArray<FArcCueBehaviorState_SocketNiagara> BehaviorStates;
};

USTRUCT(DisplayName = "Item Niagara Map", Category = "Arc|Cue")
struct ARCABILITYKIT_API FArcCueStateTreeTask_ItemNiagaraMap : public FArcCueStateTreeTaskBase
{
	GENERATED_BODY()

	FArcCueStateTreeTask_ItemNiagaraMap()
	{
		bShouldStateChangeOnReselect = false;
		bShouldCallTick = false;
#if WITH_EDITORONLY_DATA
		bConsideredForCompletion = false;
		bCanEditConsideredForCompletion = false;
#endif
	}

	using FInstanceDataType = FArcCueStateTreeTask_ItemNiagaraMapInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(
		FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(
		FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;
};
