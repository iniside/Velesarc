// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Math/Transform.h"

#include "ArcCueBehavior.h"
#include "Behaviors/ArcCueNiagaraTypes.h"

#include "ArcCueBehavior_SocketNiagara.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;

USTRUCT()
struct ARCCUES_API FArcCueBehaviorState_SocketNiagara : public FArcCueBehaviorState
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UNiagaraComponent> Component = nullptr;
};

USTRUCT(BlueprintType, meta=(DisplayName="Socket Niagara"))
struct ARCCUES_API FArcCueBehavior_SocketNiagara : public FArcCueBehavior
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="SocketNiagara")
	TObjectPtr<UNiagaraSystem> NiagaraSystem = nullptr;

	UPROPERTY(EditAnywhere, Category="SocketNiagara")
	FName MeshComponentTag;

	UPROPERTY(EditAnywhere, Category="SocketNiagara")
	FName SocketName;

	UPROPERTY(EditAnywhere, Category="SocketNiagara")
	FTransform LocalOffset = FTransform::Identity;

	UPROPERTY(EditAnywhere, Category="SocketNiagara")
	FArcCueNiagaraControl Control;

	virtual void OnExecute(const FArcCueContext& Context) const override;
	virtual TInstancedStruct<FArcCueBehaviorState> OnApplication(const FArcCueContext& Context) const override;
	virtual void OnEvent(const FArcCueContext& Context, const FArcCueEvent& Event, TInstancedStruct<FArcCueBehaviorState>& State) const override;
	virtual void OnRemoved(const FArcCueContext& Context, FArcCueBehaviorState& State) const override;
};
