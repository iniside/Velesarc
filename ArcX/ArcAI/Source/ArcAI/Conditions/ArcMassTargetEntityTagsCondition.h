#pragma once
#include "GameplayTagContainer.h"
#include "MassEQSBlueprintLibrary.h"
#include "MassStateTreeTypes.h"

#include "ArcMassTargetEntityTagsCondition.generated.h"

USTRUCT()
struct FArcMassTargetEntityTagsConditionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayTagContainer TagContainer;

	UPROPERTY(EditAnywhere, Category = Parameter)
	FMassEnvQueryEntityInfoBlueprintWrapper TargetInput;
};

USTRUCT(DisplayName="Arc Mass Target Entity Tags Condition")
struct FArcMassTargetEntityTagsCondition : public FMassStateTreeConditionBase
{
	GENERATED_BODY()
	
public:
	using FInstanceDataType = FArcMassTargetEntityTagsConditionInstanceData;

	FArcMassTargetEntityTagsCondition() = default;

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