#pragma once
#include "GameplayTagContainer.h"
#include "MassActorSubsystem.h"
#include "MassStateTreeTypes.h"

#include "ArcMassActorTagsCondition.generated.h"

USTRUCT()
struct FArcMassActorTagConditionConditionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayTagContainer TagContainer;
};

USTRUCT(DisplayName="Arc Mass Actor Tags Condition")
struct FArcMassActorTagsCondition : public FMassStateTreeConditionBase
{
	GENERATED_BODY()
	
public:
	using FInstanceDataType = FArcMassActorTagConditionConditionInstanceData;

	FArcMassActorTagsCondition() = default;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual void GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const override;
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
	
	TStateTreeExternalDataHandle<FMassActorFragment> MassActorHandle;

	UPROPERTY(EditAnywhere, Category = Condition)
	EGameplayContainerMatchType MatchType = EGameplayContainerMatchType::Any;

	/** If true, the tag has to be exactly present, if false then TagContainer will include it's parent tags while matching */
	UPROPERTY(EditAnywhere, Category = Condition)
	bool bExactMatch = false;

	UPROPERTY(EditAnywhere, Category = Condition)
	bool bInvert = false;
};