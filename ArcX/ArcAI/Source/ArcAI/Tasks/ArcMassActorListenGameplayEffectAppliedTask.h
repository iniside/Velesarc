// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcMassStateTreeRunEnvQueryTask.h"
#include "MassActorSubsystem.h"
#include "UObject/Object.h"
#include "ArcMassActorListenGameplayEffectAppliedTask.generated.h"

USTRUCT()
struct FArcMassActorListenGameplayEffectAppliedTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayTagContainer RequiredTags;
	
	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayTagContainer DenyTags;
	
	UPROPERTY(EditAnywhere, Category = Parameter)
	FStateTreeDelegateDispatcher OnEffectAppliedToSelf;;
	
	UPROPERTY(EditAnywhere, Category = Output)
	float Duration = 0.f;
	
	FDelegateHandle OnAppliedToSelfDelegateHandle;
};

USTRUCT(meta = (DisplayName = "Arc Mass Actor Listen Gameplay Effect Applied Task", Category = "Arc|Events"))
struct FArcMassActorListenGameplayEffectAppliedTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()
	
	using FInstanceDataType = FArcMassActorListenGameplayEffectAppliedTaskInstanceData;

public:
	FArcMassActorListenGameplayEffectAppliedTask();
	
protected:
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual void GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const override;
	
	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

	TStateTreeExternalDataHandle<FMassActorFragment> MassActorFragment;
};
