// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcMassStateTreeRunEnvQueryTask.h"
#include "Perception/ArcMassPerception.h"
#include "UObject/Object.h"
#include "ArcMassSightPerceptionTask.generated.h"

USTRUCT()
struct FArcMassSightPerceptionTaskInstanceData
{
	GENERATED_BODY()
		
	UPROPERTY(EditAnywhere, Category = Output)
	TArray<FArcPerceivedEntity> PerceivedEntities;
	
	UPROPERTY(EditAnywhere, Category = Parameter)
	FStateTreeDelegateDispatcher OnResultChanged;
};

USTRUCT(DisplayName="Arc Mass Sight Perception Task", meta = (Category = "Arc|Perception"))
struct FArcMassSightPerceptionTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

public:
	using FInstanceDataType = FArcMassSightPerceptionTaskInstanceData;

	FArcMassSightPerceptionTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

USTRUCT()
struct FArcGetActorFromArrayFunctionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Parameter)
	TArray<TObjectPtr<AActor>> Input;
	
	UPROPERTY(EditAnywhere, Category = Output)
	TObjectPtr<AActor> Output;
};

USTRUCT(DisplayName = "Arc Get Actor From Array", meta = (Category = "Arc|Perception"))
struct FArcGetActorFromArrayPropertyFunction : public FStateTreePropertyFunctionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcGetActorFromArrayFunctionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual void Execute(FStateTreeExecutionContext& Context) const override;
};

USTRUCT(BlueprintType)
struct ARCAI_API FArcSelectedEntity
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FMassEntityHandle Entity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<AActor> Actor;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector LastKnownLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float Distance = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float Strength = 0.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float TimeSinceLastSeen = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	double LastTimeSeen = 0.0f;
		
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bIsBeingPerceived = false;
};

USTRUCT()
struct FArcGetPerceivedEntityFromArrayFunctionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Parameter)
	TArray<FArcPerceivedEntity> Input;
	
	UPROPERTY(EditAnywhere, Category = Output)
	FArcSelectedEntity Output;
};

USTRUCT(DisplayName = "Arc Get Perceived Entity From Array", meta = (Category = "Arc|Perception"))
struct FArcGetPerceivedEntityFromArrayPropertyFunction : public FStateTreePropertyFunctionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcGetPerceivedEntityFromArrayFunctionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual void Execute(FStateTreeExecutionContext& Context) const override;
	
};
USTRUCT()
struct FArcMassPerceptionSelectActorInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Input)
	FArcSelectedEntity PerceivedEntity;
	
	UPROPERTY(EditAnywhere, Category = Output)
	FArcSelectedEntity OutPerceivedEntity;
	
	UPROPERTY(EditAnywhere, Category = Parameter)
	FStateTreeDelegateListener TriggerSearch;
	
	UPROPERTY(EditAnywhere, Category = Parameter)
	FStateTreeDelegateDispatcher OnSelectedActorChanged;
	
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (Categories = "GlobalStore"))
	FGameplayTag DataStoreTag;
};

USTRUCT(DisplayName="Arc Mass Perception Select Actor Task", meta = (Category = "Arc|Perception"))
struct FArcMassPerceptionSelectActorTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

public:
	using FInstanceDataType = FArcMassPerceptionSelectActorInstanceData;

	FArcMassPerceptionSelectActorTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};