// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Math/Transform.h"

#include "ArcCueBehavior.h"
#include "Behaviors/ArcCueNiagaraTypes.h"

#include "ArcCueBehavior_SpawnNiagara.generated.h"

class UNiagaraSystem;
class UNiagaraComponent;

USTRUCT()
struct ARCCUES_API FArcCueBehaviorState_SpawnNiagara : public FArcCueBehaviorState
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UNiagaraComponent> Component = nullptr;
};

USTRUCT(BlueprintType, meta=(DisplayName="Spawn Niagara"))
struct ARCCUES_API FArcCueBehavior_SpawnNiagara : public FArcCueBehavior
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="SpawnNiagara")
	TObjectPtr<UNiagaraSystem> NiagaraSystem = nullptr;

	UPROPERTY(EditAnywhere, Category="SpawnNiagara")
	FTransform LocalOffset = FTransform::Identity;

	UPROPERTY(EditAnywhere, Category="SpawnNiagara")
	FArcCueNiagaraControl Control;

	virtual void OnExecute(const FArcCueContext& Context) const override;
	virtual TInstancedStruct<FArcCueBehaviorState> OnApplication(const FArcCueContext& Context) const override;
	virtual void OnEvent(const FArcCueContext& Context, const FArcCueEvent& Event, TInstancedStruct<FArcCueBehaviorState>& State) const override;
	virtual void OnRemoved(const FArcCueContext& Context, FArcCueBehaviorState& State) const override;
};
