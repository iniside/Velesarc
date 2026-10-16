// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "ArcMass/NiagaraVisualization/ArcNiagaraVisFragments.h"

#include "ArcCueBehavior.h"
#include "Behaviors/ArcCueNiagaraTypes.h"

#include "ArcCueBehavior_AttachedNiagara.generated.h"

class UNiagaraSystem;

USTRUCT(BlueprintType)
struct ARCCUES_API FArcCueAttachedNiagaraConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="AttachedNiagara")
	TObjectPtr<UNiagaraSystem> NiagaraSystem = nullptr;

	UPROPERTY(EditAnywhere, Category="AttachedNiagara")
	bool bCastShadow = false;

	UPROPERTY(EditAnywhere, Category="AttachedNiagara")
	bool bFixedBounds = false;

	UPROPERTY(EditAnywhere, Category="AttachedNiagara", meta=(EditCondition="bFixedBounds"))
	FBox FixedBounds = FBox(ForceInit);

	UPROPERTY(EditAnywhere, Category="AttachedNiagara")
	FTransform ComponentTransform = FTransform::Identity;
};

USTRUCT()
struct ARCCUES_API FArcCueBehaviorState_AttachedNiagara : public FArcCueBehaviorState
{
	GENERATED_BODY()

	UPROPERTY()
	FMassEntityHandle SpawnedEntity;

	UPROPERTY()
	FArcNiagaraVisParamsFragment Parameters;

	EArcCueNiagaraAction PendingAction = EArcCueNiagaraAction::None;
	bool bParametersPending = false;
	bool bActionPending = false;
};

USTRUCT(BlueprintType, meta=(DisplayName="Attached Niagara"))
struct ARCCUES_API FArcCueBehavior_AttachedNiagara : public FArcCueBehavior
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="AttachedNiagara")
	FArcCueAttachedNiagaraConfig NiagaraConfig;

	UPROPERTY(EditAnywhere, Category="AttachedNiagara")
	FArcCueNiagaraControl Control;

	virtual TInstancedStruct<FArcCueBehaviorState> OnApplication(const FArcCueContext& Context) const override;
	virtual void OnEvent(const FArcCueContext& Context, const FArcCueEvent& Event, TInstancedStruct<FArcCueBehaviorState>& State) const override;
	virtual void OnTick(const FArcCueContext& Context, FArcCueBehaviorState& State, float DeltaTime) const override;
	virtual void OnAttachmentLost(const FArcCueContext& Context, FMassEntityHandle AttachmentEntity, TInstancedStruct<FArcCueBehaviorState>& State) const override;
	virtual void OnRemoved(const FArcCueContext& Context, FArcCueBehaviorState& State) const override;
	virtual bool WantsTick() const override { return true; }

private:
	TInstancedStruct<FArcCueBehaviorState> CreateState(const FArcCueContext& Context, const FArcCueEvent* Event) const;
};
