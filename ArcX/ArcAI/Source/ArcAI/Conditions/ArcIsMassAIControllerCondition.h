#pragma once
#include "MassActorSubsystem.h"
#include "MassStateTreeTypes.h"

#include "ArcIsMassAIControllerCondition.generated.h"

USTRUCT()
struct FArcIsMassAIControllerConditionInstanceData
{
	GENERATED_BODY()
};

USTRUCT(DisplayName="Is Mass Actor AI Controller Available")
struct FArcIsMassAIControllerCondition : public FMassStateTreeConditionBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcIsMassAIControllerConditionInstanceData;

	FArcIsMassAIControllerCondition() = default;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual void GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const override;
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
	
	TStateTreeExternalDataHandle<FMassActorFragment> MassActorHandle;
};