#pragma once
#include "MassActorSubsystem.h"
#include "MassStateTreeTypes.h"
#include "SmartObjectTypes.h"

#include "ArcIsMassActorAvailableCondition.generated.h"

USTRUCT()
struct FArcIsMassActorAvailableConditionInstanceData
{
	GENERATED_BODY()
};

USTRUCT(DisplayName="Arc Is Mass Actor Available")
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

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};


USTRUCT()
struct FArcIsSmartObjectHandleValidConditionInstanceData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = Input)
	FSmartObjectHandle SmartObjectHandle;
};

USTRUCT(DisplayName="Arc Is Smart Object Handle Valid")
struct FArcIsSmartObjectHandleValidCondition : public FMassStateTreeConditionBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcIsSmartObjectHandleValidConditionInstanceData;

	FArcIsSmartObjectHandleValidCondition() = default;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};