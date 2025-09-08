#pragma once
#include "MassActorSubsystem.h"
#include "MassStateTreeTypes.h"

#include "ArcIsMassActorAvailableCondition.generated.h"

USTRUCT()
struct FArcIsMassActorAvailableConditionInstanceData
{
	GENERATED_BODY()
};

USTRUCT(DisplayName="Is Mass Actor Available")
struct FArcIsMassActorAvailableCondition : public FMassStateTreeConditionBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcIsMassActorAvailableConditionInstanceData;

	FArcIsMassActorAvailableCondition() = default;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual void GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const override;
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
	
	TStateTreeExternalDataHandle<FMassActorFragment> MassActorHandle;
};