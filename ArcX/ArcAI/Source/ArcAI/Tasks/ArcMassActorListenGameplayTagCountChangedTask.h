// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcMassStateTreeRunEnvQueryTask.h"
#include "GameplayEffectTypes.h"
#include "MassActorSubsystem.h"
#include "UObject/Object.h"
#include "ArcMassActorListenGameplayTagCountChangedTask.generated.h"

USTRUCT()
struct FArcMassActorListenGameplayTagCountChangedTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayTag Tag;
		
	UPROPERTY(EditAnywhere, Category = Parameter)
	TEnumAsByte<EGameplayTagEventType::Type> EventType = EGameplayTagEventType::NewOrRemoved;
	
	UPROPERTY(EditAnywhere, Category = Output)
	int32 MinimumCount = 0;
	
	UPROPERTY(EditAnywhere, Category = Parameter)
	FStateTreeDelegateDispatcher OnGameplayTagCountChanged;;
	
	UPROPERTY(EditAnywhere, Category = Output)
	int32 NewCount = 0;
	
	FDelegateHandle OnGameplayEffectCountChangedDelegateHandle;
};

USTRUCT(meta = (DisplayName = "Arc Mass Actor Listen Gameplay Tag Count Changed Task", Category = "Arc|Events"))
struct FArcMassActorListenGameplayTagCountChangedTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()
	
	using FInstanceDataType = FArcMassActorListenGameplayTagCountChangedTaskInstanceData;

public:
	FArcMassActorListenGameplayTagCountChangedTask();
	
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
