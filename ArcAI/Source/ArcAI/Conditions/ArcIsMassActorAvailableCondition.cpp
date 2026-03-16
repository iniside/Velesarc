#include "ArcIsMassActorAvailableCondition.h"

#include "MassStateTreeDependency.h"
#include "MassStateTreeExecutionContext.h"
#include "StateTreeLinker.h"

bool FArcIsMassActorAvailableCondition::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(MassActorHandle);
	
	return true;
}

void FArcIsMassActorAvailableCondition::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
	Builder.AddReadOnly<FMassActorFragment>();
}

bool FArcIsMassActorAvailableCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FMassStateTreeExecutionContext& MassStateTreeContext = static_cast<FMassStateTreeExecutionContext&>(Context);
	FMassActorFragment* MassActor = MassStateTreeContext.GetExternalDataPtr(MassActorHandle);
	if (!MassActor)
	{
		return false;
	}

	if (!MassActor->Get())
	{
		return false;
	}

	return true;
}

bool FArcIsSmartObjectHandleValidCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	return InstanceData.SmartObjectHandle.IsValid();
}

#if WITH_EDITOR
FText FArcIsMassActorAvailableCondition::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return NSLOCTEXT("ArcAI", "IsMassActorAvailableDesc", "Mass Actor Available");
}

FText FArcIsSmartObjectHandleValidCondition::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return NSLOCTEXT("ArcAI", "IsSmartObjectHandleValidDesc", "Smart Object Handle Valid");
}
#endif
