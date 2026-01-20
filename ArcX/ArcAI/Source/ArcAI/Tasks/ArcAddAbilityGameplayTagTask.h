// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcMassStateTreeRunEnvQueryTask.h"
#include "MassActorSubsystem.h"
#include "UObject/Object.h"
#include "ArcAddAbilityGameplayTagTask.generated.h"

USTRUCT()
struct FArcMassUseGameplayAbilityTagTaskInstanceData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Prameter")
	TObjectPtr<AActor> Actor;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	FGameplayTagContainer TagsToAdd;
};
/**
 * 
 */
USTRUCT(meta = (DisplayName = "Arc Add AbilityGameplay Tag", Category = "AI|Action"))
struct ARCAI_API FArcAddAbilityGameplayTagTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()
public:
	using FInstanceDataType = FArcMassUseGameplayAbilityTagTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	FArcAddAbilityGameplayTagTask();
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual void GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	
	TStateTreeExternalDataHandle<FMassActorFragment> MassActorHandle;
};
