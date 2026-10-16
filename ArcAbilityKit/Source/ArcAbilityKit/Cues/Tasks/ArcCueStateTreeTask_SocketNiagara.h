// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Behaviors/ArcCueBehavior_SocketNiagara.h"
#include "Cues/ArcCueStateTreeTaskBase.h"
#include "Mass/EntityHandle.h"

#include "ArcCueStateTreeTask_SocketNiagara.generated.h"

class UNiagaraSystem;

USTRUCT()
struct FArcCueStateTreeTask_SocketNiagaraInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Parameter")
	TObjectPtr<UNiagaraSystem> NiagaraSystem = nullptr;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	FMassEntityHandle TargetEntity;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	FName MeshComponentTag;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	FName SocketName;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	FTransform LocalOffset = FTransform::Identity;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	TArray<FArcCueNiagaraParameter> InitialParameters;

	UPROPERTY()
	FArcCueBehaviorState_SocketNiagara BehaviorState;
};

USTRUCT(DisplayName = "Socket Niagara", Category = "Arc|Cue")
struct ARCABILITYKIT_API FArcCueStateTreeTask_SocketNiagara : public FArcCueStateTreeTaskBase
{
	GENERATED_BODY()

	FArcCueStateTreeTask_SocketNiagara()
	{
		bShouldStateChangeOnReselect = false;
		bShouldCallTick = false;
#if WITH_EDITORONLY_DATA
		bConsideredForCompletion = false;
		bCanEditConsideredForCompletion = false;
#endif
	}

	using FInstanceDataType = FArcCueStateTreeTask_SocketNiagaraInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(
		FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(
		FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;
};
