#pragma once
#include "AITypes.h"
#include "AttributeSet.h"
#include "MassActorSubsystem.h"
#include "MassStateTreeTypes.h"

#include "ArcMassActorGameplayAttributeCondition.generated.h"

USTRUCT()
struct FArcMassActorGameplayAttributeConditionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayAttribute Attribute;

	UPROPERTY(EditAnywhere, Category = Parameter)
	float Value = 0.f;
};

USTRUCT(DisplayName="Arc Mass Actor Gameplay Attribute Condition")
struct FArcMassActorGameplayAttributeCondition : public FMassStateTreeConditionBase
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, Category = "Condition", meta = (InvalidEnumValues = "IsTrue"))
	EGenericAICheck Operator = EGenericAICheck::Equal;
	
	using FInstanceDataType = FArcMassActorGameplayAttributeConditionInstanceData;

	FArcMassActorGameplayAttributeCondition() = default;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual void GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const override;
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
	
	TStateTreeExternalDataHandle<FMassActorFragment> MassActorHandle;
};
