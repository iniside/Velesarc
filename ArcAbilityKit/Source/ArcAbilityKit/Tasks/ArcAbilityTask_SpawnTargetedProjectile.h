// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "Abilities/Tasks/ArcAbilityTaskBase.h"
#include "Abilities/Targeting/ArcMassTargetData.h"
#include "StateTreeDelegate.h"
#include "StructUtils/InstancedStruct.h"
#include "ArcAbilityTask_SpawnTargetedProjectile.generated.h"

class UArcMassEntityConfigAsset;

USTRUCT()
struct FArcAbilityTask_SpawnTargetedProjectileInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Parameter")
	TObjectPtr<UArcMassEntityConfigAsset> ProjectileConfig = nullptr;

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (BaseStruct = "/Script/ArcMass.ArcMassStatefulAction", ExcludeBaseStruct))
	TArray<FInstancedStruct> ImpactActions;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	FArcMassTargetDataHandle TargetDataHandle;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bUseOverrideLocation = false;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	FVector OverrideLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	FStateTreeDelegateListener SpawnTrigger;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	TArray<FMassEntityHandle> ProjectileEntities;

	uint32 ActivationSerial = 0;
	bool bIsActive = false;
	bool bTriggerPending = false;
	bool bSpawnQueued = false;
};

USTRUCT(DisplayName = "Spawn Targeted Projectile", Category = "Arc|Ability|Spawning")
struct ARCABILITYKIT_API FArcAbilityTask_SpawnTargetedProjectile : public FArcAbilityTaskBase
{
	GENERATED_BODY()

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FArcAbilityTask_SpawnTargetedProjectileInstanceData::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

private:
	EStateTreeRunStatus QueueSpawn(FStateTreeExecutionContext& Context) const;
};
