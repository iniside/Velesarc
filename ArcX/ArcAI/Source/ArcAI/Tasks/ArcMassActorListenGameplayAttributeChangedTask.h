// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassActorSubsystem.h"
#include "StateTreeDelegate.h"
#include "AttributeSet.h"
#include "MassStateTreeTypes.h"

#include "ArcMassActorListenGameplayAttributeChangedTask.generated.h"

UENUM()
enum class EArcAttributeBroadcastCondition : uint8
{
	Always,
	NewValueGreater,
	NewValueLess,
	OldValueGreater,
	OldValueLess,
	DifferenceValueGreater,
	DifferenceValueLess,
};

USTRUCT()
struct FArcMassActorListenGameplayAttributeChangedTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayAttribute Attribute;
	
	UPROPERTY(EditAnywhere, Category = Parameter)
	FStateTreeDelegateDispatcher OnAttributeChanged;
	
	UPROPERTY(EditAnywhere, Category = Parameter)
	EArcAttributeBroadcastCondition BroadcastCondition = EArcAttributeBroadcastCondition::Always;
	
	UPROPERTY(EditAnywhere, Category = Parameter)
	float ConditionValue = 0.f;
	
	UPROPERTY(EditAnywhere, Category = Output)
	float NewValue = 0.f;
	
	UPROPERTY(EditAnywhere, Category = Output)
	float OldValue = 0.f;
	
	UPROPERTY(EditAnywhere, Category = Output)
	float Difference = 0.f;
	
	FDelegateHandle OnAttributeChangedDelegateHandle;
};

USTRUCT(meta = (DisplayName = "Arc Mass Actor Listen Gameplay Attribute Changed Task", Category = "Arc|Events"))
struct FArcMassActorListenGameplayAttributeChangedTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()
	
	using FInstanceDataType = FArcMassActorListenGameplayAttributeChangedTaskInstanceData;

public:
	FArcMassActorListenGameplayAttributeChangedTask();
	
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
