// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "Mass/ExecutionContext.h"
#include "MassSubsystemAccess.h"
#include "StateTreeExecutionContext.h"
#include "Abilities/ArcAbilityHandle.h"
#include "ArcAbilityExecutionContext.generated.h"

struct FMassEntityManager;
struct FArcAbilityContext;
struct FArcOwnedTagsFragment;
struct FArcEffectStackFragment;
struct FArcAbilityInputFragment;
struct FArcActiveAbility;

USTRUCT()
struct FArcAbilityExecutionExtension : public FStateTreeExecutionExtension
{
	GENERATED_BODY()

	FMassEntityHandle Entity;
	FArcAbilityHandle AbilityHandle;

	virtual void ScheduleNextTick(const FContextParameters& Context, const FNextTickArguments& Args) override;
};

struct ARCMASSABILITIES_API FArcAbilityExecutionContext : public FStateTreeExecutionContext
{
	FArcAbilityExecutionContext(UObject& InOwner,
	                            const UStateTree& InStateTree,
	                            FStateTreeInstanceData& InInstanceData,
	                            FMassExecutionContext& InMassContext);

	FMassEntityManager& GetEntityManager() const;
	FMassExecutionContext& GetMassContext() const { return *MassContext; }

	FMassEntityHandle GetEntity() const { return Entity; }
	void SetEntity(FMassEntityHandle InEntity);

	FArcAbilityHandle GetAbilityHandle() const { return AbilityHandle; }
	void SetAbilityHandle(FArcAbilityHandle InHandle) { AbilityHandle = InHandle; }

	void SetContextRequirements(FMassEntityHandle Entity,
	                            FMassEntityManager& EntityManager,
	                            FArcEffectStackFragment* EffectStack,
	                            FArcAbilityContext& AbilityContext,
	                            FInstancedStruct& SourceData);

	EStateTreeRunStatus Start();

private:
	FMassExecutionContext* MassContext = nullptr;
	FMassEntityHandle Entity;
	FArcAbilityHandle AbilityHandle;
};
