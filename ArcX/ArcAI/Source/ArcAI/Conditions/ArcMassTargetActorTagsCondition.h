#pragma once
#include "GameplayTagContainer.h"
#include "MassStateTreeTypes.h"

#include "ArcMassTargetActorTagsCondition.generated.h"

class AActor;

USTRUCT()
struct FArcMassTargetActorTagsConditionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayTagContainer TagContainer;

	UPROPERTY(EditAnywhere, Category = Parameter)
	TObjectPtr<AActor> TargetInput;
};

USTRUCT(DisplayName="Arc Mass Target Actor Tags Condition")
struct FArcMassTargetActorTagsCondition : public FMassStateTreeConditionBase
{
	GENERATED_BODY()

public:
	using FInstanceDataType = FArcMassTargetActorTagsConditionInstanceData;

	FArcMassTargetActorTagsCondition() = default;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual void GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const override;
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

	UPROPERTY(EditAnywhere, Category = Condition)
	EGameplayContainerMatchType MatchType = EGameplayContainerMatchType::Any;

	/** If true, the tag has to be exactly present, if false then TagContainer will include it's parent tags while matching */
	UPROPERTY(EditAnywhere, Category = Condition)
	bool bExactMatch = false;

	UPROPERTY(EditAnywhere, Category = Condition)
	bool bInvert = false;
};